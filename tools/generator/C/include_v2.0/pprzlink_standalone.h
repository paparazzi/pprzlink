/*
 * Copyright (C) 2019 Gautier Hattenberger <gautier.hattenberger@enac.fr>
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

/** \file pprzlink_standalone.h
 *
 *  Definition and utility functions for C standalone library
 */

#ifndef PPRZLINK_STANDALONE_H
#define PPRZLINK_STANDALONE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <inttypes.h>

#define PPRZLINK_STX 0x99

//
// TX definitions
//

typedef int (*check_space_t)(uint8_t); // in: number of bytes free space, out: true if enough space available
typedef void (*put_char_t)(uint8_t); // in: byte to send
typedef void (*send_message_t)(void); // if needed, terminate message sending

/** Transmission device structure
 */
struct pprzlink_device_tx {
  check_space_t check_space;
  put_char_t put_char;
  send_message_t send_message;
  uint8_t ck_a, ck_b;
};

/** Init TX device
 *
 * @param cs function pointer to check free space in device
 * @param pc function pointer to send/add a single byte
 * @param sm function pointer to send to complete message or NULL if not needed
 */
static inline struct pprzlink_device_tx pprzlink_device_tx_init(check_space_t cs, put_char_t pc, send_message_t sm) {
  struct pprzlink_device_tx dev;
  dev.check_space = cs;
  dev.put_char = pc;
  dev.send_message = sm;
  dev.ck_a = 0;
  dev.ck_b = 0;
  return dev;
}

/** Put several bytes to the device and compute the checksum accordingly
 *
 * @param dev pointer to the TX device structure
 * @param b byte array
 * @param len number of bytes to add
 */
static inline void pprzlink_put_bytes(struct pprzlink_device_tx *dev, uint8_t *b, int len) {
  int i;
  for (i = 0; i < len; i++) {
    dev->put_char(b[i]);
    dev->ck_a += b[i];
    dev->ck_b += dev->ck_a;
  }
}

//
// RX definitions
//

typedef int (*char_available_t)(void); // out: true if at least one byte available
typedef uint8_t (*get_char_t)(void); // out: get next available byte

// PPRZLINK parsing state machine
#define PPRZLINK_UNINIT      0
#define PPRZLINK_GOT_STX     1
#define PPRZLINK_GOT_LENGTH  2
#define PPRZLINK_GOT_PAYLOAD 3
#define PPRZLINK_GOT_CRC1    4

/** Transmission device structure
 */
struct pprzlink_device_rx {
  char_available_t char_available;
  get_char_t get_char;
  bool msg_received;
  uint8_t *payload;
  uint8_t payload_len;
  uint8_t payload_idx;
  uint8_t status;
  uint8_t ck_a, ck_b;
  void *user_data;
};

/** Init RX device
 *
 * @param ca function pointer to check for new bytes available
 * @param pc function pointer to get the next byte
 * @param buf pointer to the input payload buffer, should be long enough to parse any type of incoming messages (always less than 255 bytes)
 */
static inline struct pprzlink_device_rx pprzlink_device_rx_init(char_available_t ca, get_char_t gc, uint8_t *buf, void *user_data) {
  struct pprzlink_device_rx dev;
  dev.char_available = ca;
  dev.get_char = gc;
  dev.payload = buf;
  dev.status = PPRZLINK_UNINIT;
  dev.msg_received = false;
  dev.ck_a = 0;
  dev.ck_b = 0;
  dev.user_data = user_data;
  return dev;
}

/** Parsing function
 *
 * @param dev pointer to the RX device structure
 * @param c next byte to parse
 */
static inline void pprzlink_parse(struct pprzlink_device_rx *dev, uint8_t c)
{
  switch (dev->status) {
    case PPRZLINK_UNINIT:
      if (c == PPRZLINK_STX) {
        dev->status++;
      }
      break;
    case PPRZLINK_GOT_STX:
      if (dev->msg_received) {
        goto error; // previous message not processed
      }
      dev->payload_len = c - 4; /* Counting STX, LENGTH and CRC1 and CRC2 */
      dev->ck_a = dev->ck_b = c;
      dev->status++;
      dev->payload_idx = 0;
      break;
    case PPRZLINK_GOT_LENGTH:
      dev->payload[dev->payload_idx] = c;
      dev->ck_a += c;
      dev->ck_b += dev->ck_a;
      dev->payload_idx++;
      if (dev->payload_idx == dev->payload_len) {
        dev->status++;
      }
      break;
    case PPRZLINK_GOT_PAYLOAD:
      if (c != dev->ck_a) {
        goto error;
      }
      dev->status++;
      break;
    case PPRZLINK_GOT_CRC1:
      if (c != dev->ck_b) {
        goto error;
      }
      dev->msg_received = true;
      goto restart;
    default:
      goto error;
  }
  return;
error:
restart:
  dev->status = PPRZLINK_UNINIT;
  return;
}

/** New message callback
 *  parameters: sender id, receiver id, class id, message id, payload buffer, user_data pointer
 */
typedef void (*new_message_t)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t*, void*);

/** Check and parse, call function on new message received
 */
static inline void pprzlink_check_and_parse(struct pprzlink_device_rx *dev, new_message_t new_message)
{
  if (dev->char_available()) {
    while (dev->char_available() && !dev->msg_received) {
      pprzlink_parse(dev, dev->get_char());
    }
    if (dev->msg_received) {
      uint8_t sender_id = dev->payload[0];
      uint8_t receiver_id = dev->payload[1];
      uint8_t class_id = dev->payload[2];
      uint8_t message_id = dev->payload[3];
      new_message(sender_id, receiver_id, class_id, message_id, dev->payload, dev->user_data);
      dev->msg_received = false;
    }
  }
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // PPRZLINK_STANDALONE_H

