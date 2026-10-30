// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file UdpTransport.cpp
 * @brief UDP framing and per-datagram source tracking.
 * @ingroup transports
 *
 * Multiple frames in one datagram retain its source. An incomplete trailing frame is discarded before receiving another datagram or peer.
 */

#include "UdpTransport.h"
#include <array>
#include <mutex>

namespace pprzlink {
  /// @brief Internal conversion between value endpoints and native Asio endpoints.
  /// @ingroup internals
  namespace {
    /// @brief Parse a numeric endpoint without performing DNS lookup.
    /// @param[in] address Numeric IP address and UDP port.
    /// @return Asio endpoint selecting IPv4 or IPv6 from the parsed address.
    /// @throws boost::system::system_error The address is not a supported numeric IP string.
    boost::asio::ip::udp::endpoint socketEndpoint(const UdpEndpoint &address)
    {
      return {boost::asio::ip::make_address(address.address), address.port};
    }
  }

  /// @brief Socket and operation buffers retained until canceled completions finish.
  /// @ingroup internals
  struct UdpTransport::IoState {
    /// @brief Bind socket operations to the supplied execution context.
    /// @param[in] context Event loop that outlives pending completions.
    explicit IoState(boost::asio::io_context &context) : socket(context) {}
    boost::asio::ip::udp::socket socket; ///< Owned UDP socket.
    std::mutex mutex; ///< Protects operations and the shared buffer/peer pair.
    std::array<uint8_t, 65536> bytes; ///< Storage for one complete datagram.
    boost::asio::ip::udp::endpoint peer; ///< Source endpoint written by the same receive operation.
    bool pending = false; ///< Prevents read buffer reuse before completion.
  };

  UdpTransport::UdpTransport(boost::asio::io_context &context,
                             const MessageDictionary &dictionary, const UdpOptions &options)
    : Receiver(dictionary, Kind::Udp), io(std::make_shared<IoState>(context)), decoder(dictionary)
  {
    const auto local = socketEndpoint(options.local);
    io->socket.open(local.protocol());
    io->socket.set_option(boost::asio::socket_base::broadcast(options.broadcast));
    io->socket.bind(local);
  }

  UdpTransport::~UdpTransport()
  {
    auto lock = lockReceiver();
    shutdown();
    std::lock_guard ioLock(io->mutex);
    boost::system::error_code ignored;
    io->socket.close(ignored);
  }

  void UdpTransport::start()
  {
    auto lock = lockReceiver();
    if (!activate()) return;
    ++generation;
    try { armReceive(); } catch (...) { deactivate(); throw; }
  }

  void UdpTransport::stop()
  {
    auto lock = lockReceiver();
    deactivate();
    ++generation;
    std::lock_guard ioLock(io->mutex);
    io->socket.cancel();
  }

  void UdpTransport::armReceive()
  {
    std::lock_guard ioLock(io->mutex);
    if (!isRunning() || io->pending) return;
    auto completion = guarded([this, session = generation](const boost::system::error_code &error,
                                                                const BytesBuffer &bytes, const UdpEndpoint &peer) {
      if (session != generation) { armReceive(); return; }
      if (error) {
        stop();
        reportError(std::make_exception_ptr(boost::system::system_error(error)), ReceiveError::Kind::Io);
        return;
      }
      try {
        decoder.pushBytes(bytes);
        while (isRunning()) {
          std::optional<ReceivedMessage> received;
          try { received = decoder.tryReceive(); }
          catch (...) { reportError(std::current_exception(), ReceiveError::Kind::Decode); continue; }
          if (!received) break;
          deliver(received->message, ReceiveInfo{received->frameSize, std::nullopt, peer});
        }
        decoder.discardPendingInput(); // Never combine partial frames from different datagrams/peers.
        if (isRunning()) armReceive();
      } catch (...) { stop(); throw; }
    });
    io->pending = true;
    io->socket.async_receive_from(boost::asio::buffer(io->bytes), io->peer,
      [io = io, completion](const boost::system::error_code &error, size_t size) mutable {
        BytesBuffer bytes;
        UdpEndpoint peer;
        {
          std::lock_guard lock(io->mutex);
          bytes.assign(io->bytes.begin(), io->bytes.begin() + size);
          peer = {io->peer.address().to_string(), io->peer.port()};
          io->pending = false;
        }
        completion(error, bytes, peer);
      });
  }

  size_t UdpTransport::sendMessage(const Message &message, const UdpEndpoint &destination)
  {
    const auto bytes = encodePprzFrame(message);
    auto lock = lockReceiver();
    std::lock_guard ioLock(io->mutex);
    const auto sent = io->socket.send_to(boost::asio::buffer(bytes), socketEndpoint(destination));
    if (sent != bytes.size()) throw std::runtime_error("Incomplete UDP write");
    return sent;
  }

  UdpEndpoint UdpTransport::localEndpoint() const
  {
    auto lock = lockReceiver();
    std::lock_guard ioLock(io->mutex);
    const auto local = io->socket.local_endpoint();
    return {local.address().to_string(), local.port()};
  }
}
