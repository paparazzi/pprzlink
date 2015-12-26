/*
 * Copyright (C) 2006  Pascal Brisset, Antoine Drouin
 * Copyright (C) 2014-2015  Gautier Hattenberger <gautier.hattenberger@enac.fr>
 *
 * This file is part of paparazzi.
 *
 * paparazzi is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * paparazzi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with paparazzi; see the file COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

/**
 * @file pprzlink/xbee_transport.c
 * Maxstream XBee serial input and output
 */

#include "pprzlink/xbee_transport.h"
#include "pprzlink/print_utils.h"

/** Ground station address */
#define GROUND_STATION_ADDR 0x100

/** Constants for the API protocol */
#define TX_OPTIONS 0x00
#define NO_FRAME_ID 0
#define XBEE_API_OVERHEAD 5 /* start + len_msb + len_lsb + API_id + checksum */

#define AT_COMMAND_SEQUENCE "+++"
#define AT_SET_MY "ATMY"
#define AT_AP_MODE "ATAP1\r"
#define AT_EXIT "ATCN\r"

/** XBEE 2.4 specific parameters */
#define XBEE_24_TX_ID 0x01 /* 16 bits address */
#define XBEE_24_RX_ID 0x81 /* 16 bits address */
#define XBEE_24_RFDATA_OFFSET 5
#define XBEE_24_TX_OVERHEAD 4
#define XBEE_24_TX_HEADER { \
    XBEE_24_TX_ID, \
    NO_FRAME_ID, \
    (GROUND_STATION_ADDR >> 8), \
    (GROUND_STATION_ADDR & 0xff), \
    TX_OPTIONS \
  }

/** XBEE 868 specific parameters */
#define XBEE_868_TX_ID 0x10
#define XBEE_868_RX_ID 0x90
#define XBEE_868_RFDATA_OFFSET 12
#define XBEE_868_TX_OVERHEAD 13
#define XBEE_868_TX_HEADER { \
    XBEE_868_TX_ID, \
    NO_FRAME_ID, \
    0x00, \
    0x00, \
    0x00, \
    0x00, \
    0x00, \
    0x00, \
    (GROUND_STATION_ADDR >> 8), \
    (GROUND_STATION_ADDR & 0xff), \
    0xff, \
    0xfe, \
    0x00, \
    TX_OPTIONS \
  }


/** Start byte */
#define XBEE_START 0x7e

/** Status of the API packet receiver automata */
#define XBEE_UNINIT         0
#define XBEE_GOT_START      1
#define XBEE_GOT_LENGTH_MSB 2
#define XBEE_GOT_LENGTH_LSB 3
#define XBEE_GOT_PAYLOAD    4

/** Xbee protocol implementation */

static void put_1byte(struct xbee_transport *trans, struct link_device *dev, const uint8_t byte)
{
  trans->cs_tx += byte;
  dev->put_byte(dev->periph, byte);
}

static void put_bytes(struct xbee_transport *trans, struct link_device *dev,
                      enum TransportDataType type __attribute__((unused)), enum TransportDataFormat format __attribute__((unused)),
                      uint8_t len, const void *bytes)
{
  const uint8_t *b = (const uint8_t *) bytes;
  int i;
  for (i = 0; i < len; i++) {
    put_1byte(trans, dev, b[i]);
  }
}

static void put_named_byte(struct xbee_transport *trans, struct link_device *dev,
                           enum TransportDataType type __attribute__((unused)), enum TransportDataFormat format __attribute__((unused)),
                           uint8_t byte, const char *name __attribute__((unused)))
{
  put_1byte(trans, dev, byte);
}

static uint8_t size_of(struct xbee_transport *trans, uint8_t len)
{
  // message length: payload + API overhead + XBEE TX overhead (868 or 2.4)
  if (trans->type == XBEE_24) {
    return len + XBEE_API_OVERHEAD + XBEE_24_TX_OVERHEAD;
  } else {
    return len + XBEE_API_OVERHEAD + XBEE_868_TX_OVERHEAD;
  }
}

static void start_message(struct xbee_transport *trans, struct link_device *dev, uint8_t payload_len)
{
  dev->nb_msgs++;
  dev->put_byte(dev->periph, XBEE_START);
  const uint16_t len = payload_len + XBEE_API_OVERHEAD;
  dev->put_byte(dev->periph, (len >> 8));
  dev->put_byte(dev->periph, (len & 0xff));
  trans->cs_tx = 0;
  if (trans->type == XBEE_24) {
    const uint8_t header[] = XBEE_24_TX_HEADER;
    put_bytes(trans, dev, DL_TYPE_UINT8, DL_FORMAT_SCALAR, XBEE_24_TX_OVERHEAD + 1, header);
  } else {
    const uint8_t header[] = XBEE_868_TX_HEADER;
    put_bytes(trans, dev, DL_TYPE_UINT8, DL_FORMAT_SCALAR, XBEE_868_TX_OVERHEAD + 1, header);
  }
}

static void end_message(struct xbee_transport *trans, struct link_device *dev)
{
  trans->cs_tx = 0xff - trans->cs_tx;
  dev->put_byte(dev->periph, trans->cs_tx);
  dev->send_message(dev->periph);
}

static void overrun(struct xbee_transport *trans __attribute__((unused)),
                    struct link_device *dev __attribute__((unused)))
{
  dev->nb_ovrn++;
}

static void count_bytes(struct xbee_transport *trans __attribute__((unused)),
                        struct link_device *dev __attribute__((unused)), uint8_t bytes)
{
  dev->nb_bytes += bytes;
}

static int check_available_space(struct xbee_transport *trans __attribute__((unused)), struct link_device *dev,
                                 uint8_t bytes)
{
  return dev->check_free_space(dev->periph, bytes);
}

static uint8_t xbee_text_reply_is_ok(struct link_device *dev)
{
  char c[2];
  int count = 0;

  while (dev->char_available(dev->periph)) {
    char cc = dev->get_byte(dev->periph);
    if (count < 2) {
      c[count] = cc;
    }
    count++;
  }

  if ((count > 2) && (c[0] == 'O') && (c[1] == 'K')) {
    return TRUE;
  }

  return FALSE;
}

static uint8_t xbee_try_to_enter_api(struct link_device *dev, void (*wait)(uint32_t))
{

  /** Switching to AT mode (FIXME: busy waiting) */
  print_string(dev, AT_COMMAND_SEQUENCE);

  /** - busy wait 1.25s */
  if (wait != NULL) {
    wait(1250000);
  }
  // TODO else do something ? should not append

  return xbee_text_reply_is_ok(dev);
}

#if XBEE_BAUD == B9600
#undef XBEE_BAUD
#define XBEE_BAUD 9600
#define XBEE_BAUD_ALTERNATE 57600
#define XBEE_ATBD_CODE "ATBD3\rATWR\r"
#pragma message "Experimental: XBEE-API@9k6 auto-baudrate 57k6 -> 9k6 (stop ground link for correct operation)"
#elif XBEE_BAUD == B57600
#undef XBEE_BAUD
#define XBEE_BAUD 57600
#define XBEE_BAUD_ALTERNATE 9600
#define XBEE_ATBD_CODE "ATBD6\rATWR\r"
#pragma message "Experimental: XBEE-API@57k6 auto-baudrate 9k6 -> 57k6 (stop ground link for correct operation)"
#else
#pragma message "XBEE-API Non default baudrate: auto-baud disabled."
#endif

// Init function
void xbee_transport_init(struct xbee_transport *t, struct link_device *dev, uint16_t addr, enum XBeeType type, void (*wait)(uint32_t), char *xbee_init)
{
  t->status = XBEE_UNINIT;
  t->type = type;
  t->rssi = 0;
  t->trans_rx.msg_received = FALSE;
  t->trans_tx.size_of = (size_of_t) size_of;
  t->trans_tx.check_available_space = (check_available_space_t) check_available_space;
  t->trans_tx.put_bytes = (put_bytes_t) put_bytes;
  t->trans_tx.put_named_byte = (put_named_byte_t) put_named_byte;
  t->trans_tx.start_message = (start_message_t) start_message;
  t->trans_tx.end_message = (end_message_t) end_message;
  t->trans_tx.overrun = (overrun_t) overrun;
  t->trans_tx.count_bytes = (count_bytes_t) count_bytes;
  t->trans_tx.impl = (void *)(t);

  // Empty buffer before init process
  while (dev->char_available(dev->periph)) {
    dev->get_byte(dev->periph);
  }

  /** - busy wait 1.25s
   * Mandatory to configure dynamically the xbee module
   * if no wait function are provided, skipping this
   * and assuming static configuration
   */
  if (wait != NULL) {
    wait(1250000);

    if (! xbee_try_to_enter_api(dev, wait)) {
#if defined XBEE_BAUD && defined XBEE_BAUD_ALTERNATE
      // Badly configured... try the alternate baudrate:
      dev->set_baudrate(dev, XBEE_BAUD_ALTERNATE); // FIXME add set_baudrate to generic device
      if (xbee_try_to_enter_api(dev, wait)) {
        // The alternate baudrate worked,
        print_string(dev, XBEE_ATBD_CODE);
      } else {
        // Complete failure, none of the 2 baudrates result in any reply
        // TODO: set LED?

        // Set the default baudrate, just in case everything is right
        dev->set_baudrate(dev, XBEE_BAUD); // FIXME add set_baudrate to generic device
        print_string(dev, "\r");
      }

      // Continue changing settings until the EXIT is issued.
#endif
    }

    /** Setting my address */
    print_string(dev, AT_SET_MY);
    print_hex16(dev, addr);
    print_string(dev, "\r");

    print_string(dev, AT_AP_MODE);

    // Extra configuration AT commands
    if (xbee_init != NULL) {
      print_string(dev, xbee_init);
    }

    /** Switching back to normal mode */
    print_string(dev, AT_EXIT);

#ifdef XBEE_BAUD
    dev->set_baudrate(dev, XBEE_BAUD); // FIXME add set_baudrate to generic device
#endif

  }
}

/** Parsing a XBee API frame */
static inline void parse_xbee(struct xbee_transport *t, uint8_t c)
{
  switch (t->status) {
    case XBEE_UNINIT:
      if (c == XBEE_START) {
        t->status++;
      }
      break;
    case XBEE_GOT_START:
      if (t->trans_rx.msg_received) {
        t->trans_rx.ovrn++;
        goto error;
      }
      t->trans_rx.payload_len = c << 8;
      t->status++;
      break;
    case XBEE_GOT_LENGTH_MSB:
      t->trans_rx.payload_len |= c;
      t->status++;
      t->payload_idx = 0;
      t->cs_rx = 0;
      break;
    case XBEE_GOT_LENGTH_LSB:
      t->trans_rx.payload[t->payload_idx] = c;
      t->cs_rx += c;
      t->payload_idx++;
      if (t->payload_idx == t->trans_rx.payload_len) {
        t->status++;
      }
      break;
    case XBEE_GOT_PAYLOAD:
      if (c + t->cs_rx != 0xff) {
        goto error;
      }
      t->trans_rx.msg_received = TRUE;
      goto restart;
      break;
    default:
      goto error;
  }
  return;
error:
  t->trans_rx.error++;
restart:
  t->status = XBEE_UNINIT;
  return;
}

/** Parsing a frame data and copy the payload to the datalink buffer */
void xbee_check_and_parse(struct link_device *dev, struct xbee_transport *trans, uint8_t *buf, uint8_t *msg_available)
{
  uint8_t i;
  if (dev->char_available(dev->periph)) {
    while (dev->char_available(dev->periph) && !trans->trans_rx.msg_received) {
      parse_xbee(trans, dev->get_byte(dev->periph));
    }
    if (trans->trans_rx.msg_received) {
      if (trans->type == XBEE_24) {
        switch (trans->trans_rx.payload[0]) {
          case XBEE_24_RX_ID:
          case XBEE_24_TX_ID: /* Useful if A/C is connected to the PC with a cable */
            trans->rssi = trans->trans_rx.payload[3];
            for (i = XBEE_24_RFDATA_OFFSET; i < trans->trans_rx.payload_len; i++) {
              buf[i - XBEE_24_RFDATA_OFFSET] = trans->trans_rx.payload[i];
            }
            *msg_available = TRUE;
            break;
          default:
            break;
        }
        trans->trans_rx.msg_received = FALSE;
      }
      else if (trans->type == XBEE_868) {
        switch (trans->trans_rx.payload[0]) {
          case XBEE_868_RX_ID:
          case XBEE_868_TX_ID: /* Useful if A/C is connected to the PC with a cable */
            for (i = XBEE_868_RFDATA_OFFSET; i < trans->trans_rx.payload_len; i++) {
              buf[i - XBEE_868_RFDATA_OFFSET] = trans->trans_rx.payload[i];
            }
            *msg_available = TRUE;
            break;
          default:
            break;
        }
        trans->trans_rx.msg_received = FALSE;
      }
    }
  }
}

