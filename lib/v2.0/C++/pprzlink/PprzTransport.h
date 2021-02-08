/*
 * Copyright 2020 garciafa
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

/** \file PprzTransport.h
 *
 *
 */


#ifndef PPRZLINKCPP_PPRZTRANSPORT_H
#define PPRZLINKCPP_PPRZTRANSPORT_H

/*
 PPRZ-message: ABCxxxxxxxDE
    A PPRZ_STX (0x99)
    B LENGTH (A->E)
    C PPRZ_DATA
      0 SOURCE (~sender_ID)
      1 DESTINATION (can be a broadcast ID)
      2 CLASS/COMPONENT
        bits 0-3: 16 class ID available
        bits 4-7: 16 component ID available
      3 MSG_ID
      4 MSG_PAYLOAD
      . DATA (messages.xml)
    D PPRZ_CHECKSUM_A (sum[B->C])
    E PPRZ_CHECKSUM_B (sum[ck_a])
 */

#include "Transport.h"

#define PPRZ_STX (0x99)

namespace pprzlink {
  class PprzTransport : public Transport {
  public:
    PprzTransport(Device *device, const MessageDictionary &dictionary);

    bool hasMessage() override;

    std::unique_ptr<Message> getMessage() override;

    size_t sendMessage(Message const &msg) override;
  protected:
    bool decodeMessage();


    BytesBuffer transportBuffer;
    std::unique_ptr<Message> currentMessage;
  };
}
#endif //PPRZLINKCPP_PPRZTRANSPORT_H
