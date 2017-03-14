/*
 * Copyright (C) 2015 Gautier Hattenberger <gautier.hattenberger@enac.fr>
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
 * @file pprzlink/print_utils.h
 *
 * Utility functions to print HEX format on pprzlink device
 *
 */

#ifndef PRINT_UTILS_H
#define PRINT_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pprzlink/pprzlink_device.h"

static inline void print_string(struct link_device *dev, long fd, char *s)
{
  uint8_t i = 0;
  while (s[i]) {
    dev->put_byte(dev->periph, fd, s[i]);
    i++;
  }
}

static inline void print_hex(struct link_device *dev, long fd, uint8_t c)
{
  const uint8_t hex[16] =
  { '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  uint8_t high = (c & 0xF0)>>4;
  uint8_t low  = c & 0x0F;
  dev->put_byte(dev->periph, fd, hex[high]);
  dev->put_byte(dev->periph, fd, hex[low]);
}

static inline void print_hex16(struct link_device *dev, long fd, uint16_t c)
{
  uint8_t high16 = (uint8_t)(c>>8);
  uint8_t low16  = (uint8_t)(c);
  print_hex(dev, fd, high16);
  print_hex(dev, fd, low16);
}

static inline void print_hex32(struct link_device *dev, long fd, uint32_t c)
{
  uint16_t high32 = (uint16_t)(c>>16);
  uint16_t low32  = (uint16_t)(c);
  print_hex16(dev, fd, high32);
  print_hex16(dev, fd, low32);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PRINT_UTILS_H */

