// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <pprzlink/XbeeTransport.h>
#include <array>
#include <chrono>
#include <optional>
#include <random>

namespace link_app {
  /// Own outstanding IDs and host retries, honoring the message's radio_retry hint.
  /// Disabled suppresses radio ACK/retries and host resends. Enabled uses the
  /// modem's normal radio behavior and the configured maximum host attempts.
  class XbeeTransmitter {
  public:
    using Clock = std::chrono::steady_clock;
    XbeeTransmitter(pprzlink::XbeeTransport &transport, int maximumAttempts);
    void send(const pprzlink::Message &message, Clock::time_point now = Clock::now());
    void status(const pprzlink::XbeeTransport::TransmitStatus &status, Clock::time_point now = Clock::now());
    std::optional<Clock::time_point> nextDeadline() const;
    void poll(Clock::time_point now = Clock::now());

  private:
    struct Pending {
      pprzlink::Message message;
      int attempts = 1;
      Clock::time_point expires;
      std::optional<Clock::time_point> retryAt;
    };
    pprzlink::XbeeTransport &transport;
    int maximumAttempts;
    uint8_t nextId = 1;
    std::array<std::optional<Pending>, 256> pending;
    std::mt19937 random{std::random_device{}()};
  };
}
