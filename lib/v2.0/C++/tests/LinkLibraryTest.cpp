// SPDX-License-Identifier: LGPL-3.0-or-later
#include "TestSupport.h"
#include <pprzlink/IvyMessageCodec.h>
#include <pprzlink/PprzFrameCodec.h>
#include <pprzlink/XbeeTransport.h>
#include <iostream>
#include <limits>

using namespace pprzlink;

namespace {
  const char *xml = R"(<protocol><msg_class name="telemetry" id="1">
    <message name="VALUE" id="42" link="forwarded"><field name="value" type="float" format="%.2f"/></message>
    <message name="WIDE" id="43"><field name="signed" type="int64"/><field name="unsigned" type="uint64"/>
      <field name="array" type="int64[]"/></message>
    <message name="EMPTY" id="44"/>
  </msg_class></protocol>)";

  BytesBuffer xbeeFrame(BytesBuffer payload)
  {
    BytesBuffer frame{0x7e, 0, static_cast<uint8_t>(payload.size())};
    uint8_t checksum = 0xff;
    for (auto byte : payload) checksum -= byte;
    frame.insert(frame.end(), payload.begin(), payload.end());
    frame.push_back(checksum);
    return frame;
  }

  void formatsAndMetadata(const MessageDictionary &dictionary)
  {
    const auto &definition = dictionary.getDefinition("VALUE");
    require(definition.getLinkMode() == MessageDefinition::LinkMode::Forwarded, "XML link mode");
    require(definition.getField(0).getFormat() == "%.2f", "XML number format");
    Message message(definition);
    message.setSenderId(uint8_t{42});
    message.addField("value", 1.25f);
    require(ivy_codec::serializeLegacyMessage(message) == "42 VALUE 1.25", "OCaml format");
    require(ivy_codec::serializeMessage(message) == "42 VALUE 1.25", "Compact modern Ivy codec");
    auto parsed = ivy_codec::parseLegacyMessageBody(definition, "42", "VALUE   1.25  ");
    require(parsed.getField<float>("value") == 1.25f, "Legacy whitespace");
    tinyxml2::XMLDocument bad;
    bad.Parse("<protocol><msg_class name='t' id='1'><message name='X' id='1' link='unknown'/></msg_class></protocol>");
    expectException<bad_message_file>([&] { MessageDictionary ignored(bad.RootElement()); });
    bad.Parse("<protocol><msg_class name='t' id='1'><message name='X' id='1'><field name='n' type='float' format='%n'/></message></msg_class></protocol>");
    MessageDictionary unsafe(bad.RootElement());
    Message formatted(unsafe.getDefinition("X"));
    formatted.addField("n", 1.f);
    expectException<wrong_message_format>([&] { (void)ivy_codec::serializeLegacyMessage(formatted); });
  }

  void wideIntegers(const MessageDictionary &dictionary)
  {
    const auto &definition = dictionary.getDefinition("WIDE");
    auto message = ivy_codec::parseLegacyMessageBody(definition, "42",
      "WIDE -9223372036854775808 0xffffffffffffffff -1,0,9223372036854775807");
    require(message.getField<int64_t>("signed") == std::numeric_limits<int64_t>::min(), "int64 minimum");
    require(message.getField<uint64_t>("unsigned") == std::numeric_limits<uint64_t>::max(), "uint64 maximum");
    const auto bytes = encodePprzFrame(message);
    PprzFrameDecoder decoder(dictionary);
    decoder.pushBytes(bytes);
    auto decoded = decoder.nextMessage();
    require(decoded && decoded->getField<std::vector<int64_t>>("array") ==
      std::vector<int64_t>{-1, 0, std::numeric_limits<int64_t>::max()}, "Full-width integer array round trip");
    require(ivy_codec::serializeLegacyMessage(*decoded) ==
      "42 WIDE -9223372036854775808 18446744073709551615 -1,0,9223372036854775807", "Full-width text");
    expectException<wrong_message_format>([&] {
      (void)ivy_codec::parseLegacyMessageBody(definition, "42", "WIDE -9223372036854775809 0 0");
    });
  }

  void framingAndStatistics(const MessageDictionary &dictionary)
  {
    Message message(dictionary.getDefinition("VALUE"));
    message.addField("value", 1.25f);
    const auto frame = encodePprzFrame(message);
    PprzFrameDecoder decoder(dictionary);
    decoder.pushBytes(std::span(frame).first(3));
    require(!decoder.nextMessage(), "Fragment must wait");
    decoder.discardPendingInput();
    decoder.pushBytes(std::span(frame).subspan(3));
    require(!decoder.nextMessage(), "Do not combine different datagrams");
    auto bad = frame;
    bad.back() ^= 1;
    decoder.pushBytes(bad);
    decoder.pushBytes(BytesBuffer{0x99, 1});
    decoder.pushBytes(frame);
    require(decoder.nextMessage().has_value(), "Recover after checksum and length errors");
    const auto &statistics = decoder.getStatistics();
    require(statistics.checksumErrors == 1 && statistics.lengthErrors == 1, "Distinct error counters");
    require(statistics.receivedMessages == 1 && statistics.receivedMessageBytes == frame.size(), "RX counters");
    require(decoder.getLastReceivedFrameSize() == frame.size(), "Frame metadata");
  }

  void extendedXbee(const MessageDictionary &dictionary)
  {
    auto device = std::make_unique<MemoryDevice>();
    auto &wire = *device;
    XbeeTransport radio(std::move(device), dictionary, XbeeTransport::Api::Series868);
    ReceiptQueue inbox(radio);
    radio.setSanityChecksEnabled(true);
    Message message(dictionary.getDefinition("EMPTY"));
    message.setReceiverId(42);
    radio.sendMessageWithId(message, 7);
    // Digi 90001020_F: TX 0x10, ID, 64-bit address, FFFE, radius, options, RF data.
    const auto expected = xbeeFrame({0x10, 7, 0, 0, 0, 0, 0, 0, 0, 42, 0xff, 0xfe, 0, 0, 0, 42, 1, 44});
    require(wire.outgoing == expected, "868 TX fixture and caller-owned ID");
    wire.incoming = xbeeFrame({0x90, 0, 0, 0, 0, 0, 0, 0, 42, 0xff, 0xfe, 1, 42, 0, 1, 44});
    require(inbox.getMessage() != nullptr, "868 RX fixture");
    require(radio.getLastReceiveInfo()->sourceAddress == 42 && !radio.getLastReceiveInfo()->hasRssi, "868 metadata");
    require(radio.getLastReceivedFrameSize() == 20 && radio.getStatistics().receivedMessages == 1, "868 RX bytes");
    std::optional<XbeeTransport::TransmitStatus> received;
    radio.setStatusCallback([&](const auto &event) {
      if (const auto status = std::get_if<XbeeTransport::TransmitStatus>(&event)) received = *status;
    });
    wire.incoming = xbeeFrame({0x8b, 7, 0xff, 0xfe, 3, 1, 0});
    require(!inbox.hasMessage(), "TX status is not telemetry");
    require(received && received->frameId == 7 && received->status == 1 && received->retries == 3, "868 TX status");
  }
}

int main()
{
  try {
    tinyxml2::XMLDocument document;
    require(document.Parse(xml) == tinyxml2::XML_SUCCESS, "test XML");
    MessageDictionary dictionary(document.RootElement());
    formatsAndMetadata(dictionary);
    wideIntegers(dictionary);
    framingAndStatistics(dictionary);
    extendedXbee(dictionary);
    return 0;
  } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
