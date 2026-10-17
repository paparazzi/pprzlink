// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <pprzlink/XbeeTransport.h>
#include <array>
#include <chrono>
#include <optional>
#include <random>

namespace link_app {
  /// Own outstanding IDs and host retries after XML link routing has chosen a destination.
  /// Unicast uses normal radio options and the configured maximum host attempts.
  /// Broadcast requests no MAC ACK and is never resent by this application.
  class XbeeTransmitter {
  public:
    using Clock = std::chrono::steady_clock;
    enum class SendResult { Sent, NotReady, Saturated };
    XbeeTransmitter(pprzlink::XbeeTransport &transport, int maximumAttempts);
    /// Suppressed sends are not queued; initialization or ID saturation can recover later.
    /// Device write errors still propagate to the caller.
    SendResult send(const pprzlink::Message &message, Clock::time_point now = Clock::now());
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
