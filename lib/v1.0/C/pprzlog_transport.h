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
 * @file pprzlink/pprzlog_transport.h
 *
 * Protocol for on-board data logger with timestamp
 *
 */

#ifndef PPRZLOG_TRANSPORT_H
#define PPRZLOG_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pprzlink/pprzlink_transport.h"

typedef uint32_t (*get_time_usec_t)(void);

struct pprzlog_transport {
  // generic transmission interface
  struct transport_tx trans_tx;
  // specific pprz transport_tx variables
  uint8_t ck;
  // get current time function pointer
  get_time_usec_t get_time_usec;
};

// Init function
extern void pprzlog_transport_init(struct pprzlog_transport *t, uint32_t (*get_time_usec_t)(void));

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

