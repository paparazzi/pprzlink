// SPDX-License-Identifier: LGPL-3.0-or-later
#include "TestSupport.h"
#include <pprzlink/MemoryDevice.h>
#include <pprzlink/PprzTransport.h>
#include <iostream>

namespace {
  void notifications()
  {
    boost::asio::io_context context;
    pprzlink::MemoryDevice memory(context);
    int calls = 0;
    pprzlink::BytesBuffer received;
    memory.setReceiveCallback([&] { ++calls; received = memory.readAll(); });
    memory.feed(pprzlink::BytesBuffer{1, 2});
    require(memory.availableBytes() == 2 && calls == 0, "Inactive memory input is retained without callbacks");
    memory.startReception(); memory.startReception();
    memory.feed(pprzlink::BytesBuffer{3});
    require(calls == 0, "Memory callbacks run on Asio, never inline in feed/start");
    context.run();
    require(calls == 1 && received == pprzlink::BytesBuffer({1, 2, 3}) && memory.availableBytes() == 0,
            "Adjacent memory writes are ordered and coalesced into a readiness event");
    context.restart();
    memory.feed(pprzlink::BytesBuffer{4});
    memory.stopReception();
    context.run();
    require(calls == 1 && memory.availableBytes() == 1, "Stopping cancels queued notification but preserves input");
    context.restart();
    memory.startReception(); memory.stopReception(); memory.startReception();
    context.run();
    require(calls == 2 && received == pprzlink::BytesBuffer{4}, "Restart before a queued completion remains exactly once");
  }
  void transport()
  {
    tinyxml2::XMLDocument xml;
    xml.Parse(R"(<protocol><msg_class name="guide" id="1"><message name="VALUE" id="1"><field name="value" type="uint8"/></message></msg_class></protocol>)");
    const pprzlink::MessageDictionary dictionary(xml.RootElement());
    boost::asio::io_context context;
    auto device = std::make_unique<pprzlink::MemoryDevice>(context); auto &wire = *device;
    pprzlink::PprzTransport receiver(std::move(device), dictionary);
    int calls = 0;
    receiver.bind("VALUE", {.senderId = 42}, [&](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
      require(message.getFieldAs<int>("value") == 7 && info.frameSize == 9 && !info.udpPeer && !info.xbee,
              "Memory transport preserves message, addressing and frame metadata");
      ++calls; receiver.stop();
    });
    pprzlink::Message message(dictionary.getDefinition("VALUE"));
    message.setField("value", 7); message.setSenderId(42);
    const auto bytes = pprzlink::encodePprzFrame(message);
    receiver.start();
    wire.feed(std::span(bytes).first(3)); context.run();
    require(calls == 0, "A fragmented memory frame does not dispatch a partial message");
    context.restart(); wire.feed(std::span(bytes).subspan(3)); context.run();
    require(calls == 1, "Completing a memory frame dispatches its callback");
    context.restart(); receiver.start(); receiver.sendMessage(message); context.run();
    require(calls == 2, "sendMessage loops back through the ordinary reactive transport API");
  }
  void lifetime()
  {
    boost::asio::io_context context;
    int calls = 0;
    {
      pprzlink::MemoryDevice memory(context);
      memory.setReceiveCallback([&] { ++calls; });
      memory.startReception(); memory.feed(pprzlink::BytesBuffer{1});
    }
    context.run();
    require(calls == 0, "Queued memory events cannot invoke observers after device destruction");
  }
}
int main()
{
  try { notifications(); transport(); lifetime(); std::cout << "Memory notifications, loopback, fragments, restart and lifetime passed\n"; }
  catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
