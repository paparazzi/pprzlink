/*
 * Copyright (C) 2003  Pascal Brisset, Antoine Drouin
 * Copyright (C) 2015  Gautier Hattenberger <gautier.hattenberger@enac.fr>
 * Copyright (C) 2017 Michal Podhradsky <mpodhradsky@galois.com>
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
 * @file pprzlink/spprz_transport.h
 *
 * Building and parsing secure Paparazzi frames.
 * Provides only basic hooks, the majority of work (encryption/decryption)
 * is done in an external user-supplied file.
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

#ifndef SPPRZ_TRANSPORT_H
#define SPPRZ_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>
#include "pprzlink/pprzlink_transport.h"
#include "pprzlink/pprzlink_device.h"

// Start byte
#define PPRZ_STX        0x99

/* PPRZ Transport
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

  // secure link specific:
  // buffered tx message
  uint8_t tx_msg[TRANSPORT_PAYLOAD_LEN];
  volatile uint8_t tx_msg_idx;
  uint8_t packet_type;
};

// include after defining the spprz_stransport struct
#include "pprzlink/spprz_utils.h"

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

#endif /* SPPRZ_TRANSPORT_H */

