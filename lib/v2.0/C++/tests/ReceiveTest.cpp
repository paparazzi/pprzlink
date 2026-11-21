// SPDX-License-Identifier: LGPL-3.0-or-later
#include "TestSupport.h"
#include <pprzlink/PprzTransport.h>
#include <pprzlink/XbeeTransport.h>
#include <iostream>

using namespace pprzlink;

namespace {
  BytesBuffer radioFrame(uint16_t source, uint8_t rssi, uint8_t value)
  {
    BytesBuffer bytes{0x7e, 0, 10, 0x81, static_cast<uint8_t>(source >> 8),
      static_cast<uint8_t>(source), rssi, 0, 42, 0, 1, 1, value};
    uint8_t sum = 0xff;
    for (size_t i = 3; i < bytes.size(); ++i) sum -= bytes[i];
    bytes.push_back(sum);
    return bytes;
  }
}

int main()
{
  try {
    tinyxml2::XMLDocument xml;
    xml.Parse("<protocol><msg_class name='telemetry' id='1'><message name='VALUE' id='1'><field name='value' type='uint8'/></message></msg_class></protocol>");
    MessageDictionary dictionary(xml.RootElement());
    auto device = std::make_unique<MemoryDevice>();
    auto &wire = *device;
    XbeeTransport radio(std::move(device), dictionary);
    ReceiptQueue inbox(radio);
    require(!inbox.receive(), "No message yet is not an error");
    wire.incoming = radioFrame(0x1234, 60, 7);
    const auto secondFrame = radioFrame(0x5678, 80, 8);
    wire.incoming.insert(wire.incoming.end(), secondFrame.begin(), secondFrame.end());
    const auto first = inbox.receive();
    const auto second = inbox.receive();
    require(first && second && first->message.getField<uint8_t>("value") == 7 &&
            second->message.getField<uint8_t>("value") == 8, "Both concatenated messages received");
    require(first->xbee && first->xbee->sourceAddress == 0x1234 && first->xbee->rssi == 60 &&
            second->xbee && second->xbee->sourceAddress == 0x5678 && second->xbee->rssi == 80,
            "Metadata remains associated with each retained message");
    require(first->frameSize == 14 && second->frameSize == 14 && !first->udpPeer, "Complete frame size and transport kind");
    require(!inbox.receive(), "Queue drained");
    wire.incoming = radioFrame(0x9876, 50, 9);
    require(inbox.getMessage()->getField<uint8_t>("value") == 9, "A subsequent source notification delivers the next message");
    require(first->xbee->sourceAddress == 0x1234, "Legacy receive does not overwrite retained metadata");

    Message value(dictionary.getDefinition("VALUE"));
    value.setField("value", 23);
    auto serialDevice = std::make_unique<MemoryDevice>();
    serialDevice->incoming = encodePprzFrame(value);
    PprzTransport serial(std::move(serialDevice), dictionary);
    ReceiptQueue serialInbox(serial);
    const auto received = serialInbox.receive();
    require(received && received->frameSize == 9 && !received->xbee && !received->udpPeer,
            "Same receipt API for ordinary serial frames");
    PprzFrameDecoder decoder(dictionary);
    decoder.pushBytes(encodePprzFrame(value));
    require(decoder.tryReceive()->message.getFieldAs<int>("value") == 23, "Same receipt API without I/O");
    return 0;
  } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
