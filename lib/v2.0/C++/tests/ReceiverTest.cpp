// SPDX-License-Identifier: LGPL-3.0-or-later
#include "TestSupport.h"
#include <pprzlink/PprzTransport.h>
#include <pprzlink/UdpTransport.h>
#include <pprzlink/XbeeTransport.h>
#include <atomic>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include <thread>

using namespace pprzlink;
using namespace std::chrono_literals;

namespace {
  MessageDictionary dictionary()
  {
    tinyxml2::XMLDocument xml;
    require(xml.Parse(R"(<protocol><msg_class name="guide" id="1">
      <message name="VALUE" id="1"><field name="value" type="uint8"/></message>
      <message name="EMPTY" id="2"/>
    </msg_class></protocol>)") == tinyxml2::XML_SUCCESS, "Reactive fixture XML");
    return MessageDictionary(xml.RootElement());
  }
  Message value(const MessageDictionary &dict, unsigned int number = 7, unsigned int sender = 42)
  {
    Message message(dict.getDefinition("VALUE"));
    message.setField("value", number);
    message.setSenderId(sender); message.setReceiverId(7); message.setComponentId(3);
    return message;
  }
  void emit(MemoryDevice &wire, const Message &message)
  { wire.incoming = encodePprzFrame(message); wire.notify(); }

  void bindings()
  {
    const auto dict = dictionary();
    auto device = std::make_unique<MemoryDevice>(); auto &wire = *device;
    PprzTransport receiver(std::move(device), dict);
    std::vector<std::string> order;
    auto all = receiver.bind(ALL, [&](const Message &) { order.push_back("all"); });
    receiver.bind("VALUE", {.senderId = 42, .receiverId = 7, .className = "guide", .componentId = 3,
      .where = [](const Message &message) { return message.getFieldAs<int>("value") > 5; }},
      [&](const Message &, const ReceiveInfo &info) { require(info.frameSize == 9, "Frame metadata"); order.push_back("value"); });
    receiver.start(); receiver.start();
    emit(wire, value(dict));
    require(order == std::vector<std::string>({"all", "value"}), "Bare bind retains both callbacks and registration order");
    emit(wire, value(dict, 3)); emit(wire, value(dict, 7, 43));
    require(order == std::vector<std::string>({"all", "value", "all", "all"}), "Built-in criteria and predicate combine with AND");
    receiver.unbind(all); receiver.unbind(all);
    emit(wire, value(dict)); require(order.back() == "value" && order.size() == 5, "Unbind affects later messages");
    expectException<no_such_message>([&] { receiver.bind("UNKNOWN", [](const Message &) {}); });
    expectException<std::invalid_argument>([&] { receiver.bind(ALL, {.senderId = 256}, [](const Message &) {}); });
    expectException<std::invalid_argument>([&] { receiver.bind(ALL, {.componentId = 16}, [](const Message &) {}); });
    expectException<std::invalid_argument>([&] { receiver.bind(ALL, {.udpPort = 1}, [](const Message &) {}); });
    expectException<std::invalid_argument>([&] { receiver.bind(ALL, {.className = "guide", .classId = 2}, [](const Message &) {}); });
    receiver.stop(); emit(wire, value(dict)); require(order.size() == 5, "Stopped receivers do not dispatch");
    receiver.start(); require(order.size() == 6, "Restart resumes reception with retained bindings");
  }

  void mutationsAndFailures()
  {
    const auto dict = dictionary();
    auto device = std::make_unique<MemoryDevice>(); auto &wire = *device;
    PprzTransport receiver(std::move(device), dict);
    Receiver::BindingId later = 0;
    bool added = false;
    std::vector<int> order;
    receiver.bind(ALL, [&](const Message &) {
      order.push_back(1); receiver.unbind(later);
      if (!added) { added = true; receiver.bind(ALL, [&](const Message &) { order.push_back(3); }); }
    });
    later = receiver.bind(ALL, [&](const Message &) { order.push_back(2); });
    receiver.start(); emit(wire, value(dict)); emit(wire, value(dict));
    require(order == std::vector<int>({1, 1, 3}), "Callback-side removal is immediate and additions start with the next message");
    int errors = 0;
    receiver.onError([&](const ReceiveError &error) { require(error.kind == ReceiveError::Kind::Decode, "Decode category"); ++errors; });
    auto invalid = encodePprzFrame(value(dict)); invalid[5] = 99;
    uint8_t a = 0, b = 0;
    for (size_t i = 1; i < invalid.size() - 2; ++i) { a += invalid[i]; b += a; }
    invalid[invalid.size() - 2] = a; invalid.back() = b;
    const auto valid = encodePprzFrame(value(dict)); invalid.insert(invalid.end(), valid.begin(), valid.end());
    wire.incoming = invalid; wire.notify();
    require(errors == 1 && order.size() == 5, "Malformed frame is consumed and later valid input is dispatched");
    const auto failure = receiver.bind("VALUE", [](const Message &) { throw std::runtime_error("application"); });
    expectException<std::runtime_error>([&] { emit(wire, value(dict)); });
    require(!receiver.isRunning() && errors == 1, "Application exceptions stop reception and are not hidden as decoding errors");
    receiver.unbind(failure); receiver.start(); emit(wire, value(dict));
    require(receiver.isRunning(), "Explicit restart after an application failure");
  }

  void predicateStop()
  {
    const auto dict = dictionary();
    auto device = std::make_unique<MemoryDevice>(); auto &wire = *device;
    PprzTransport receiver(std::move(device), dict);
    int predicates = 0, selected = 0, later = 0;
    receiver.bind("VALUE", {.where = [&](const Message &) {
      if (++predicates == 1) receiver.stop();
      return true;
    }}, [&](const Message &) { ++selected; });
    receiver.bind(ALL, [&](const Message &) { ++later; });
    receiver.start(); emit(wire, value(dict));
    require(predicates == 1 && selected == 0 && later == 0 && !receiver.isRunning(),
            "A predicate that stops reception suppresses its own and subsequent callbacks");
    receiver.start(); emit(wire, value(dict));
    require(predicates == 2 && selected == 1 && later == 1 && receiver.isRunning(),
            "Restart preserves bindings after predicate-side stop");
  }

  void predicateUnbind()
  {
    const auto dict = dictionary();
    auto device = std::make_unique<MemoryDevice>(); auto &wire = *device;
    PprzTransport receiver(std::move(device), dict);
    Receiver::BindingId self = 0;
    int predicates = 0, selected = 0, later = 0;
    self = receiver.bind("VALUE", {.where = [&](const Message &) {
      ++predicates;
      receiver.unbind(self);
      return true;
    }}, [&](const Message &) { ++selected; });
    receiver.bind(ALL, [&](const Message &) { ++later; });
    receiver.start(); emit(wire, value(dict)); emit(wire, value(dict));
    require(predicates == 1 && selected == 0 && later == 2 && receiver.isRunning(),
            "A predicate that unbinds itself suppresses its callback and preserves other bindings");
  }

  void udpSourcesAndLifetime()
  {
    const auto dict = dictionary();
    boost::asio::io_context context;
    UdpTransport receiver(context, dict, {.local = {"127.0.0.1", 0}});
    UdpTransport first(context, dict, {.local = {"127.0.0.1", 0}}), second(context, dict, {.local = {"127.0.0.1", 0}});
    int all = 0, selected = 0;
    std::vector<UdpEndpoint> peers;
    const auto runner = std::this_thread::get_id();
    receiver.bind(ALL, [&](const Message &, const ReceiveInfo &info) {
      require(std::this_thread::get_id() == runner, "Callback executes in context.run, without a polling thread");
      ++all; peers.push_back(*info.udpPeer);
    });
    receiver.bind("VALUE", {.senderId = 42, .udpPeer = first.localEndpoint(), .udpAddress = "127.0.0.1",
      .udpPort = first.localEndpoint().port, .where = [](const Message &message, const ReceiveInfo &info) {
        return message.getFieldAs<int>("value") > 5 && info.frameSize == 9 && !info.xbee;
      }}, [&](const Message &) { ++selected; });
    expectException<std::invalid_argument>([&] { receiver.bind(ALL, {.xbeeAddress = 42}, [](const Message &) {}); });
    receiver.start();
    first.sendMessage(value(dict), receiver.localEndpoint()); second.sendMessage(value(dict), receiver.localEndpoint());
    first.sendMessage(value(dict, 3), receiver.localEndpoint()); second.sendMessage(value(dict, 8, 43), receiver.localEndpoint());
    while (all < 4) require(context.run_one_for(2s) > 0, "UDP callback deadline");
    require(selected == 1 && peers[0] == first.localEndpoint() && peers[1] == second.localEndpoint(),
            "PPRZLINK sender and actual UDP source are independent filters");
    receiver.stop(); receiver.start();
    first.sendMessage(value(dict), receiver.localEndpoint());
    while (all < 5) require(context.run_one_for(2s) > 0, "Restart callback deadline");
    require(selected == 2, "Restart before cancellation completion never overlaps read buffers");
    receiver.bind("VALUE", [&](const Message &) { receiver.stop(); });
    first.sendMessage(value(dict), receiver.localEndpoint());
    while (receiver.isRunning()) require(context.run_one_for(2s) > 0, "Callback-side stop");
    {
      UdpTransport pending(context, dict);
      pending.bind(ALL, [](const Message &) { throw std::runtime_error("destroyed callback"); });
      pending.start();
    }
    context.restart();
    context.run(); // Drain canceled handlers after wrapper destruction.
  }

  void multipleRunners()
  {
    const auto dict = dictionary();
    boost::asio::io_context context;
    UdpTransport receiver(context, dict, {.local = {"127.0.0.1", 0}});
    UdpTransport sender(context, dict, {.local = {"127.0.0.1", 0}});
    boost::asio::steady_timer timeout(context, 2s);
    std::atomic<int> active{0}, received{0}, sum{0};
    const auto caller = std::this_thread::get_id();
    receiver.bind(ALL, [&](const Message &message) {
      require(std::this_thread::get_id() != caller, "Callbacks use application-provided runners");
      require(++active == 1, "One receiver never invokes callbacks concurrently");
      sum += message.getFieldAs<int>("value");
      --active;
      if (++received == 64) { receiver.stop(); timeout.cancel(); }
    });
    timeout.async_wait([&](const boost::system::error_code &error) { if (!error) receiver.stop(); });
    receiver.start();
    for (unsigned int number = 0; number < 64; ++number) sender.sendMessage(value(dict, number), receiver.localEndpoint());
    std::array<std::exception_ptr, 2> errors;
    std::array<std::jthread, 2> workers;
    for (size_t index = 0; index < workers.size(); ++index) workers[index] = std::jthread([&, index] {
      try { context.run(); } catch (...) { errors[index] = std::current_exception(); context.stop(); }
    });
    for (auto &worker : workers) worker.join();
    for (const auto &error : errors) if (error) std::rethrow_exception(error);
    require(received == 64 && sum == 2016, "Multiple runners preserve exactly-once message delivery");
  }

  void radioFilters()
  {
    const auto dict = dictionary();
    auto device = std::make_unique<MemoryDevice>(); auto &wire = *device;
    XbeeTransport receiver(std::move(device), dict);
    int count = 0;
    receiver.bind("VALUE", {.senderId = 42, .xbeeAddress = 0x1234, .minimumRssi = -70},
      [&](const Message &) { ++count; });
    receiver.start();
    for (const auto &[source, rssi] : {std::pair{0x1234, 60}, {0x1234, 80}, {0x5678, 60}}) {
      wire.incoming = {0x7e, 0, 10, 0x81, static_cast<uint8_t>(source >> 8), static_cast<uint8_t>(source),
                       static_cast<uint8_t>(rssi), 0, 42, 0, 1, 1, 7};
      uint8_t checksum = 0xff; for (const auto byte : std::span(wire.incoming).subspan(3)) checksum -= byte;
      wire.incoming.push_back(checksum); wire.notify();
    }
    require(count == 1, "Radio address, sender and signed RSSI threshold are combined");
  }
}
int main()
{
  try {
    bindings(); mutationsAndFailures(); predicateStop(); predicateUnbind();
    udpSourcesAndLifetime(); multipleRunners(); radioFilters();
    std::cout << "Reactive bindings, filters, mutation, errors, restart, peers and lifetime passed\n";
  } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
