/*
 * Copyright 2019 garciafa
 * This file is part of PprzLinkCPP
 *
 * PprzLinkCPP is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PprzLinkCPP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with ModemTester.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/** \file Link.h
 *
 *
 */

#ifndef PPRZLINKCPP_LINK_H
#define PPRZLINKCPP_LINK_H

#include "Message.h"

namespace pprzlink {
  /**
   *
   */
  class Link {

    /**
     *
     * @param sender_id
     * @param component_id
     * @param receiver_id
     * @param msg
     */
    void sendMessage(uint8_t sender_id, uint8_t component_id, uint8_t receiver_id,  const Message &msg);


  };
}
#endif //PPRZLINKCPP_LINK_H
