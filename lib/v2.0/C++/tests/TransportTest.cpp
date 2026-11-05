#include "TestSupport.h"
#include <pprzlink/PprzTransport.h>
#include <pprzlink/XbeeTransport.h>
#include <type_traits>
#include <iostream>
#include <memory>
#include <utility>

using namespace pprzlink;

namespace {
  const BytesBuffer byteFrame{0x99, 9, 42, 7, 0x31, 1, 123, 231, 52};

  void updateChecksum(BytesBuffer &frame)
  {
    frame[1] = static_cast<uint8_t>(frame.size());
    uint8_t a = 0, b = 0;
    for (size_t i = 1; i < frame.size() - 2; ++i) { a += frame[i]; b += a; }
    frame[frame.size() - 2] = a;
    frame.back() = b;
  }

  void append(BytesBuffer &buffer, const BytesBuffer &frame)
  {
    buffer.insert(buffer.end(), frame.begin(), frame.end());
  }

  void requireByte(ReceiptQueue &inbox)
  {
    const auto message = inbox.getMessage();
    require(message && message->getField<uint8_t>("value") == 123, "Recovered byte frame");
    require(!inbox.hasMessage(), "Exactly one complete frame");
  }

  template<class TransportType>
  void testDeviceOwnership(const MessageDictionary &dictionary)
  {
    static_assert(!std::is_constructible_v<TransportType, Device*, const MessageDictionary&>);
    static_assert(!std::is_copy_constructible_v<TransportType>);
    static_assert(!std::is_move_constructible_v<TransportType>);
    static_assert(std::is_same_v<decltype(std::declval<TransportType&>().getDevice()), Device&>);
    static_assert(std::is_same_v<decltype(std::declval<const TransportType&>().getDevice()), const Device&>);

    struct TrackedDevice : MemoryDevice {
      explicit TrackedDevice(int &count) : destructions(count) {}
      ~TrackedDevice() override { ++destructions; }
      int &destructions;
    };

    int destructions = 0;
    {
      auto device = std::make_unique<TrackedDevice>(destructions);
      auto &borrowed = *device;
      std::unique_ptr<Transport> transport =
          std::make_unique<TransportType>(std::move(device), dictionary);
      require(!device && destructions == 0, "Ownership transfers without destroying the device");
      require(&transport->getDevice() == &borrowed &&
              &std::as_const(*transport).getDevice() == &borrowed, "Device references stay valid");
      borrowed.incoming = {0x55}; // The transport can still use the transferred device.
      transport->start();
      require(borrowed.incoming.empty(), "Owned device remains usable");
    }
    require(destructions == 1, "Polymorphic transport destruction releases the device exactly once");

    struct FailingTransport : TransportType {
      FailingTransport(std::unique_ptr<Device> device, const MessageDictionary &dictionary)
        : TransportType(std::move(device), dictionary)
      {
        throw std::runtime_error("Transport initialization failed");
      }
    };
    expectException<std::runtime_error>([&] {
      FailingTransport transport(std::make_unique<TrackedDevice>(destructions), dictionary);
    });
    require(destructions == 2, "Failed transport construction releases its device");
  }

  void testReception(const MessageDictionary &dictionary)
  {
    for (size_t split = 0; split <= byteFrame.size(); ++split) {
      auto deviceOwner = std::make_unique<MemoryDevice>();
      auto &device = *deviceOwner;
      PprzTransport transport(std::move(deviceOwner), dictionary);
      ReceiptQueue inbox(transport);
      device.incoming.assign(byteFrame.begin(), byteFrame.begin() + split);
      require(inbox.hasMessage() == (split == byteFrame.size()), "Fragmented frame completeness");
      device.incoming.assign(byteFrame.begin() + split, byteFrame.end());
      requireByte(inbox);
    }
    for (uint8_t length = 0; length < 8; ++length) {
      auto deviceOwner = std::make_unique<MemoryDevice>();
      auto &device = *deviceOwner;
      PprzTransport transport(std::move(deviceOwner), dictionary);
      ReceiptQueue inbox(transport);
      device.incoming = {0x99, length};
      append(device.incoming, byteFrame);
      requireByte(inbox);
    }
    auto deviceOwner = std::make_unique<MemoryDevice>();
    auto &device = *deviceOwner;
    PprzTransport transport(std::move(deviceOwner), dictionary);
    ReceiptQueue inbox(transport);
    for (int i = 0; i < 20000; ++i) append(device.incoming, {0x55, 0x99, 0});
    auto corrupt = byteFrame;
    corrupt.back() ^= 1;
    append(device.incoming, corrupt);
    append(device.incoming, byteFrame);
    requireByte(inbox); // No recursion proportional to noise/corrupt frames.

    // A checksum-valid header promises a uint32, but only supplies one payload byte.
    auto shortPayload = byteFrame;
    shortPayload[5] = 2;
    updateChecksum(shortPayload);
    device.incoming = shortPayload;
    append(device.incoming, byteFrame);
    expectException<std::out_of_range>([&] { (void)inbox.getMessage(); });
    requireByte(inbox); // Neither checksum nor following frame became field data.

    auto extraPayload = byteFrame;
    extraPayload.insert(extraPayload.end() - 2, 77);
    updateChecksum(extraPayload);
    device.incoming = extraPayload;
    append(device.incoming, byteFrame);
    expectException<wrong_message_format>([&] { (void)inbox.hasMessage(); });
    requireByte(inbox);

    auto unknown = byteFrame;
    unknown[5] = 200;
    updateChecksum(unknown);
    device.incoming = unknown;
    append(device.incoming, byteFrame);
    expectException<no_such_message>([&] { (void)inbox.getMessage(); });
    requireByte(inbox);
  }

  void testSending(const MessageDictionary &dictionary)
  {
    auto deviceOwner = std::make_unique<MemoryDevice>();
    auto &device = *deviceOwner;
    PprzTransport transport(std::move(deviceOwner), dictionary);
    ReceiptQueue inbox(transport);
    Message message(dictionary.getDefinition("BYTE"));
    message.addField("value", 123);
    message.setSenderId(std::string("42"));
    message.setReceiverId(7);
    message.setComponentId(3);
    require(transport.sendMessage(message) == byteFrame.size() && device.outgoing == byteFrame,
            "Exact frame fixture and numeric string sender");
    for (const auto *sender : {"", "256", "-1", "4x", "plane"}) {
      message.setSenderId(std::string(sender));
      expectException<wrong_message_format>([&] { transport.sendMessage(message); });
      require(device.outgoing == byteFrame, "Invalid message sends no bytes");
    }
    message.setSenderId(uint8_t{42});
    message.setComponentId(16);
    expectException<wrong_message_format>([&] { transport.sendMessage(message); });

    Message large(dictionary.getDefinition("BLOB"));
    large.addField("data", std::vector<uint8_t>(246, 0xaa));
    require(transport.sendMessage(large) == 255 && device.outgoing[1] == 255, "Maximum frame size");
    device.incoming = device.outgoing;
    require(inbox.getMessage()->getField<std::vector<uint8_t>>("data").size() == 246,
            "Maximum frame round trip");
    large.addField("data", std::vector<uint8_t>(247, 0xaa));
    expectException<std::length_error>([&] { transport.sendMessage(large); });
    require(device.outgoing.size() == 255, "Oversized frame is rejected before writing");
    expectException<std::invalid_argument>([&] { PprzTransport invalid(nullptr, dictionary); });
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
    testDeviceOwnership<PprzTransport>(dictionary);
    testDeviceOwnership<XbeeTransport>(dictionary);
    testReception(dictionary);
    testSending(dictionary);
    std::cout << "Frame boundaries, resynchronization, atomic decode and sending passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
