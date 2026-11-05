// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file UdpTransport.h
 * @brief Asynchronous PPRZLINK datagrams with explicit destinations.
 * @ingroup transports
 *
 * Every send names a destination. Source endpoint metadata accompanies each received message; application discovery and broadcast policy remain outside the transport.
 */

#pragma once
#include <pprzlink/PprzFrameCodec.h>
#include <pprzlink/Receiver.h>
#include <boost/asio/ip/udp.hpp>

namespace pprzlink {
  /// @brief Local socket binding and broadcast permission for a UDP transport.
  /// @ingroup transports
  struct UdpOptions {
    UdpEndpoint local; ///< Bind address and port; zero selects an ephemeral port.
    bool broadcast = false; ///< Enable the socket's broadcast option without choosing destinations.
  };

  /// PPRZ frames over UDP. Start reception and run the supplied Asio context.
  /// Unlike a byte stream, every send needs an explicit destination. Aircraft
  /// discovery, uplink ports and broadcast policy belong to the application.
  /// The supplied context and dictionary must outlive the transport.
  /// @ingroup transports
  class UdpTransport : public Receiver {
  public:
    /// @brief Open and bind a UDP socket; start() arms asynchronous reception.
    /// @param[in] context Asio context that must outlive this transport.
    /// @param[in] dictionary Borrowed, unchanged schemas that must outlive this transport.
    /// @param[in] options Numeric local endpoint and broadcast permission.
    /// @throws boost::system::system_error Invalid address, socket option or bind failure.
    UdpTransport(boost::asio::io_context &context, const MessageDictionary &dictionary,
                 const UdpOptions &options = {});
    /// @brief Stop callbacks and close the socket; canceled reads retain their buffers.
    ~UdpTransport() override;
    /// @brief Copying the socket/decoder owner is prohibited.
    UdpTransport(const UdpTransport &) = delete;
    /// @brief Copy assignment is prohibited.
    /// @return This operation is deleted and cannot return.
    UdpTransport &operator=(const UdpTransport &) = delete;
    /// @brief Moving the socket/decoder owner is prohibited.
    UdpTransport(UdpTransport &&) = delete;
    /// @brief Move assignment is prohibited.
    /// @return This operation is deleted and cannot return.
    UdpTransport &operator=(UdpTransport &&) = delete;

    /// @brief Start asynchronous datagram reception on the caller's io_context.
    void start() override;
    /// @brief Cancel reception without closing the socket or stopping the shared loop.
    void stop() override;
    /// @brief Encode one message and synchronously send its complete frame as one datagram.
    /// @param[in] message Fully populated message compatible with the destination's XML.
    /// @param[in] destination Numeric IPv4/IPv6 address and explicit remote UDP port.
    /// @return Complete frame bytes sent.
    /// @throws std::exception Encoding, address parsing, socket I/O or incomplete write failure.
    size_t sendMessage(const Message &message, const UdpEndpoint &destination);
    /// @brief Query the bound local endpoint, including an OS-selected port.
    /// @return Address and port as a value independent of the socket's lifetime.
    /// @throws boost::system::system_error Querying the socket fails.
    [[nodiscard]] UdpEndpoint localEndpoint() const;
    /// @brief Borrow cumulative frame validation counters.
    /// @return Decoder statistics updated by asynchronous receive completions.
    [[nodiscard]] const TransportStatistics &getStatistics() const noexcept { return decoder.getStatistics(); }

  private:
    struct IoState;
    std::shared_ptr<IoState> io; ///< Socket and buffers retained by pending completion handlers.
    PprzFrameDecoder decoder;
    uint64_t generation = 0; ///< Discards completions from an earlier start/stop session.
    void armReceive();
  };
}
