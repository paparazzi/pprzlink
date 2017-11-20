/*
 * Copyright (C) 2006  Pascal Brisset, Antoine Drouin
 * Copyright (C) 2014-2015  Gautier Hattenberger <gautier.hattenberger@enac.fr>
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
 * @file pprzlink/secure_pprz_transport.c
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

#include <inttypes.h>
#include "pprzlink/secure_pprz_transport.h"

// crypto library for encryption/decryption
// NOTE: this could be abstracted into some more generic crypto function
#include "datalink/hacl-c/Chacha20Poly1305.h"

// PPRZ parsing state machine
#define UNINIT      0
#define GOT_STX     1
#define GOT_LENGTH  2
#define GOT_PAYLOAD 3
#define GOT_CRC1    4

#define msg_put_byte(_t,_b) {\
    _t.msg[_t.msg_idx] = _b;\
    _t.msg_idx++;\
}

#define buffer_put_byte(_t,_b) {\
    _t->tx_buffer[_t->tx_idx] = _b;\
    _t->tx_idx++;\
}


static void accumulate_checksum(struct spprz_transport *trans, const uint8_t byte)
{
  trans->ck_a_tx += byte;
  trans->ck_b_tx += trans->ck_a_tx;
}


static void put_bytes(struct spprz_transport *trans, struct link_device *dev __attribute__((unused)), long fd __attribute__((unused)),
                      enum TransportDataType type __attribute__((unused)), enum TransportDataFormat format __attribute__((unused)),
                      const void *bytes, uint16_t len)
{
  const uint8_t *b = (const uint8_t *) bytes;
  int i;
  for (i = 0; i < len; i++) {
    // note we are putting bytes indiscriminately into the msg buffer
    msg_put_byte(trans->tx_msg, b[i]);
  }
}


static void put_named_byte(struct spprz_transport *trans, struct link_device *dev __attribute__((unused)), long fd __attribute__((unused))  ,
                           enum TransportDataType type __attribute__((unused)), enum TransportDataFormat format __attribute__((unused)),
                           uint8_t byte, const char *name __attribute__((unused)))
{
  // note we are putting bytes indiscriminately into the msg buffer
  msg_put_byte(trans->tx_msg, byte);
}


/**
 * Determine size of the message
 * The size changes based on whether we are sending encrypted or non-encrypted data
 */
static uint8_t size_of(struct spprz_transport *trans , uint8_t len)
{
  // this depends on whether we are encrypting data or not
  if (trans->crypto_ok) {
    // message length = payload +
    // protocol overhead (STX + len + ck_a + ck_b = 4) +
    // crypto overhead (4 byte counter, 16 byte tag)
    return len + PPRZ_HEADER_LEN + PPRZ_CRYPTO_OVERHEAD;
  } else {
    // message length = payload +
    // protocol overhead (STX + len + ck_a + ck_b = 4)
    return len + PPRZ_HEADER_LEN;
  }
}


/**
 * Initiate a new message to be sent
 */
static void start_message(struct spprz_transport *trans, struct link_device *dev __attribute__((unused)), long fd __attribute__((unused)), uint8_t payload_len)
{
  memset(&(trans->tx_msg),0,sizeof(trans->tx_msg)); // erase aux variables
  memset(&(trans->tx_buffer),0,sizeof(trans->tx_buffer)); // erase message buffer
  trans->tx_idx = 0; // reset counter
  buffer_put_byte(trans, PPRZ_STX); // insert STX
  const uint8_t msg_len = size_of(trans, payload_len); // length including crypto overhead and header
  buffer_put_byte(trans, msg_len); // insert payload length
}


/**
 * Finalize and send message
 * Note that unless the protocol status is CRYPTO_OK,
 * no data encryption and message sending is performed.
 * In such case the data are silently dropped.
 *
 * If you want to send unencrypted messages (for example during STS phase),
 * after calling end_message(), `spprz_send_plaintext()` has to be explicitly called to
 * calculate checksum and send the data.
 *
 * If status = CRYPTO_OK, message encryption is performend, and if successfull the counter
 * is incremented and the encrypted message is sent.
 */
static void end_message(struct spprz_transport *trans, struct link_device *dev, long fd)
{
  // TODO: properly check which messages can be send
  if (trans->crypto_ok != true) {
    // return immediately
    return;
  }

  // set nonce
  trans->tx_cnt++;  // increment first
  memcpy(trans->tx_msg.nonce, &trans->tx_cnt, sizeof(uint32_t));  // simply copy 4 byte counter

  // append counter to the buffer
  memcpy(&trans->tx_buffer[trans->tx_idx], &trans->tx_cnt, sizeof(uint32_t));
  trans->tx_idx += sizeof(uint32_t);

  // we authenticate the counter
  memcpy(trans->tx_msg.aad, &trans->tx_cnt, sizeof(uint32_t));
  trans->tx_msg.aad_idx += sizeof(uint32_t);

  // encrypt
  uint32_t res = Chacha20Poly1305_aead_encrypt(&trans->tx_buffer[trans->tx_idx],  // ciphertext
      trans->tx_msg.mac,  // mac
      trans->tx_msg.msg,  // plaintext
      trans->tx_msg.msg_idx,  // plaintext len
      trans->tx_msg.aad,  // aad
      trans->tx_msg.aad_idx,  // aad len
      trans->tx_key,  // key
      trans->tx_msg.nonce);  // nonce

  // check result
  if (res != 0) {
    trans->encrypt_err++;
    return;
  }

  // increment tx buffer index with the ciphertext
  trans->tx_cnt += trans->tx_msg.msg_idx;

  // append 16 byte tag to the tx buffer
  memcpy(&trans->tx_buffer[trans->tx_idx], trans->tx_msg.mac, PPRZ_MAC_LEN);
  trans->tx_idx += PPRZ_MAC_LEN;

  // initialize checksum
  trans->ck_a_tx = trans->tx_buffer[PPRZ_MSG_LEN_IDX];
  trans->ck_b_tx = trans->tx_buffer[PPRZ_MSG_LEN_IDX];

  // calculate checksum
  for (uint8_t i = 1; i < trans->tx_idx; i++) {
    accumulate_checksum(trans, trans->tx_buffer[i]);
  }

  // send everything
  dev->put_buffer(dev->periph, 0, trans->tx_buffer, trans->tx_idx);  // payload
  dev->put_byte(dev->periph, fd, trans->ck_a_tx);  // checksum
  dev->put_byte(dev->periph, fd, trans->ck_b_tx);  // checksum
  dev->send_message(dev->periph, fd);  // send
}


/**
 * Send plaintext data from the buffer
 * Only checksum calculation is performend
 * Intended to be used during the STS phase
 * NOTE: USE WITH CARE!
 */
extern void spprz_send_plaintext(struct link_device *dev, struct spprz_transport *trans){
  // initialize checksum
  trans->ck_a_tx = trans->tx_buffer[PPRZ_MSG_LEN_IDX];
  trans->ck_b_tx = trans->tx_buffer[PPRZ_MSG_LEN_IDX];

  // calculate checksum
  for (uint8_t i = 1; i < trans->tx_idx; i++) {
    accumulate_checksum(trans, trans->tx_buffer[i]);
  }

  // send everything
  dev->put_buffer(dev->periph, 0, trans->tx_buffer, trans->tx_idx);  // payload
  dev->put_byte(dev->periph, 0, trans->ck_a_tx);  // checksum
  dev->put_byte(dev->periph, 0, trans->ck_b_tx);  // checksum
  dev->send_message(dev->periph, 0);  // send
}


static void overrun(struct spprz_transport *trans __attribute__((unused)), struct link_device *dev)
{
  dev->nb_ovrn++;
}

static void count_bytes(struct spprz_transport *trans __attribute__((unused)), struct link_device *dev, uint8_t bytes)
{
  dev->nb_bytes += bytes;
}

static int check_available_space(struct spprz_transport *trans __attribute__((unused)), struct link_device *dev __attribute__((unused)),
                                 long *fd __attribute__((unused)), uint16_t bytes)
{
  // check if we are attempting to send less than 255 bytes (TODO: handle overflows?)
  return bytes <= TRANSPORT_PAYLOAD_LEN;
}

// Init pprz transport structure
void spprz_transport_init(struct spprz_transport *t)
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

  // counters
  t->rx_cnt = 0;
  t->tx_cnt = 0;

  //init keys
  memset(t->rx_key, 0, PPRZ_KEY_LEN);
  memset(t->tx_key, 0, PPRZ_KEY_LEN);

  // comm status
  t->crypto_ok = false;

  // error counters
  t->counter_err = 0;
  t->decrypt_err = 0;
  t->encrypt_err = 0;
}


// Parsing function
void parse_spprz(struct spprz_transport *t, uint8_t c)
{
  switch (t->status) {
    case UNINIT:
      if (c == PPRZ_STX) {
        t->status++;
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

/**
 * Process unencrypted message
 * To be called before CRYPTO_OK status is achieved
 */
inline void spprz_handle_plaintext_message(struct spprz_transport *trans, uint8_t *buf, bool *msg_available) {
  uint8_t i;
  for (i = 0; i < trans->trans_rx.payload_len; i++) {
    buf[i] = trans->trans_rx.payload[i];
  }
  *msg_available = true;
}


/**
 * Process encrypted messages.
 * Attempt to decrypt, can fail.
 */
inline void spprz_handle_encrypted_message(struct spprz_transport *trans, uint8_t *buf, bool *msg_available) {
  // check counter
  uint32_t new_cnt = 0;
  memcpy(&new_cnt, trans->trans_rx.payload, PPRZ_COUNTER_LEN);

  if (new_cnt <= trans->rx_cnt) {
    *msg_available = false;
    trans->counter_err++;
    return;  // counter has to be monotonically increasing
  }

  // update nonce
  memcpy(trans->rx_msg.nonce, &new_cnt, PPRZ_COUNTER_LEN);

  // authenticate and decrypt
  memset(&(trans->rx_msg), 0, sizeof(trans->rx_msg));  // erase aux variables

  uint32_t clen = trans->trans_rx.payload_len - PPRZ_CRYPTO_OVERHEAD;
  uint32_t res = Chacha20Poly1305_aead_decrypt(trans->rx_msg.msg,  // plaintext
      &trans->trans_rx.payload[PPRZ_CIPHERTEXT_IDX],  // ciphertext
      clen,  // ciphertext len
      &trans->trans_rx.payload[PPRZ_CIPHERTEXT_IDX + clen],  // mac
      &trans->trans_rx.payload[PPRZ_COUNTER_IDX],  // aad (counter)
      PPRZ_COUNTER_LEN,  // aad len
      trans->rx_key,  // key
      trans->rx_msg.nonce);  // nonce

  if (res != 0) {
    *msg_available = false;
    trans->decrypt_err++;
    return;  // either decryption or authentication failed
  }

  // update the counter
  trans->rx_cnt = new_cnt;

  // copy decrypted payload and sender ID and message ID to the buffer
  trans->trans_rx.payload_len = (uint8_t) clen;
  memcpy(trans->trans_rx.payload, trans->rx_msg.msg,
      trans->trans_rx.payload_len);

  // mark new message available
  *msg_available = true;
}


/** Parsing a frame data and copy the payload to the datalink buffer */
void spprz_check_and_parse(struct link_device *dev, struct spprz_transport *trans, uint8_t *buf, bool *msg_available)
{
  if (dev->char_available(dev->periph)) {
    while (dev->char_available(dev->periph) && !trans->trans_rx.msg_received) {
      parse_spprz(trans, dev->get_byte(dev->periph));
    }
    if (trans->trans_rx.msg_received) {
      // differentiate between handling encrypted and unecrypted messages
      if (trans->crypto_ok) {
        // attempt to decrypt and pass the result
        spprz_handle_encrypted_message(trans, buf, msg_available);
      } else {
        // simply copy buffer over and let the upper layers handle the data
        spprz_handle_plaintext_message(trans, buf, msg_available);
      }
    }

    // either way mark the received message as processed
    trans->trans_rx.msg_received = false;
  }
}
