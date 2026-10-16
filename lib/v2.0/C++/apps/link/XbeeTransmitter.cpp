// SPDX-License-Identifier: GPL-2.0-or-later
#include "XbeeTransmitter.h"
#include <iostream>

namespace link_app {
  using namespace std::chrono_literals;

  XbeeTransmitter::XbeeTransmitter(pprzlink::XbeeTransport &transport, int maximumAttempts)
    : transport(transport), maximumAttempts(maximumAttempts) {}

  void XbeeTransmitter::send(const pprzlink::Message &message, Clock::time_point now)
  {
    if (!transport.isReady()) return; // OCaml also suppresses sends during initialization.
    for (unsigned int count = 0; count < 255; ++count) {
      const uint8_t id = nextId;
      nextId = nextId == 255 ? 1 : static_cast<uint8_t>(nextId + 1);
      if (pending[id]) continue;
      transport.sendMessageWithId(message, id, pprzlink::RadioRetries::FromDefinition);
      pending[id] = Pending{message, 1, now + 5s, std::nullopt};
      return;
    }
    throw std::runtime_error("All XBee frame IDs are awaiting a transmit status");
  }

  void XbeeTransmitter::status(const pprzlink::XbeeTransport::TransmitStatus &status, Clock::time_point now)
  {
    auto &entry = pending[status.frameId];
    if (!entry || entry->retryAt) return; // Ignore unknown or duplicate failure indications.
    const bool retryEnabled = entry->message.getDefinition().getRadioRetries() == pprzlink::RadioRetries::Enabled;
    if (retryEnabled && (status.status == 1 || status.status == 0x21) && entry->attempts < maximumAttempts) {
      const auto delay = std::chrono::milliseconds(std::uniform_int_distribution<int>(10, 209)(random));
      entry->retryAt = now + delay;
    } else {
      entry.reset();
    }
  }

  std::optional<XbeeTransmitter::Clock::time_point> XbeeTransmitter::nextDeadline() const
  {
    std::optional<Clock::time_point> deadline;
    for (const auto &entry : pending) if (entry) {
      const auto next = entry->retryAt.value_or(entry->expires);
      if (!deadline || next < *deadline) deadline = next;
    }
    return deadline;
  }

  void XbeeTransmitter::poll(Clock::time_point now)
  {
    for (size_t id = 1; id < pending.size(); ++id) {
      auto &entry = pending[id];
      if (!entry) continue;
      if (entry->retryAt && now >= *entry->retryAt) {
        transport.sendMessageWithId(entry->message, static_cast<uint8_t>(id),
                                    pprzlink::RadioRetries::FromDefinition);
        ++entry->attempts;
        entry->retryAt.reset();
        entry->expires = now + 5s;
      } else if (now >= entry->expires) {
        std::cerr << "XBee transmit status timed out for frame " << id << '\n';
        entry.reset(); // Missing status is not proof that delivery failed: no blind resend.
      }
    }
  }
}
