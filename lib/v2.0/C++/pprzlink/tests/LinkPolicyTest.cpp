// SPDX-License-Identifier: GPL-2.0-or-later
#include "TestSupport.h"
#include "AircraftRegistry.h"
#include "Options.h"
#include "XbeeTransmitter.h"
#include <pprzlink/PprzTransport.h>
#include <iostream>

using namespace std::chrono_literals;

namespace {
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
}

int main()
{
  try {
    testRoutedDestinations();
    link_app::AircraftRegistry registry;
    require(!registry.isLive(42, 0), "Disabling expiry must not make unknown aircraft live");
    registry.received(42, 13, 1, false, 1, std::nullopt);
    registry.transmitted(42);
    registry.pinged(42, 1);
    registry.received(42, 8, 1, true, 1.025, std::nullopt);
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
    sender.send(ping, start);
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
    for (int i = 0; i < 255; ++i) sender.send(ping, start);
    expectException<std::runtime_error>([&] { sender.send(ping, start); });
    sender.status({42, 0}, start);
    sender.send(ping, start);
    require(wire.outgoing[4] == 42, "Only released IDs may be reused");
    return 0;
  } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
