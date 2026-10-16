#include "TestSupport.h"
#include <pprzlink/XbeeTransport.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

using namespace pprzlink;

namespace {
  template<class Integer>
  concept CanCreateRadioAddress = requires(Integer address) {
    XbeeAddress::network(address);
    XbeeAddress::ieee(address);
  };

  // PPRZLINK payload: sender=42, receiver=7, class=1/component=3, BYTE=123.
  // TX fixtures also checked against common/ocaml/xbee_transport.ml.
  const BytesBuffer tx16{0x7e, 0, 10, 1, 1, 0, 7, 0, 42, 7, 0x31, 1, 123, 0x18};
  const BytesBuffer tx64{0x7e, 0, 16, 0, 2, 0, 0x13, 0xa2, 0, 0x40, 0x52, 0x91, 0xab,
                          0, 42, 7, 0x31, 1, 123, 0x9c};
  const BytesBuffer rx16{0x7e, 0, 10, 0x81, 0x12, 0x34, 0x40, 0, 42, 7, 0x31, 1, 123, 0x1a};
  const BytesBuffer rx64{0x7e, 0, 16, 0x80, 0, 0x13, 0xa2, 0, 0x40, 0x52, 0x91, 0xab,
                          0x40, 2, 42, 7, 0x31, 1, 123, 0xdc};

  BytesBuffer frame(const BytesBuffer &data)
  {
    BytesBuffer result{0x7e, static_cast<uint8_t>(data.size() >> 8), static_cast<uint8_t>(data.size())};
    result.insert(result.end(), data.begin(), data.end());
    uint8_t sum = 0;
    for (const auto byte : data) sum += byte;
    result.push_back(static_cast<uint8_t>(0xff - sum));
    return result;
  }

  void append(BytesBuffer &bytes, const BytesBuffer &suffix)
  {
    bytes.insert(bytes.end(), suffix.begin(), suffix.end());
  }

  void requireByte(ReceiptQueue &inbox, uint8_t value = 123)
  {
    require(inbox.hasMessage() && inbox.hasMessage(), "Callback inbox preserves unconsumed messages");
    const auto message = inbox.getMessage();
    require(message && message->getField<uint8_t>("value") == value, "Decode XBee RF payload");
    require(std::get<uint8_t>(message->getSenderId()) == 42 && message->getReceiverId() == 7 &&
            message->getComponentId() == 3 && message->getClassId() == 1, "Preserve PPRZLINK header");
  }

  void testSending(const MessageDictionary &dictionary)
  {
    auto deviceOwner = std::make_unique<MemoryDevice>();
    auto &device = *deviceOwner;
    XbeeTransport transport(std::move(deviceOwner), dictionary);
    ReceiptQueue inbox(transport);
    Message message(dictionary.getDefinition("BYTE"));
    message.addField("value", 123);
    message.setSenderId(std::string("42"));
    message.setReceiverId(7);
    message.setComponentId(3);
    require(transport.getLastFrameId() == 0, "No transmit ID before first send");
    require(transport.sendMessage(message) == tx16.size() && device.outgoing == tx16,
            "Exact OCaml-compatible TX16 frame, without nested PPRZ envelope");
    require(transport.sendMessageTo64(message, 0x0013a200405291ab) == tx64.size() && device.outgoing == tx64,
            "Exact big-endian TX64 destination");
    require(transport.getLastFrameId() == 2, "Incrementing status correlation IDs");
    transport.sendMessageTo16(message, 0x1234);
    require(device.outgoing[5] == 0x12 && device.outgoing[6] == 0x34 && device.outgoing[9] == 7,
            "Radio address override preserves PPRZLINK receiver");
    message.setReceiverId(255);
    transport.sendMessage(message);
    require(device.outgoing[5] == 255 && device.outgoing[6] == 255 && device.outgoing[9] == 255,
            "PPRZLINK broadcast maps to radio broadcast 0xffff");
    for (size_t i = 4; i < 256; ++i) transport.sendMessage(message);
    require(transport.getLastFrameId() == 1, "Transmit IDs skip zero on wraparound");

    const auto previous = device.outgoing;
    for (const auto *sender : {"", "-1", "256", "aircraft"}) {
      message.setSenderId(std::string(sender));
      expectException<wrong_message_format>([&] { transport.sendMessage(message); });
      require(device.outgoing == previous && transport.getLastFrameId() == 1,
              "Invalid sender neither writes nor advances frame ID");
    }
    message.setSenderId(uint8_t{42});
    message.setComponentId(16);
    expectException<wrong_message_format>([&] { transport.sendMessage(message); });
    Message missing(dictionary.getDefinition("BYTE"));
    expectException<field_has_no_value>([&] { transport.sendMessage(missing); });
    require(device.outgoing == previous, "Invalid component or missing value sends nothing");

    Message large(dictionary.getDefinition("BLOB"));
    large.addField("data", std::vector<uint8_t>(95, 0x7e));
    require(transport.sendMessage(large) == 109, "100 RF bytes plus TX16 envelope/header");
    large.addField("data", std::vector<uint8_t>(96, 0));
    expectException<std::length_error>([&] { transport.sendMessage(large); });
    require(device.outgoing.size() == 109, "Reject oversize before serial write");

    class FailingDevice : public MemoryDevice {
      void writeBuffer(const BytesBuffer &) override { throw std::runtime_error("write failure"); }
    };
    XbeeTransport failed(std::make_unique<FailingDevice>(), dictionary);
    large.addField("data", std::vector<uint8_t>{});
    expectException<std::runtime_error>([&] { failed.sendMessage(large); });
    require(failed.getLastFrameId() == 0, "Write failure is not reported as a successful send");
    expectException<std::invalid_argument>([&] { XbeeTransport invalid(nullptr, dictionary); });
  }

  void testAddressFactories()
  {
    constexpr auto network = XbeeAddress::network(0x0100);
    constexpr auto ieee = XbeeAddress::ieee(0x0100);
    static_assert(network.kind() == XbeeAddress::Kind::Network && network.value() == 0x0100);
    static_assert(ieee.kind() == XbeeAddress::Kind::Ieee && ieee.value() == 0x0100);
    static_assert(network != ieee);
    static_assert(network == XbeeAddress::network(0x0100));
    static_assert(XbeeAddress::network(0).value() == 0);
    static_assert(XbeeAddress::network(0xffff).value() == 0xffff);
    static_assert(XbeeAddress::ieee(0xffffffffffffffff).value() == std::numeric_limits<std::uint64_t>::max());
    static_assert(CanCreateRadioAddress<int> && CanCreateRadioAddress<std::uint64_t>);
    static_assert(!CanCreateRadioAddress<bool> && !CanCreateRadioAddress<double>);
    static_assert(!std::is_convertible_v<int, XbeeAddress> && !std::is_default_constructible_v<XbeeAddress>);
    expectException<std::out_of_range>([] { (void)XbeeAddress::network(-1); });
    expectException<std::out_of_range>([] { (void)XbeeAddress::network(0x10000); });
    expectException<std::out_of_range>([] { (void)XbeeAddress::network(0x10000000100); });
    expectException<std::out_of_range>([] { (void)XbeeAddress::ieee(-1); });
  }

  void testTypedAddressSending(const MessageDictionary &dictionary)
  {
    using Retries = XbeeTransport::RadioRetries;
    const BytesBuffer payload{42, 7, 0x31, 1, 123};
    for (const auto api : {XbeeTransport::Api::Legacy802154, XbeeTransport::Api::Series868}) {
      auto owner = std::make_unique<MemoryDevice>();
      auto &device = *owner;
      XbeeTransport transport(std::move(owner), dictionary, api);
      transport.setSanityChecksEnabled(true);
      Message message(dictionary.getDefinition("BYTE"));
      message.setField("value", 123);
      message.setSenderId(42);
      message.setReceiverId(7);
      message.setComponentId(3);
      const bool extended = api == XbeeTransport::Api::Series868;
      uint8_t id = 0;
      const auto check = [&](BytesBuffer header, size_t sent) {
        header.insert(header.end(), payload.begin(), payload.end());
        const auto expected = frame(header);
        require(sent == expected.size() && device.outgoing == expected,
                "Typed radio destination preserves addressing, retry options, PPRZLINK receiver and checksum");
        require(transport.getLastFrameId() == id, "Typed send preserves automatic transmit IDs");
      };
      const auto checkIeee = [&](const BytesBuffer &addressBytes, uint8_t options, size_t sent) {
        BytesBuffer header{extended ? uint8_t{0x10} : uint8_t{0x00}, ++id};
        append(header, addressBytes);
        if (extended) append(header, {0xff, 0xfe, 0});
        header.push_back(options);
        check(std::move(header), sent);
      };

      for (const auto retries : {Retries::Enabled, Retries::Disabled}) {
        const uint8_t options = retries == Retries::Disabled ? 1 : 0;
        if (extended) {
          const auto previous = device.outgoing;
          expectException<std::invalid_argument>([&] {
            transport.sendMessageTo(message, XbeeAddress::network(0x0100), retries);
          });
          require(device.outgoing == previous && transport.getLastFrameId() == id,
                  "Series868 rejects network addressing before writing or consuming a frame ID");
        } else {
          const auto sent = transport.sendMessageTo(message, XbeeAddress::network(0x0100), retries);
          check({0x01, ++id, 0x01, 0x00, options}, sent);
        }
        auto sent = transport.sendMessageTo(message, XbeeAddress::ieee(0x0100), retries);
        checkIeee({0, 0, 0, 0, 0, 0, 1, 0}, options, sent);
        sent = transport.sendMessageTo(message, XbeeAddress::ieee(0x0013a200405291ab), retries);
        checkIeee({0, 0x13, 0xa2, 0, 0x40, 0x52, 0x91, 0xab}, options, sent);
        sent = transport.sendMessageTo(message, XbeeAddress::ieee(0xffffffffffffffff), retries);
        checkIeee({0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, options, sent);
      }

      const auto destination = extended ? XbeeAddress::ieee(0x0100) : XbeeAddress::network(0x0100);
      const auto sent = transport.sendMessageTo(message, destination);
      if (extended) checkIeee({0, 0, 0, 0, 0, 0, 1, 0}, 0, sent);
      else check({0x01, ++id, 0x01, 0x00, 0}, sent);
      const auto previous = device.outgoing;
      expectException<std::out_of_range>([&] {
        transport.sendMessageTo(message, XbeeAddress::network(0x10000));
      });
      expectException<std::out_of_range>([&] {
        transport.sendMessageTo(message, XbeeAddress::ieee(-1));
      });
      expectException<std::invalid_argument>([&] {
        transport.sendMessageTo(message, destination, static_cast<Retries>(99));
      });
      require(device.outgoing == previous && transport.getLastFrameId() == id,
              "Invalid typed destinations/retry enums never reach the wire or consume an ID");
    }
  }

  void testRadioRetries(const MessageDictionary &dictionary)
  {
    using Retries = XbeeTransport::RadioRetries;
    const BytesBuffer payload{42, 7, 0x31, 1, 123};
    for (const auto api : {XbeeTransport::Api::Legacy802154, XbeeTransport::Api::Series868}) {
      auto owner = std::make_unique<MemoryDevice>();
      auto &device = *owner;
      XbeeTransport transport(std::move(owner), dictionary, api);
      transport.setSanityChecksEnabled(true);
      Message message(dictionary.getDefinition("BYTE"));
      message.setField("value", 123);
      message.setSenderId(42);
      message.setReceiverId(7);
      message.setComponentId(3);
      const bool extended = api == XbeeTransport::Api::Series868;
      const uint8_t implicitType = extended ? 0x10 : 0x01;
      const uint8_t explicit64Type = extended ? 0x10 : 0x00;
      uint8_t id = 0;
      const auto check = [&](uint8_t options, uint8_t expectedId, uint8_t type) {
        const size_t optionsOffset = type == 0x01 ? 7 : (type == 0x00 ? 13 : 16);
        require(device.outgoing.size() == optionsOffset + payload.size() + 2 &&
                device.outgoing[0] == 0x7e && device.outgoing[3] == type,
                "Retry choice preserves the selected XBee frame family and payload size");
        require(device.outgoing[optionsOffset] == options, "Per-frame radio retry option at the documented wire offset");
        require(device.outgoing[4] == expectedId && transport.getLastFrameId() == expectedId,
                "Radio retry choice preserves the requested/automatic status identifier");
        require(std::equal(payload.begin(), payload.end(), device.outgoing.begin() + optionsOffset + 1),
                "Radio retry choice does not alter the PPRZLINK message");
        transport.validateTransmitFrame(device.outgoing); // Includes the checksum.
      };

      transport.sendMessage(message, Retries::Disabled);
      check(1, ++id, implicitType);
      if (!extended) {
        const BytesBuffer disabledTx16{0x7e, 0, 10, 1, 1, 0, 7, 1, 42, 7, 0x31, 1, 123, 0x17};
        require(device.outgoing == disabledTx16, "Exact TX16 fixture with Disable MAC ACK and corrected checksum");
      }
      transport.sendMessage(message);
      check(0, ++id, implicitType);
      transport.sendMessage(message, Retries::Enabled);
      check(0, ++id, implicitType);
      Transport &polymorphic = transport;
      polymorphic.sendMessage(message);
      check(0, ++id, implicitType);

      transport.sendMessageTo64(message, 0x0013a200405291ab, Retries::Disabled);
      check(1, ++id, explicit64Type);
      transport.sendMessageTo64(message, 0x0013a200405291ab);
      check(0, ++id, explicit64Type);
      transport.sendMessageTo64(message, 0x0013a200405291ab, Retries::Enabled);
      check(0, ++id, explicit64Type);
      if (!extended) {
        transport.sendMessageTo16(message, 0x1234, Retries::Disabled);
        check(1, ++id, 0x01);
        transport.sendMessageTo16(message, 0x1234);
        check(0, ++id, 0x01);
        transport.sendMessageTo16(message, 0x1234, Retries::Enabled);
        check(0, ++id, 0x01);
      }

      transport.sendMessageWithId(message, 77, Retries::Disabled);
      check(1, 77, implicitType);
      bool receivedStatus = false;
      transport.setStatusCallback([&](const XbeeTransport::RadioStatus &event) {
        const auto *status = std::get_if<XbeeTransport::TransmitStatus>(&event);
        receivedStatus = status && status->frameId == 77 && status->status == 0;
      });
      ReceiptQueue inbox(transport);
      device.incoming = frame(extended ? BytesBuffer{0x8b, 77, 0xff, 0xfe, 0, 0, 0}
                                       : BytesBuffer{0x89, 77, 0});
      require(!inbox.hasMessage() && receivedStatus, "Disabling radio retries does not suppress transmit-status callbacks");

      transport.sendMessageWithId(message, 78, Retries::Enabled);
      check(0, 78, implicitType);
      transport.sendMessageWithId(message, 79);
      check(0, 79, implicitType);
      transport.sendMessageWithId(message, 0, Retries::Disabled);
      check(1, 0, implicitType);
      transport.sendMessageWithId(message, 0, Retries::Enabled);
      check(0, 0, implicitType);

      const auto previous = device.outgoing;
      const auto invalid = static_cast<Retries>(99);
      expectException<std::invalid_argument>([&] { transport.sendMessage(message, invalid); });
      expectException<std::invalid_argument>([&] { transport.sendMessageWithId(message, 5, invalid); });
      expectException<std::invalid_argument>([&] { transport.sendMessageTo64(message, 7, invalid); });
      if (!extended)
        expectException<std::invalid_argument>([&] { transport.sendMessageTo16(message, 7, invalid); });
      require(device.outgoing == previous && transport.getLastFrameId() == 0,
              "An invalid retry enum never writes or advances the frame ID");
    }
  }

  void testReception(const MessageDictionary &dictionary)
  {
    for (const auto &fixture : {rx16, rx64}) {
      for (size_t split = 0; split <= fixture.size(); ++split) {
        auto deviceOwner = std::make_unique<MemoryDevice>();
        auto &device = *deviceOwner;
        XbeeTransport transport(std::move(deviceOwner), dictionary);
        ReceiptQueue inbox(transport);
        require(!transport.getLastReceiveInfo(), "No metadata before first RX");
        device.incoming.assign(fixture.begin(), fixture.begin() + split);
        require(inbox.hasMessage() == (split == fixture.size()), "Fragmented XBee frame completeness");
        device.incoming.assign(fixture.begin() + split, fixture.end());
        requireByte(inbox);
        const auto &info = *transport.getLastReceiveInfo();
        const bool is64 = fixture[3] == 0x80;
        require(info.addressIs64Bit == is64 && info.rssi == 64 && info.options == (is64 ? 2 : 0) &&
                info.sourceAddress == (is64 ? uint64_t{0x0013a200405291ab} : 0x1234), "Radio RX metadata");
        require(!inbox.getMessage(), "Exactly one message");
      }
    }
    auto deviceOwner = std::make_unique<MemoryDevice>();
    auto &device = *deviceOwner;
    XbeeTransport transport(std::move(deviceOwner), dictionary);
    ReceiptQueue inbox(transport);
    for (const auto byte : rx64) {
      device.incoming = {byte};
      inbox.hasMessage();
    }
    requireByte(inbox);
    device.incoming = rx16;
    append(device.incoming, rx64);
    requireByte(inbox);
    requireByte(inbox);
    require(!inbox.hasMessage(), "Concatenated frames drained");

    // In AP=1 all payload bytes, including 0x7e and 0x7d, are literal.
    device.incoming = frame({0x81, 0, 42, 30, 0, 42, 7, 0x31, 1, 0x7e});
    requireByte(inbox, 0x7e);
    BytesBuffer data{0x80, 0, 0, 0, 0, 0, 0, 0, 42, 50, 0, 42, 7, 0x31, 3, 95};
    data.insert(data.end(), 95, 0x7d);
    device.incoming = frame(data);
    require(inbox.getMessage()->getField<std::vector<uint8_t>>("data").size() == 95,
            "Maximum RX64 frame and literal escape byte");
  }

  void testCorruption(const MessageDictionary &dictionary)
  {
    auto deviceOwner = std::make_unique<MemoryDevice>();
    auto &device = *deviceOwner;
    XbeeTransport transport(std::move(deviceOwner), dictionary);
    ReceiptQueue inbox(transport);
    for (size_t i = 0; i < 20000; ++i) append(device.incoming, {0x55, 0x7e, 0xff, 0xff});
    append(device.incoming, {0x7e, 0, 0});
    auto bad = rx16;
    bad.back() ^= 1;
    append(device.incoming, bad);
    append(device.incoming, rx64);
    requireByte(inbox);

    for (const BytesBuffer &malformed : {BytesBuffer{0x81}, BytesBuffer{0x80, 1, 2},
         BytesBuffer{0x89, 1}, BytesBuffer{0x89, 1, 0, 0}, BytesBuffer{0x8a}, BytesBuffer{0x88, 1, 'A'}}) {
      device.incoming = frame(malformed);
      append(device.incoming, rx16);
      expectException<wrong_message_format>([&] { inbox.hasMessage(); });
      requireByte(inbox);
    }
    // Valid API checksum with an unknown PPRZLINK message, truncated fields, or trailing bytes.
    device.incoming = frame({0x81, 0, 42, 50, 0, 42, 7, 0x31, 200, 123});
    append(device.incoming, rx16);
    expectException<no_such_message>([&] { inbox.getMessage(); });
    requireByte(inbox);
    device.incoming = frame({0x81, 0, 42, 50, 0, 42, 7, 0x31, 2, 123});
    append(device.incoming, rx16);
    expectException<std::out_of_range>([&] { inbox.getMessage(); });
    requireByte(inbox);
    device.incoming = frame({0x81, 0, 42, 50, 0, 42, 7, 0x31, 1, 123, 10});
    append(device.incoming, rx16);
    expectException<wrong_message_format>([&] { inbox.getMessage(); });
    requireByte(inbox);

    // Discard a complete unhandled API indication, even if its bytes contain a valid RX frame.
    BytesBuffer unknown{0x82};
    append(unknown, rx16);
    device.incoming = frame(unknown);
    require(!inbox.hasMessage(), "Unknown API frame is skipped atomically");
  }

  void testSanity(const MessageDictionary &dictionary)
  {
    auto deviceOwner = std::make_unique<MemoryDevice>();
    auto &device = *deviceOwner;
    XbeeTransport transport(std::move(deviceOwner), dictionary);
    ReceiptQueue inbox(transport);
    transport.validateTransmitFrame(tx16);
    transport.validateTransmitFrame(tx64);
    require(device.outgoing.empty(), "Validation alone performs no I/O");

    const auto reject = [&](const BytesBuffer &bytes, const std::string &reason) {
      const auto error = expectException<wrong_message_format>([&] { transport.validateTransmitFrame(bytes); });
      require(error.starts_with("XBee sanity check:") && error.find(reason) != std::string::npos,
              "Sanity error identifies the failure: " + error);
      require(device.outgoing.empty(), "Invalid frame does not reach the device");
    };
    for (size_t size = 0; size < tx16.size(); ++size) {
      reject(BytesBuffer(tx16.begin(), tx16.begin() + size), "");
    }
    auto invalid = tx16;
    invalid[0] = 0x99;
    reject(invalid, "delimiter");
    invalid = tx16;
    ++invalid[2];
    reject(invalid, "length");
    invalid = tx16;
    invalid.back() ^= 1;
    reject(invalid, "checksum");
    reject(frame({0x81, 1, 0, 7, 0, 42, 7, 0x31, 1, 123}), "frame type");
    reject(frame({0x01, 1, 0}), "header");
    reject(frame({0x00, 1, 0, 7, 0}), "header");
    reject(frame({0x01, 1, 0, 7, 0, 42, 7, 0x31}), "RF payload");
    reject(frame({0x01, 1, 0, 7, 2, 42, 7, 0x31, 1, 123}), "option bits");
    reject(frame({0x01, 1, 0, 7, 0, 42, 7, 0x31, 200, 123}), "XML");
    reject(frame({0x01, 1, 0, 7, 0, 42, 7, 0x31, 2, 123}), "XML");
    reject(frame({0x01, 1, 0, 7, 0, 42, 7, 0x31, 1, 123, 0}), "XML");
    BytesBuffer large{0x01, 1, 0, 7, 0, 42, 7, 0x31, 3, 96};
    large.insert(large.end(), 96, 0);
    reject(frame(large), "RF payload");
    transport.validateTransmitFrame(frame({0x01, 0, 0xff, 0xff, 5, 42, 7, 0x31, 1, 0x7e}));

    // A Message from a different XML definition can be encoded correctly but
    // still violate this transport's dictionary. Verify the pre-write gate itself.
    tinyxml2::XMLDocument otherXml;
    otherXml.Parse(R"(<protocol><msg_class name="test" id="1">
      <message name="BYTE" id="1"><field name="value" type="uint32"/></message>
    </msg_class></protocol>)");
    MessageDictionary other(otherXml.RootElement());
    Message mismatch(other.getDefinition("BYTE"));
    mismatch.addField("value", 123);
    transport.setSanityChecksEnabled(true);
    const auto error = expectException<wrong_message_format>([&] { transport.sendMessage(mismatch); });
    require(error.find("XML") != std::string::npos && device.outgoing.empty() && transport.getLastFrameId() == 0,
            "Sanity failure blocks the real send before any serial bytes or frame ID change");
    Message valid(dictionary.getDefinition("BYTE"));
    valid.addField("value", 123);
    valid.setSenderId(uint8_t{42});
    valid.setReceiverId(7);
    valid.setComponentId(3);
    transport.sendMessage(valid);
    require(device.outgoing == tx16, "Sanity mode preserves valid wire bytes");
    transport.sendMessageTo64(valid, 0x0013a200405291ab);
    require(device.outgoing == tx64, "Sanity mode also validates TX64");
  }

  void testStatuses(const MessageDictionary &dictionary)
  {
    auto deviceOwner = std::make_unique<MemoryDevice>();
    auto &device = *deviceOwner;
    XbeeTransport transport(std::move(deviceOwner), dictionary);
    ReceiptQueue inbox(transport);
    std::vector<XbeeTransport::RadioStatus> events;
    transport.setStatusCallback([&](const auto &event) { events.push_back(event); });
    // Digi's published transmit-status fixture (frame 0x52 delivered successfully).
    device.incoming = {0x7e, 0, 3, 0x89, 0x52, 0, 0x24};
    append(device.incoming, frame({0x89, 5, 1}));
    append(device.incoming, frame({0x8a, 0}));
    append(device.incoming, frame({0x88, 6, 'M', 'Y', 0, 0x01, 0x00}));
    append(device.incoming, rx16);
    requireByte(inbox);
    require(events.size() == 4, "Radio status frames processed before next PPRZLINK message");
    require(std::get<XbeeTransport::TransmitStatus>(events[0]).frameId == 0x52 &&
            std::get<XbeeTransport::TransmitStatus>(events[0]).status == 0, "TX success event");
    require(std::get<XbeeTransport::TransmitStatus>(events[1]).status == 1, "No-ACK event");
    require(std::get<XbeeTransport::ModemStatus>(events[2]).status == 0, "Modem event");
    const auto &at = std::get<XbeeTransport::AtCommandResponse>(events[3]);
    require(at.frameId == 6 && at.command == std::array<char, 2>{'M', 'Y'} && at.status == 0 &&
            at.value == BytesBuffer{1, 0}, "AT response fields");
    device.incoming = frame({0x89, 1, 2});
    require(!inbox.hasMessage() && events.size() == 5, "Statuses alone do not produce messages");

    transport.setStatusCallback([](const auto &) { throw std::runtime_error("callback failure"); });
    device.incoming = frame({0x8a, 1});
    append(device.incoming, rx16);
    expectException<std::runtime_error>([&] { inbox.hasMessage(); });
    transport.start();
    requireByte(inbox);
    transport.setStatusCallback({});
    device.incoming = frame({0x8a, 1});
    require(!inbox.hasMessage(), "Callback is optional");
  }
}

int main()
{
  try {
    tinyxml2::XMLDocument xml;
    xml.Parse(R"(<protocol><msg_class name="test" id="1">
      <message name="BYTE" id="1"><field name="value" type="uint8"/></message>
      <message name="WORD" id="2"><field name="value" type="uint32"/></message>
      <message name="BLOB" id="3"><field name="data" type="uint8[]"/></message>
    </msg_class></protocol>)");
    MessageDictionary dictionary(xml.RootElement());
    testSending(dictionary);
    testAddressFactories();
    testTypedAddressSending(dictionary);
    testRadioRetries(dictionary);
    testReception(dictionary);
    testCorruption(dictionary);
    testStatuses(dictionary);
    testSanity(dictionary);
    std::cout << "XBee AP=1 addressing, wire fixtures, RX, statuses and corruption recovery passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
