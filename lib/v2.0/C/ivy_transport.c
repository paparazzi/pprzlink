/*
 * Copyright (C) 2003  Pascal Brisset, Antoine Drouin
 * Copyright (C) 2014  Gautier Hattenberger <gautier.hattenberger@enac.fr>
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
 * @file pprzlink/ivy_transport.c
 *
 * Building Paparazzi frames over IVY.
 *
 */

#include "pprzlink/ivy_transport.h"

#include <stdio.h>
#include <Ivy/ivy.h>

static struct ivy_transport * get_ivy_trans(struct pprzlink_msg *msg)
{
  return (struct ivy_transport *)(msg->trans->impl);
}

static void put_bytes(struct pprzlink_msg *msg, long fd __attribute__((unused)),
                      enum TransportDataType type __attribute__((unused)), enum TransportDataFormat format __attribute__((unused)),
                      const void *bytes, uint16_t len)
{
  const uint8_t *b = (const uint8_t *) bytes;
  struct ivy_transport *trans = get_ivy_trans(msg);

  // Start delimiter "quote" for char arrays (strings)
  if (format == DL_FORMAT_ARRAY && type == DL_TYPE_CHAR) {
    trans->ivy_p += sprintf(trans->ivy_p, "\"");
  }

  int i = 0;
  while (i < len) {
    // print data with correct type
    switch (type) {
      case DL_TYPE_CHAR:
        trans->ivy_p += sprintf(trans->ivy_p, "%c", (char)(*((char *)(b + i))));
        i++;
        break;
      case DL_TYPE_UINT8:
        trans->ivy_p += sprintf(trans->ivy_p, "%u", b[i]);
        i++;
        break;
      case DL_TYPE_UINT16:
        trans->ivy_p += sprintf(trans->ivy_p, "%u", (uint16_t)(*((uint16_t *)(b + i))));
        i += 2;
        break;
      case DL_TYPE_UINT32:
      case DL_TYPE_TIMESTAMP:
        trans->ivy_p += sprintf(trans->ivy_p, "%u", (uint32_t)(*((uint32_t *)(b + i))));
        i += 4;
        break;
      case DL_TYPE_UINT64:
#if __WORDSIZE == 64
        trans->ivy_p += sprintf(trans->ivy_p, "%lu", (uint64_t)(*((uint64_t *)(b + i))));
#else
        trans->ivy_p += sprintf(trans->ivy_p, "%llu", (uint64_t)(*((uint64_t *)(b + i))));
#endif
        i += 8;
        break;
      case DL_TYPE_INT8:
        trans->ivy_p += sprintf(trans->ivy_p, "%d", (int8_t)(*((int8_t *)(b + i))));
        i++;
        break;
      case DL_TYPE_INT16:
        trans->ivy_p += sprintf(trans->ivy_p, "%d", (int16_t)(*((int16_t *)(b + i))));
        i += 2;
        break;
      case DL_TYPE_INT32:
        trans->ivy_p += sprintf(trans->ivy_p, "%d", (int32_t)(*((int32_t *)(b + i))));
        i += 4;
        break;
      case DL_TYPE_INT64:
#if __WORDSIZE == 64
        trans->ivy_p += sprintf(trans->ivy_p, "%ld", (uint64_t)(*((uint64_t *)(b + i))));
#else
        trans->ivy_p += sprintf(trans->ivy_p, "%lld", (uint64_t)(*((uint64_t *)(b + i))));
#endif
        i += 8;
        break;
      case DL_TYPE_FLOAT:
        trans->ivy_p += sprintf(trans->ivy_p, "%f", (float)(*((float *)(b + i))));
        i += 4;
        break;
      case DL_TYPE_DOUBLE:
        trans->ivy_p += sprintf(trans->ivy_p, "%f", (double)(*((double *)(b + i))));
        i += 8;
        break;
      case DL_TYPE_ARRAY_LENGTH:
      default:
        // Don't print array length but increment index
        i++;
        break;
    }
    // Coma delimiter for array, no delimiter for char array (string), space otherwise
    if (format == DL_FORMAT_ARRAY) {
      if (type != DL_TYPE_CHAR) {
        trans->ivy_p += sprintf(trans->ivy_p, ",");
      }
    } else {
      trans->ivy_p += sprintf(trans->ivy_p, " ");
    }
  }

  // space end delimiter for arrays, additionally un-quote char arrays (strings)
  if (format == DL_FORMAT_ARRAY) {
    if (type == DL_TYPE_CHAR) {
      trans->ivy_p += sprintf(trans->ivy_p, "\" ");
    } else {
      trans->ivy_p += sprintf(trans->ivy_p, " ");
    }
  }
}

static void put_named_byte(struct pprzlink_msg *msg, long fd __attribute__((unused)),
                           enum TransportDataType type __attribute__((unused)), enum TransportDataFormat format __attribute__((unused)),
                           uint8_t byte __attribute__((unused)), const char *name __attribute__((unused)))
{
  if (name != NULL) {
    get_ivy_trans(msg)->ivy_p += sprintf(get_ivy_trans(msg)->ivy_p, "%s ", name);
  }
}

static uint8_t size_of(struct pprzlink_msg *msg __attribute__((unused)), uint8_t len)
{
  return len;
}

static void start_message(struct pprzlink_msg *msg, long fd __attribute__((unused)),
                          uint8_t payload_len __attribute__((unused)))
{
  get_ivy_trans(msg)->ivy_p = get_ivy_trans(msg)->ivy_buf;
}

static void end_message(struct pprzlink_msg *msg, long fd __attribute__((unused)))
{
  struct ivy_transport *trans = get_ivy_trans(msg);
  *(--trans->ivy_p) = '\0';
  if (trans->ivy_dl_enabled) {
    IvySendMsg("%s", trans->ivy_buf);
    msg->dev->nb_msgs++;
  }
}

static void overrun(struct pprzlink_msg *msg)
{
  msg->dev->nb_ovrn++;
}

static void count_bytes(struct pprzlink_msg *msg, uint8_t bytes)
{
  msg->dev->nb_bytes += bytes;
}

static int check_available_space(struct pprzlink_msg *msg __attribute__((unused)), long *fd __attribute__((unused)), uint16_t bytes __attribute__((unused)))
{
  return 1;
}

static int check_free_space(struct ivy_transport *t __attribute__((unused)), long *fd __attribute__((unused)), uint16_t len __attribute__((unused))) { return 1; }
static void put_byte(struct ivy_transport *t __attribute__((unused)), long fd __attribute__((unused)), uint8_t byte __attribute__((unused))) {}
static void put_buffer(struct ivy_transport *t __attribute__((unused)), long fd __attribute__((unused)), const uint8_t *buffer __attribute__((unused)), uint16_t len __attribute__((unused))) {}
static void send_message(struct ivy_transport *t __attribute__((unused)), long fd __attribute__((unused))) {}
static int null_function(struct ivy_transport *t __attribute__((unused))) { return 0; }
static uint8_t null_byte_function(struct ivy_transport *t __attribute__((unused))) { return 0; }

void ivy_transport_init(struct ivy_transport *t)
{
  t->ivy_p = t->ivy_buf;
  t->ivy_dl_enabled = true;

  t->trans_tx.size_of = (size_of_t) size_of;
  t->trans_tx.check_available_space = (check_available_space_t) check_available_space;
  t->trans_tx.put_bytes = (put_bytes_t) put_bytes;
  t->trans_tx.put_named_byte = (put_named_byte_t) put_named_byte;
  t->trans_tx.start_message = (start_message_t) start_message;
  t->trans_tx.end_message = (end_message_t) end_message;
  t->trans_tx.overrun = (overrun_t) overrun;
  t->trans_tx.count_bytes = (count_bytes_t) count_bytes;
  t->trans_tx.impl = (void *)(t);
  t->device.check_free_space = (check_free_space_t) check_free_space;
  t->device.put_byte = (put_byte_t) put_byte;
  t->device.put_buffer = (put_buffer_t) put_buffer;
  t->device.send_message = (send_message_t) send_message;
  t->device.char_available = (char_available_t) null_function;
  t->device.get_byte = (get_byte_t) null_byte_function;
  t->device.periph = (void *)(t);
}
