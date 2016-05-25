/*
 * Copyright (C) 2016 Freek van Tienen <freek.v.tienen@gmail.com>
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
 * @file pprzlink/short_transport.c
 *
 * Paparazzi frame with only PPRZ_DATA
 *
 */

#include <inttypes.h>
#include "pprzlink/short_transport.h"

static void put_bytes(struct short_transport *trans __attribute__((unused)), struct link_device *dev, long fd,
                      enum TransportDataType type __attribute__((unused)), enum TransportDataFormat format __attribute__((unused)),
                      const void *bytes, uint16_t len)
{
  uint8_t *b = (uint8_t *)bytes;
  dev->put_buffer(dev->periph, fd, b, len);
}

static void put_named_byte(struct short_transport *trans __attribute__((unused)), struct link_device *dev, long fd,
                           enum TransportDataType type __attribute__((unused)), enum TransportDataFormat format __attribute__((unused)),
                           uint8_t byte, const char *name __attribute__((unused)))
{
  dev->put_byte(dev->periph, fd, byte);
}

static uint8_t size_of(struct short_transport *trans __attribute__((unused)), uint8_t len)
{
  return len;
}

static void start_message(struct short_transport *trans __attribute__((unused)), struct link_device *dev __attribute__((unused)),
                          long fd __attribute__((unused)), uint8_t payload_len __attribute__((unused)))
{
}

static void end_message(struct short_transport *trans __attribute__((unused)), struct link_device *dev, long fd)
{
  dev->send_message(dev->periph, fd);
}

static void overrun(struct short_transport *trans __attribute__((unused)),
                    struct link_device *dev __attribute__((unused)))
{
}

static void count_bytes(struct short_transport *trans __attribute__((unused)),
                        struct link_device *dev __attribute__((unused)), uint8_t bytes __attribute__((unused)))
{
}

static int check_available_space(struct short_transport *trans __attribute__((unused)), struct link_device *dev, long *fd,
                                 uint16_t bytes)
{
  return dev->check_free_space(dev->periph, fd, bytes);
}

void short_transport_init(struct short_transport *t)
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
}

