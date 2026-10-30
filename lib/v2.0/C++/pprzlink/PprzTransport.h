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

/**
 * @file PprzTransport.h
 * @brief PPRZLINK v2 messages exchanged through an owned byte stream.
 * @ingroup transports
 *
 * The transport borrows its dictionary, retains incomplete input and exposes complete messages together with frame statistics.
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
#include "PprzFrameCodec.h"

/// @brief Start delimiter of a PPRZLINK v2 serial frame.
#define PPRZ_STX (0x99)

namespace pprzlink {
  /// Frame Messages as PprzLink v2 bytes and decode incoming frames from its owned Device.
  /// @ingroup transports
  class PprzTransport : public Transport {
  public:
    /// @brief Own a byte stream and create an incremental frame decoder.
    /// @param[in] device Non-null stream transferred to the transport.
    /// @param[in] dictionary Borrowed schemas that must outlive the transport.
    /// @throws std::invalid_argument The device pointer is null.
    PprzTransport(std::unique_ptr<Device> device, const MessageDictionary &dictionary);

    /// @brief Disable pending notifications before releasing decoder state.
    ~PprzTransport() override;

    /// @brief Encode and synchronously write one complete serial frame.
    /// @param[in] msg Fully populated message compatible with the peer's XML.
    /// @return Written frame bytes; no remote delivery acknowledgement is implied.
    /// @throws std::exception Message validation, encoding or device I/O fails.
    size_t sendMessage(const Message &msg) override;

  protected:
    /// @brief Decode and distribute every complete frame following an input notification.
    void receiveAvailable() override;
    /// @brief Schedule buffered-frame delivery on the device executor after a restart.
    void receptionStarted() override;

    PprzFrameDecoder decoder; ///< Incremental decoder borrowing the same dictionary.
  };
}
#endif // PPRZLINKCPP_PPRZTRANSPORT_H
