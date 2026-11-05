// SPDX-License-Identifier: GPL-2.0-or-later
#include "TestSupport.h"
#include "AircraftRegistry.h"
#include "Options.h"
#include "XbeeTransmitter.h"
#include <pprzlink/PprzTransport.h>
#include <iostream>
#include <system_error>

using namespace std::chrono_literals;
using SendResult = link_app::XbeeTransmitter::SendResult;

namespace {
  void testRoundTripTime()
  {
    using Registry = link_app::AircraftRegistry;
    const auto start = Registry::TimePoint{};
    Registry registry;
    const auto reportHasRtt = [&](std::string_view value) {
      return registry.reports(-1, 1000, 0).front().ends_with(value);
    };

    registry.received(42, 13, 0, false, start, std::nullopt);
    require(reportHasRtt(" 9999.00"), "RTT is unknown until a PING/PONG completes");
    registry.received(42, 8, 0, true, start, std::nullopt);
    require(reportHasRtt(" 9999.00"), "An unsolicited PONG cannot create a measurement");

    registry.pinged(42, start);
    require(reportHasRtt(" 9999.00"), "The first pending PING has no RTT yet");
    registry.received(42, 13, 0, false, start + 20ms, std::nullopt);
    require(reportHasRtt(" 9999.00"), "Ordinary telemetry does not complete a PING");
    registry.received(42, 8, 0, true, start + 25ms, std::nullopt);
    require(reportHasRtt(" 25.00"), "A PONG records the completed monotonic RTT");
    registry.received(42, 8, 0, true, start + 100ms, std::nullopt);
    require(reportHasRtt(" 25.00"), "A duplicate PONG cannot change the last completed RTT");

    registry.pinged(42, start + 1s);
    require(reportHasRtt(" 25.00"), "A new pending PING preserves the last completed RTT");
    registry.pinged(42, start + 2s);
    require(reportHasRtt(" 25.00"), "A missing PONG preserves the last completed RTT");
    registry.received(42, 8, 0, true, start + 2080ms, std::nullopt);
    require(reportHasRtt(" 80.00"), "The next PONG measures from the latest accepted PING");

    registry.received(43, 8, 0, true, start + 3s, std::nullopt);
    require(registry.reports(-1, 1000, 3).back().ends_with(" 9999.00"),
            "Another aircraft cannot reuse a completed RTT");
  }

  void testUnreadyRadio()
  {
    tinyxml2::XMLDocument xml;
    xml.Parse("<protocol><msg_class name='datalink' id='2'><message name='PING' id='8'/></msg_class></protocol>");
    pprzlink::MessageDictionary dictionary(xml.RootElement());
    auto device = std::make_unique<MemoryDevice>();
    auto &wire = *device;
    pprzlink::XbeeTransport radio(std::move(device), dictionary);
    radio.startInitialization({});
    link_app::XbeeTransmitter sender(radio, 3);
    pprzlink::Message ping(dictionary.getDefinition("PING"));
    ping.setReceiverId(42);
    require(sender.send(ping) == SendResult::NotReady,
            "A radio still initializing reports that no PING was sent");
    require(wire.outgoing.empty() && !sender.nextDeadline(), "A suppressed PING writes nothing and cannot retry");
  }

  void testRoutedDestinations()
  {
    for (const auto *attribute : {"", " link='forwarded'", " link='broadcasted'"}) {
      tinyxml2::XMLDocument xml;
      const auto text = std::string("<protocol><msg_class name='datalink' id='2'>") +
        "<message name='PING' id='8'" + attribute + "/></msg_class></protocol>";
      require(xml.Parse(text.c_str()) == tinyxml2::XML_SUCCESS, "Routing fixture XML");
      pprzlink::MessageDictionary dictionary(xml.RootElement());
      // Routing has already chosen the receiver before handing a message to the
      // transmitter. The actual destination governs retries, including overrides.
      for (const uint8_t receiver : {uint8_t{42}, uint8_t{255}}) {
        pprzlink::Message message(dictionary.getDefinition("PING"));
        message.setReceiverId(receiver);
        const bool enabled = receiver != 255;
        for (const auto api : {pprzlink::XbeeTransport::Api::Legacy802154, pprzlink::XbeeTransport::Api::Series868}) {
          auto device = std::make_unique<MemoryDevice>();
          auto &wire = *device;
          pprzlink::XbeeTransport radio(std::move(device), dictionary, api);
          link_app::XbeeTransmitter sender(radio, 3);
          const auto start = link_app::XbeeTransmitter::Clock::now();
          sender.send(message, start);
          const auto original = wire.outgoing;
          const bool extended = api == pprzlink::XbeeTransport::Api::Series868;
          require(original[extended ? 16 : 7] == 0,
                  "link++ preserves normal legacy transmit options; broadcast inherently has no MAC ACK");
          const size_t addressEnd = extended ? 12 : 6;
          require(original[addressEnd - 1] == (enabled ? 0 : 255) &&
                  original[addressEnd] == (enabled ? 42 : 255),
                  "The selected unicast/broadcast destination reaches the radio frame");
          const uint8_t id = original[4];
          require(id != 0, "Broadcast messages still request a local transmit status");
          const uint8_t failure = extended ? 0x21 : 1;
          sender.status({id, failure}, start);
          wire.outgoing.clear();
          if (!enabled) {
            require(!sender.nextDeadline(), "Broadcast failure releases the pending transmission");
            sender.poll(start + 1s);
            require(wire.outgoing.empty(), "A broadcast is never resent, regardless of the CLI limit");
            for (int i = 0; i < 300; ++i) {
              sender.send(message, start);
              sender.status({wire.outgoing[4], failure}, start);
            }
            require(!sender.nextDeadline(), "Broadcast failures release frame IDs across wraparound");
            continue;
          }
          sender.poll(start + 250ms);
          require(wire.outgoing == original, "Unicast retries preserve the complete frame and ID");
          sender.status({id, failure}, start + 250ms);
          wire.outgoing.clear();
          sender.poll(start + 500ms);
          require(wire.outgoing == original, "Unicast uses the configured third attempt");
          sender.status({id, failure}, start + 500ms);
          wire.outgoing.clear();
          sender.poll(start + 1s);
          require(wire.outgoing.empty() && !sender.nextDeadline(),
                  "The CLI maximum attempt count bounds unicast retries");
        }
      }
    }
  }

  void testSaturationRecovery()
  {
    tinyxml2::XMLDocument xml;
    xml.Parse("<protocol><msg_class name='datalink' id='2'><message name='PING' id='8'/></msg_class></protocol>");
    pprzlink::MessageDictionary dictionary(xml.RootElement());
    auto device = std::make_unique<MemoryDevice>();
    auto &wire = *device;
    pprzlink::XbeeTransport radio(std::move(device), dictionary);
    link_app::XbeeTransmitter sender(radio, 2);
    pprzlink::Message ping(dictionary.getDefinition("PING"));
    ping.setReceiverId(42);
    const auto start = link_app::XbeeTransmitter::Clock::now();
    for (int id = 1; id <= 255; ++id) {
      require(sender.send(ping, start) == SendResult::Sent, "All 255 IDs are initially available");
      require(wire.outgoing[4] == id, "An outstanding ID must not be overwritten");
    }
    wire.outgoing.clear();
    require(sender.send(ping, start) == SendResult::Saturated, "Saturation is a recoverable send result");
    require(wire.outgoing.empty(), "A saturated send writes nothing");
    require(sender.nextDeadline() == start + 5s, "Saturation preserves the expiry deadline");

    sender.status({42, 0}, start + 1s);
    require(sender.send(ping, start + 1s) == SendResult::Sent, "A status makes an ID available again");
    require(wire.outgoing[4] == 42, "Only the released ID may be reused");
    wire.outgoing.clear();
    require(sender.send(ping, start + 1s) == SendResult::Saturated, "The reused ID is reserved again");
    sender.poll(start + 5s);
    require(wire.outgoing.empty(), "Expiry never resends a command with an unknown delivery outcome");
    require(sender.nextDeadline() == start + 6s, "The newer transmission retains its own expiry");
    require(sender.send(ping, start + 5s) == SendResult::Sent, "Expired IDs allow subsequent sends");
    require(wire.outgoing[4] != 42, "A still-outstanding ID remains reserved after other IDs expire");
  }

  void testWriteFailures()
  {
    class FailingDevice : public MemoryDevice {
    public:
      bool fail = true;
      void writeBuffer(const pprzlink::BytesBuffer &bytes) override
      {
        if (fail) throw std::system_error(std::make_error_code(std::errc::io_error));
        MemoryDevice::writeBuffer(bytes);
      }
    };
    tinyxml2::XMLDocument xml;
    xml.Parse("<protocol><msg_class name='datalink' id='2'><message name='PING' id='8'/></msg_class></protocol>");
    pprzlink::MessageDictionary dictionary(xml.RootElement());
    auto device = std::make_unique<FailingDevice>();
    auto &wire = *device;
    pprzlink::XbeeTransport radio(std::move(device), dictionary);
    link_app::XbeeTransmitter sender(radio, 2);
    pprzlink::Message ping(dictionary.getDefinition("PING"));
    ping.setReceiverId(42);
    const auto start = link_app::XbeeTransmitter::Clock::now();
    expectException<std::system_error>([&] { sender.send(ping, start); });
    require(!sender.nextDeadline(), "A failed write does not become a pending transmission");
    wire.fail = false;
    require(sender.send(ping, start) == SendResult::Sent, "A later successful write is tracked");
    sender.status({wire.outgoing[4], 1}, start);
    wire.fail = true;
    expectException<std::system_error>([&] { sender.poll(start + 250ms); });
  }
}

int main()
{
  try {
    testRoundTripTime();
    testUnreadyRadio();
    testRoutedDestinations();
    testSaturationRecovery();
    testWriteFailures();
    link_app::AircraftRegistry registry;
    const auto receivedAt = link_app::AircraftRegistry::TimePoint{} + 1s;
    require(!registry.isLive(42, 0), "Disabling expiry must not make unknown aircraft live");
    registry.received(42, 13, 1, false, receivedAt, std::nullopt);
    registry.transmitted(42);
    registry.pinged(42, receivedAt);
    registry.received(42, 8, 1, true, receivedAt + 25ms, std::nullopt);
    auto report = registry.reports(7, 1000, 2);
    require(report == std::vector<std::string>{"link LINK_REPORT 42 7 2 0 21 2 1 21.0 2.0 1 25.00"}, "Report semantics");
    registry.age(1000);
    require(!registry.isLive(42, 1000) && registry.isLive(42, -1), "Exact expiry boundary");
    registry.broadcast();
    require(registry.all().at(42).transmittedMessages == 2, "Broadcast includes expired known aircraft in accounting");

    tinyxml2::XMLDocument xml;
    xml.Parse("<protocol><msg_class name='datalink' id='2'><message name='PING' id='8'/></msg_class></protocol>");
    pprzlink::MessageDictionary dictionary(xml.RootElement());
    auto device = std::make_unique<MemoryDevice>();
    auto &wire = *device;
    pprzlink::XbeeTransport radio(std::move(device), dictionary);
    link_app::XbeeTransmitter sender(radio, 2);
    pprzlink::Message ping(dictionary.getDefinition("PING"));
    ping.setReceiverId(42);
    const auto start = link_app::XbeeTransmitter::Clock::now();
    require(sender.send(ping, start) == SendResult::Sent, "A ready radio accepts the PING");
    const auto original = wire.outgoing;
    sender.status({1, 1}, start);
    sender.status({1, 1}, start); // A duplicated failure does not consume a second attempt.
    wire.outgoing.clear();
    sender.poll(start + 250ms);
    require(wire.outgoing == original, "Retry must preserve frame and ID");
    sender.status({1, 1}, start + 250ms);
    wire.outgoing.clear();
    sender.poll(start + 1s);
    require(wire.outgoing.empty(), "Configured retry limit must be enforced");
    return 0;
  } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
