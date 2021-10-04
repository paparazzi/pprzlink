/*
 * Copyright (C) 2017 Gautier Hattenberger <gautier.hattenberger@enac.fr>
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

/** \file pprzlink_message.h
 *
 *   Generic message header for PPRZLINK message system
 */

#ifndef PPRZLINK_MESSAGE_H
#define PPRZLINK_MESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>
#include "pprzlink_transport.h"
#include "pprzlink_device.h"

// broadcast id (all receivers)
#define PPRZLINK_MSG_BROADCAST 0xFF

// components broadcast id
#define PPRZLINK_COMPONENT_BROADCAST 0

/** Message configuration
 */
struct pprzlink_msg {
  uint8_t sender_id;          ///< sender id
  uint8_t receiver_id;        ///< destination id
  uint8_t component_id;       ///< component id
  struct transport_tx *trans; ///< transport protocol
  struct link_device *dev;    ///< device
};

/* Message id helpers */

/** Getter for the sender id of a message
 * @param msg pointer to the message
 * @return the sender id of the message
 */
static inline uint8_t pprzlink_get_msg_sender_id(void *msg)
{
 return ((uint8_t*)msg)[0];
}

/** Getter for the receiver id of a message
 * @param msg pointer to the message
 * @return the receiver id of the message
 */
static inline uint8_t pprzlink_get_msg_receiver_id(void *msg)
{
 return ((uint8_t*)msg)[1];
}

/** Getter for the component id of a message
 * @param msg pointer to the message
 * @return the component id of the message
 */
static inline uint8_t pprzlink_get_msg_component_id(void *msg) 
{
 return (((uint8_t*)msg)[2] & 0xF0)>>4;
}

/** Getter for the class id of a message
 * @param msg pointer to the message
 * @return the class ID of the message
 */
static inline uint8_t pprzlink_get_msg_class_id(void *msg) 
{
 return (((uint8_t*)msg)[2] & 0x0F);
}

/** Getter for the id of a message in its class
 * @param msg pointer to the message
 * @return the id of a message in its class
 */
static inline uint8_t pprzlink_get_msg_id(void *msg)
{
 return ((uint8_t*)msg)[3];
}

/* Compatibility macros */
#define SenderIdOfPprzMsg pprzlink_get_msg_sender_id
#define IdOfPprzMsg pprzlink_get_msg_id


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PPRZLINK_MESSAGE_H */

