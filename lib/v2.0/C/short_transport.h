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
 * @file pprzlink/short_transport.h
 *
 * Protocol without any overhead and only the paparazzi data
 * This includes AC_ID, MSG_ID and MSG_DATA
 */

#ifndef SHORT_TRANSPORT_H
#define SHORT_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pprzlink/pprzlink_transport.h"

struct short_transport {
  // generic transmission interface
  struct transport_tx trans_tx;
};

// Init function
extern void short_transport_init(struct short_transport *t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
