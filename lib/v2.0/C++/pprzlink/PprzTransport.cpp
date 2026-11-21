// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file PprzTransport.cpp
 * @brief Readiness-driven transport using the shared incremental PPRZ frame codec.
 * @ingroup transports
 *
 * Input notifications drain completed bytes and distribute every complete message; decoder statistics are updated before callbacks and after malformed payloads.
 */

#include "PprzTransport.h"
#include <boost/asio/post.hpp>

namespace pprzlink {
  PprzTransport::PprzTransport(std::unique_ptr<Device> device, const MessageDictionary &dictionary)
    : Transport(std::move(device), dictionary), decoder(dictionary) {}

  PprzTransport::~PprzTransport()
  {
    shutdown();
    try { stop(); } catch (...) {}
  }

  size_t PprzTransport::sendMessage(const Message &message)
  {
    const auto bytes = encodePprzFrame(message);
    device->writeBuffer(bytes);
    return bytes.size();
  }

  void PprzTransport::receiveAvailable()
  {
    decoder.pushBytes(receiveBytes());
    while (isRunning()) {
      std::optional<ReceivedMessage> received;
      try { received = decoder.tryReceive(); }
      catch (...) {
        statistics = decoder.getStatistics();
        reportError(std::current_exception(), ReceiveError::Kind::Decode);
        continue;
      }
      statistics = decoder.getStatistics();
      if (!received) break;
      lastReceivedFrameSize = received->frameSize;
      deliver(received->message, ReceiveInfo{received->frameSize, std::nullopt, std::nullopt});
    }
  }

  void PprzTransport::receptionStarted()
  {
    boost::asio::post(device->getExecutor(), guarded([this] {
      try { receiveAvailable(); } catch (...) { stop(); throw; }
    }));
  }
}
