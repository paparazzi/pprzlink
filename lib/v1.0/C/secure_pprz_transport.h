/*
 * Copyright (C) 2003  Pascal Brisset, Antoine Drouin
 * Copyright (C) 2015  Gautier Hattenberger <gautier.hattenberger@enac.fr>
 * Copyright (C) 2017  Michal Podhradsky <mpodhradsky@galois.com>
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
 * @file pprzlink/secure_pprz_transport.h
 *
 * Building and parsing Paparazzi frames with encryption.
 * See https://wiki.paparazziuav.org/wiki/Messages_Format
 * for more details
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

#ifndef SECURE_PPRZ_TRANSPORT_H
#define SECURE_PPRZ_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>
#include "pprzlink/pprzlink_transport.h"
#include "pprzlink/pprzlink_device.h"

// Start byte
#define PPRZ_STX  0x99
#define PPRZ_KEY_LEN 32
#define PPRZ_COUNTER_LEN 4
#define PPRZ_MAC_LEN 16
#define PPRZ_NONCE_LEN 12
#define PPRZ_MAX_AAD_LEN 4
#define PPRZ_HEADER_LEN 4
#define PPRZ_CRYPTO_OVERHEAD (PPRZ_COUNTER_LEN + PPRZ_MAC_LEN)
#define PPRZ_MAX_PAYLOAD_LEN (TRANSPORT_PAYLOAD_LEN - PPRZ_CRYPTO_OVERHEAD - PPRZ_HEADER_LEN)

#define PPRZ_MSG_LEN_IDX 1
#define PPRZ_COUNTER_IDX 2
#define PPRZ_CIPHERTEXT_IDX 6

struct spprz_message {
  uint8_t msg[PPRZ_MAX_PAYLOAD_LEN]; // ciphertext/plaintext
  uint8_t msg_idx; // msg length
  uint8_t mac[PPRZ_MAC_LEN]; // message authentication tag
  uint8_t nonce[PPRZ_NONCE_LEN]; // nonce (using only 4 bytes)
  uint8_t aad[PPRZ_MAX_AAD_LEN]; // additional authentication data
  uint8_t aad_idx; // aad length
};

/*
 * PPRZ Transport
 */
struct spprz_transport {
  // generic reception interface
  struct transport_rx trans_rx;
  // specific pprz transport_rx variables
  uint8_t status;
  uint8_t payload_idx;
  uint8_t ck_a_rx, ck_b_rx;
  // generic transmission interface
  struct transport_tx trans_tx;
  // specific pprz transport_tx variables
  uint8_t ck_a_tx, ck_b_tx;

  struct spprz_message tx_msg; // aux variables for encryption
  struct spprz_message rx_msg; // aux variables for decryption

  uint8_t tx_buffer[TRANSPORT_PAYLOAD_LEN]; // temporary storage for outgoing messages
  uint8_t tx_idx; // length of outgoing buffer
  uint8_t rx_key[PPRZ_KEY_LEN]; // key to decrypt incoming messages
  uint32_t rx_cnt; // counter (IV) for incoming messages
  uint8_t tx_key[PPRZ_KEY_LEN]; // key to encrypt outgoing messages
  uint32_t tx_cnt; // counter (IV) for outgoing messages
};

// Init function
extern void spprz_transport_init(struct spprz_transport *t);

// Checking new data and parsing
extern void spprz_check_and_parse(struct link_device *dev, struct spprz_transport *trans, uint8_t *buf, bool *msg_available);

// Parsing function, only needed for modules doing their own parsing
// without using the pprz_check_and_parse function
extern void parse_spprz(struct spprz_transport *t, uint8_t c);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SECURE_PPRZ_TRANSPORT_H */

