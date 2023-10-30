/*
 * Copyright (C) 2014-2015 Gautier Hattenberger <gautier.hattenberger@enac.fr>
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
 * @file pprzlink/pprzlog_transport.c
 *
 * Building and Paparazzi frames with timestamp for data logger.
 *
 * LOG-message: ABCDEFGHxxxxxxxI
 *   A PPRZ_STX (0x99)
 *   B LENGTH (H->H)
 *   C SOURCE (0=uart0, 1=uart1, 2=i2c0, ...)
 *   D TIMESTAMP_LSB (100 microsec raster)
 *   E TIMESTAMP
 *   F TIMESTAMP
 *   G TIMESTAMP_MSB
 *   H PPRZ_DATA
 *     0 SENDER_ID
 *     1 MSG_ID
 *     2 MSG_PAYLOAD
 *     . DATA (messages.xml)
 *   I CHECKSUM (sum[B->H])
 *
 */

#include <inttypes.h>
#include "pprzlink/pprzlog_transport.h"

#define STX_LOG  0x99

static struct pprzlog_transport * get_pprzlog_trans(struct pprzlink_msg *msg)
{
  return (struct pprzlog_transport *)(msg->trans->impl);
}

static void accumulate_checksum(struct pprzlog_transport *trans, const uint8_t byte)
{
  trans->ck += byte;
}

static void put_bytes(struct pprzlink_msg *msg, long fd,
                      enum TransportDataType type __attribute__((unused)), enum TransportDataFormat format __attribute__((unused)),
                      const void *bytes, uint16_t len)
{
  const uint8_t *b = (const uint8_t *) bytes;
  int i;
  for (i = 0; i < len; i++) {
    accumulate_checksum(get_pprzlog_trans(msg), b[i]);
  }
  msg->dev->put_buffer(msg->dev->periph, fd, b, len);
}

static void put_named_byte(struct pprzlink_msg *msg, long fd,
                           enum TransportDataType type __attribute__((unused)), enum TransportDataFormat format __attribute__((unused)),
                           uint8_t byte, const char *name __attribute__((unused)))
{
  accumulate_checksum(get_pprzlog_trans(msg), byte);
  msg->dev->put_byte(msg->dev->periph, fd, byte);
}

static uint8_t size_of(struct pprzlink_msg *msg __attribute__((unused)), uint8_t len)
{
  // add offset: STX(1), LENGTH(1), SOURCE(1), TIMESTAMP(4), CHECKSUM(1)
  return len + 8;
}

static void start_message(struct pprzlink_msg *msg, long fd, uint8_t payload_len)
{
  msg->dev->put_byte(msg->dev->periph, fd, STX_LOG);
  const uint8_t msg_len = payload_len; // only the payload length here
  get_pprzlog_trans(msg)->ck = 0;
  uint8_t buf[] = { msg_len, 0 }; // TODO use correct source ID
  put_bytes(msg, fd, DL_TYPE_UINT8, DL_FORMAT_SCALAR, buf, 2);
  uint32_t ts = get_pprzlog_trans(msg)->get_time_usec100();
  put_bytes(msg, fd, DL_TYPE_TIMESTAMP, DL_FORMAT_SCALAR, (uint8_t *)(&ts), 4);
}

static void end_message(struct pprzlink_msg *msg, long fd)
{
  msg->dev->put_byte(msg->dev->periph, fd, get_pprzlog_trans(msg)->ck);
  msg->dev->send_message(msg->dev->periph, fd);
}

static void overrun(struct pprzlink_msg *msg __attribute__((unused)))
{
}

static void count_bytes(struct pprzlink_msg *msg __attribute__((unused)), uint8_t bytes __attribute__((unused)))
{
}

static int check_available_space(struct pprzlink_msg *msg, long *fd, uint16_t bytes)
{
  return msg->dev->check_free_space(msg->dev->periph, fd, bytes);
}

void pprzlog_transport_init(struct pprzlog_transport *t, get_time_usec100_t get_time_usec100)
{
  t->trans_tx.size_of = (size_of_t) size_of;
  t->trans_tx.check_available_space = (check_available_space_t) check_available_space;
  t->trans_tx.put_bytes = (put_bytes_t) put_bytes;
  t->trans_tx.put_named_byte = (put_named_byte_t) put_named_byte;
  t->trans_tx.start_message = (start_message_t) start_message;
  t->trans_tx.end_message = (end_message_t) end_message;
  t->trans_tx.overrun = (overrun_t) overrun;
  t->trans_tx.count_bytes = (count_bytes_t) count_bytes;
  t->trans_tx.impl = (void *)(t);
  t->get_time_usec100 = get_time_usec100;
}

