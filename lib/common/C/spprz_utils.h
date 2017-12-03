/*
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
 */

/**
 * @file pprzlink/spprz_utils.h
 *
 * Secure link utilities
 *
 */
#pragma once
#ifndef SPPRZ_UTILS_H
#define SPRRZ_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "std.h"
#include "pprzlink/spprz_transport.h"

#define PPRZ_STX_SECURE 0xAA
#define PPRZ_MSG_LEN_IDX 1


/* Function shall be provided implementation */

/**
 * Process incoming packet.
 *
 * The packet is stripped of STX, LEN, and Checksum
 * The packet status (encrypted/plaintext) based on STX type is encoded in
 * trans->packet_encrypted
 *
 * trans->trans_rx.payload  - contains the payload data
 * trans->trans_rx.payload_len - payload data length
 *
 * buf - a buffer containing processed payload. In trivial case, it is a copy of
 * trans->trans_rx.payload
 *
 * The format is expected to comply with Pprzlink protocol of given version (1.0/2.0) and is
 * up to the implementation to ensure.
 *
 * Returns:
 * true - if the packet should be received
 * false - if the packet should be discarded
 *
 * Note: in case the function returns false, it is expected to log an error
 */
bool spprz_process_incoming_packet(struct spprz_transport *trans, uint8_t *buf);

/**
 * Process outgoing packet
 *
 * The received trans->tx_msg has the following format:
 * trans->tx_msg[0] = PPRZ_STX
 * trans->tx_msg[1] = msg_len (including checksum and unsecured protocol overhead)
 * trans->tx_msg[2+] = payload
 *
 * trans->tx_msg_idx doesn't include checksum (so msg_len = tx_msg_idx + 2)
 *
 * The returned trans->tx_msg buffer shall have the following format:
 * trans->tx_msg[0] = PPRZ_STX or PPRZ_STX_SECURE
 * trans->tx_msg[1] = msg_len (including any additional cryptographic overhead and checksum)
 * trans->tx_msg[2+] = additional data and payload
 *
 * trans->tx_msg_idx does not include checksum! (so msg_len = tx_msg_idx + 2)
 *
 * Checksum is calculated before sending in end_message() function
 *
 * Returns:
 * true - if the packet should be sent
 * false - if the packet should be discarded
 *
 * Note: in case the function returns false, it is expected to log an error
 */
bool spprz_process_outgoing_packet(struct spprz_transport *trans);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SPPRZ_UTILS_H */

