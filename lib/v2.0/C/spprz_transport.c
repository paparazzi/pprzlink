/*
 * Copyright (C) 2006  Pascal Brisset, Antoine Drouin
 * Copyright (C) 2014-2017  Gautier Hattenberger <gautier.hattenberger@enac.fr>
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
 * @file pprzlink/pprz_transport.c
 *
 * Building and parsing Paparazzi frames.
 *
 * Pprz frame:
 *
 * |STX|length|... payload=(length-4) bytes ...|Checksum A|Checksum B|
 *
 * where checksum is computed over length and payload:
 * @code
 * ck_A = ck_B = length
 * for each byte b in payload
 *     ck_A += b;
 *     ck_b += ck_A;
 * @endcode
 */

#include <inttypes.h>
#include "pprzlink/pprz_transport.h"

// PPRZ parsing state machine
#define UNINIT      0
#define GOT_STX     1
#define GOT_LENGTH  2
#define GOT_PAYLOAD 3
#define GOT_CRC1    4

static struct spprz_transport * get_pprz_trans(struct pprzlink_msg *msg)
{
  return (struct spprz_transport *)(msg->trans->impl);
}

static void accumulate_checksum(struct spprz_transport *trans, const uint8_t byte)
{
  trans->ck_a_tx += byte;
  trans->ck_b_tx += trans->ck_a_tx;
}

static inline void insert_byte(struct spprz_transport *trans, const uint8_t byte) {
  trans->tx_msg[trans->tx_msg_idx] = byte;
  trans->tx_msg_idx++;
}

static void put_bytes(struct pprzlink_msg *msg,
                      long fd __attribute__((unused)),
                      enum TransportDataType type __attribute__((unused)),
                      enum TransportDataFormat format __attribute__((unused)),
                      const void *bytes, uint16_t len)
{
  const uint8_t *b = (const uint8_t *) bytes;
  int i;
  for (i = 0; i < len; i++) {
    insert_byte(get_pprz_trans(msg), b[i]);
  }
}

static void put_named_byte(struct pprzlink_msg *msg,
                           long fd __attribute__((unused)),
                           enum TransportDataType type __attribute__((unused)),
                           enum TransportDataFormat format __attribute__((unused)),
                           uint8_t byte, const char *name __attribute__((unused)))
{
  insert_byte(get_pprz_trans(msg), byte);
}

static uint8_t size_of(struct pprzlink_msg *msg __attribute__((unused)), uint8_t len)
{
  // message length: payload + protocol overhead (STX + len + ck_a + ck_b = 4)
  return len + 4;
}

static void start_message(struct pprzlink_msg *msg, long fd, uint8_t payload_len)
{
  PPRZ_MUTEX_LOCK(get_pprz_trans(msg)->spprz_mtx_tx); // lock mutex
  memset(get_pprz_trans(msg)->tx_msg, 0, TRANSPORT_PAYLOAD_LEN);
  get_pprz_trans(msg)->tx_msg_idx = 0;

  insert_byte(get_pprz_trans(msg), PPRZ_STX);
  const uint8_t msg_len = size_of(msg, payload_len);
  insert_byte(get_pprz_trans(msg), msg_len);
}

static void end_message(struct pprzlink_msg *msg, long fd)
{
  msg->dev->put_byte(msg->dev->periph, fd, get_pprz_trans(msg)->ck_a_tx);
  msg->dev->put_byte(msg->dev->periph, fd, get_pprz_trans(msg)->ck_b_tx);
  msg->dev->send_message(msg->dev->periph, fd);


  if (spprz_process_outgoing_packet(get_pprz_trans(msg))) {
    get_pprz_trans(msg)->ck_a_tx = get_pprz_trans(msg)->tx_msg[PPRZ_MSG_LEN_IDX];
    get_pprz_trans(msg)->ck_b_tx = get_pprz_trans(msg)->tx_msg[PPRZ_MSG_LEN_IDX];

    // send first two bytes
    msg->dev->put_byte(msg->dev->periph, fd, get_pprz_trans(msg)->tx_msg[0]);
    msg->dev->put_byte(msg->dev->periph, fd, get_pprz_trans(msg)->tx_msg[1]);

    // we start counting checksum at byte 2
    for (uint8_t i=2; i<get_pprz_trans(msg)->tx_msg_idx; i++) {
      accumulate_checksum(get_pprz_trans(msg), get_pprz_trans(msg)->tx_msg[i]);
      msg->dev->put_byte(msg->dev->periph, fd, get_pprz_trans(msg)->tx_msg[i]);
    }

    // append checksum & send message
    msg->dev->put_byte(msg->dev->periph, fd, get_pprz_trans(msg)->ck_a_tx);
    msg->dev->put_byte(msg->dev->periph, fd, get_pprz_trans(msg)->ck_b_tx);
    msg->dev->send_message(msg->dev->periph, fd);
  }
  PPRZ_MUTEX_UNLOCK(get_pprz_trans(msg)->spprz_mtx_tx); // unlock mutex
}

static void overrun(struct pprzlink_msg *msg)
{
  msg->dev->nb_ovrn++;
}

static void count_bytes(struct pprzlink_msg *msg, uint8_t bytes)
{
  msg->dev->nb_bytes += bytes;
}

static int check_available_space(struct pprzlink_msg *msg, long *fd, uint16_t bytes)
{
  return msg->dev->check_free_space(msg->dev->periph, fd, bytes);
}

// Init pprz transport structure
void pprz_transport_init(struct pprz_transport *t)
{
  t->status = UNINIT;
  t->trans_rx.msg_received = false;
  t->trans_tx.size_of = (size_of_t) size_of;
  t->trans_tx.check_available_space = (check_available_space_t) check_available_space;
  t->trans_tx.put_bytes = (put_bytes_t) put_bytes;
  t->trans_tx.put_named_byte = (put_named_byte_t) put_named_byte;
  t->trans_tx.start_message = (start_message_t) start_message;
  t->trans_tx.end_message = (end_message_t) end_message;
  t->trans_tx.overrun = (overrun_t) overrun;
  t->trans_tx.count_bytes = (count_bytes_t) count_bytes;
  t->trans_tx.impl = (void *)(t);
  PPRZ_MUTEX_INIT(t->spprz_mtx_tx); // init mutex, check if correct pointer
}


// Parsing function
void parse_pprz(struct pprz_transport *t, uint8_t c)
{
  switch (t->status) {
    case UNINIT:
      switch (c) {
        case PPRZ_STX:
          t->status++;
          t->packet_encrypted = false;
          break;
        case PPRZ_STX_SECURE:
          t->status++;
          t->packet_encrypted = true;
          break;
        default:
          break;
      }
      break;
    case GOT_STX:
      if (t->trans_rx.msg_received) {
        t->trans_rx.ovrn++;
        goto error;
      }
      t->trans_rx.payload_len = c - 4; /* Counting STX, LENGTH and CRC1 and CRC2 */
      t->ck_a_rx = t->ck_b_rx = c;
      t->status++;
      t->payload_idx = 0;
      break;
    case GOT_LENGTH:
      t->trans_rx.payload[t->payload_idx] = c;
      t->ck_a_rx += c; t->ck_b_rx += t->ck_a_rx;
      t->payload_idx++;
      if (t->payload_idx == t->trans_rx.payload_len) {
        t->status++;
      }
      break;
    case GOT_PAYLOAD:
      if (c != t->ck_a_rx) {
        goto error;
      }
      t->status++;
      break;
    case GOT_CRC1:
      if (c != t->ck_b_rx) {
        goto error;
      }
      t->trans_rx.msg_received = true;
      goto restart;
    default:
      goto error;
  }
  return;
error:
  t->trans_rx.error++;
restart:
  t->status = UNINIT;
  return;
}


/** Parsing a frame data and copy the payload to the datalink buffer */
void pprz_check_and_parse(struct link_device *dev, struct pprz_transport *trans,
                          uint8_t *buf, bool *msg_available)
{
  if (dev->char_available(dev->periph)) {
    while (dev->char_available(dev->periph) && !trans->trans_rx.msg_received) {
      parse_spprz(trans, dev->get_byte(dev->periph));
    }
    if (trans->trans_rx.msg_received) {
      if (spprz_process_incoming_packet(trans, buf)) {
        *msg_available = true;
      }
      trans->trans_rx.msg_received = false;
    }
  }
}
