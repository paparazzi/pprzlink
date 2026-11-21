// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file ReceivedMessage.h
 * @brief Owned message results with frame and peer metadata.
 * @ingroup transports
 *
 * Metadata belongs to the same receive operation as its Message and is not overwritten by subsequent receives.
 */

#pragma once
#include <pprzlink/Message.h>
#include <cstdint>
#include <optional>
#include <string>

namespace pprzlink {
  /// An IP address and port. No socket or event-loop ownership is hidden here.
  /// @ingroup transports
  struct UdpEndpoint {
    std::string address = "0.0.0.0"; ///< Numeric IPv4 or IPv6 address accepted by Asio.
    uint16_t port = 0; ///< UDP port; zero requests an ephemeral port when binding.
    /// @brief Compare both the address string and port.
    /// @return Whether the two endpoints have identical member values.
    bool operator==(const UdpEndpoint &) const = default;
  };

  /// @brief Radio-source metadata supplied by an XBee receive indication.
  /// @ingroup xbee
  struct XbeeReceiveInfo {
    uint64_t sourceAddress; ///< Radio address, independent of the PPRZLINK sender ID.
    bool addressIs64Bit; ///< Whether sourceAddress represents an eight-byte radio address.
    uint8_t rssi; ///< Positive magnitude of negative dBm, valid only when hasRssi is true.
    uint8_t options; ///< Radio receive-option bits from the API frame.
    bool hasRssi = true; ///< False for extended 868 receive frames, which carry no RSSI.
  };

  /// Metadata borrowed alongside a decoded message during a receive callback.
  /// Copy this value to retain it after the callback returns.
  /// @ingroup transports
  struct ReceiveInfo {
    size_t frameSize = 0; ///< Complete transport frame bytes; zero when unavailable.
    std::optional<XbeeReceiveInfo> xbee; ///< Radio source, RSSI and options when available.
    std::optional<UdpEndpoint> udpPeer; ///< Actual source endpoint of a UDP datagram.
  };

  /// Own a decoded message and the metadata from that same receive operation.
  /// Subsequent receives cannot overwrite this metadata.
  /// @ingroup transports
  struct ReceivedMessage {
    Message message; ///< Decoded message, including its own schema and values.
    size_t frameSize = 0; ///< Complete frame bytes; zero when a custom transport does not report a size.
    std::optional<XbeeReceiveInfo> xbee; ///< Radio metadata for an XBee message, otherwise absent.
    std::optional<UdpEndpoint> udpPeer; ///< Source of this message's UDP datagram, otherwise absent.
  };
}
