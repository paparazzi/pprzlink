/*
 * Copyright (C) 2006  Pascal Brisset, Antoine Drouin
 * Copyright (C) 2014-2015  Gautier Hattenberger <gautier.hattenberger@enac.fr>
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
 * @file pprzlink/xbee_transport.h
 * Maxstream XBee Protocol handling
 */

#ifndef XBEE_TRANSPORT_H
#define XBEE_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>
#include "pprzlink/pprzlink_transport.h"
#include "pprzlink/pprzlink_device.h"

/** Type of XBee module: 2.4 GHz or 868 MHz
 */
enum XBeeType {
  XBEE_24,
  XBEE_868
};

struct xbee_transport {
  enum XBeeType type;  ///< type of xbee module (2.4GHz or 868MHz)
  // generic reception interface
  struct transport_rx trans_rx;
  // specific xbee transport variables
  uint8_t status;
  uint8_t payload_idx;
  uint8_t cs_rx;
  uint8_t rssi;
  // generic transmission interface
  struct transport_tx trans_tx;
  // specific pprz transport_tx variables
  uint8_t cs_tx;
};

/** Initialisation in API mode and setting of the local address
 * FIXME: busy wait */
extern void xbee_transport_init(struct xbee_transport *t, struct link_device *dev, uint16_t addr, enum XBeeType type, uint32_t baudrate, void (*wait)(uint32_t), char *xbee_init);


extern void xbee_check_and_parse(struct link_device *dev, struct xbee_transport *trans, uint8_t *buf, bool *msg_available);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* XBEE_TRANSPORT_H */
