#include "TestSupport.h"
#include <pprzlink/XbeeTransport.h>
#include <boost/asio/post.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <utility>

using namespace pprzlink;
using namespace std::chrono_literals;

namespace {
  struct ModemDevice : SerialDevice {
    MemoryDevice memory;
    boost::asio::io_context &context = memory.context;
    BytesBuffer &incoming = memory.incoming, &outgoing = memory.outgoing;
    std::vector<std::string> writes;
    bool failRead = false, failWrite = false, autoReplies = false;
    bool hostFlowControl = false, failHostEnable = false, ignoreHostSetting = false;
    unsigned int d6 = 0, d7 = 1;
    std::vector<bool> flowChanges;
    void resetBaudrate(unsigned int) override {}
    void setReceiveCallback(ReceiveCallback callback) override { memory.setReceiveCallback(std::move(callback)); }
    void startReception() override { memory.startReception(); }
    void stopReception() override { memory.stopReception(); }
    boost::asio::any_io_executor getExecutor() override { return context.get_executor(); }
    void notify() { memory.notify(); }
    size_t availableBytes() override { return memory.availableBytes(); }
    BytesBuffer readAll() override
    {
      if (failRead) throw std::runtime_error("read failed");
      return memory.readAll();
    }
    void writeBuffer(const BytesBuffer &bytes) override
    {
      if (failWrite) throw std::runtime_error("write failed");
      writes.emplace_back(bytes.begin(), bytes.end());
      memory.writeBuffer(bytes);
      if (autoReplies) {
        const auto &command = writes.back();
        if (command == "ATD60\r" || command == "ATD61\r") d6 = command[4] - '0';
        if (command == "ATD70\r" || command == "ATD71\r") d7 = command[4] - '0';
        if (command == "ATD6\r") reply(std::to_string(d6) + "\r");
        else if (command == "ATD7\r") reply(std::to_string(d7) + "\r");
        else reply("OK\r");
        if (writes.back() == "ATCN\r") incoming.insert(incoming.end(), {0x7e, 0, 9, 0x81, 0, 42, 50, 0, 42, 0, 1, 1, 0xf6});
        boost::asio::post(context, [this] { notify(); });
      }
    }
    void reply(std::string_view text) { incoming.assign(text.begin(), text.end()); }
  protected:
    void applyHardwareFlowControl(bool enabled) override
    {
      if (enabled && failHostEnable) throw std::runtime_error("Host RTS/CTS unavailable");
      flowChanges.push_back(enabled);
      if (!ignoreHostSetting) hostFlowControl = enabled;
    }
    bool readHardwareFlowControl() const override { return hostFlowControl; }
  };

  XbeeConfiguration quickConfiguration()
  {
    XbeeConfiguration options;
    options.targetBaudrate.reset(); // Exercise the explicit fixed-baud mode here.
    options.guardTime = 10ms;
    options.responseTimeout = 20ms;
    return options;
  }

  auto enter(XbeeModem &modem, ModemDevice &device, XbeeModem::TimePoint start)
  {
    device.reply("OK\r"); // A stale reply must not acknowledge the new escape sequence.
    require(!modem.poll(device, start) && device.writes.empty(), "Initial guard is silent");
    require(!modem.poll(device, start + 10ms) && device.writes.back() == "+++", "Escape without CR");
    device.reply("O");
    require(!modem.poll(device, start + 11ms), "Fragmented entry reply");
    device.reply("K\r\n");
    require(!modem.poll(device, start + 12ms) && device.writes.size() == 1, "Early OK preserves post-escape guard");
    require(!modem.poll(device, start + 20ms) && device.writes.back().starts_with("ATMY"), "Guard expires before first command");
    return start + 20ms;
  }

  void testSequence()
  {
    const auto start = XbeeModem::Clock::now();
    auto options = quickConfiguration();
    options.localAddress = 0x234;
    options.channel = 0x15;
    XbeeModem modem(options, start);
    ModemDevice device;
    auto now = enter(modem, device, start);
    for (const auto expected : {"ATCH15\r", "ATAP1\r", "ATD60\r", "ATD70\r", "ATD6\r", "ATD7\r", "ATCN\r"}) {
      const auto &previous = device.writes.back();
      device.reply(previous == "ATD6\r" || previous == "ATD7\r" ? "0\r0\r" : "OK\rOK\r");
      require(!modem.poll(device, now += 1ms) && device.writes.back() == expected, "One verified AT command at a time");
      require(!modem.poll(device, now += 1ms), "No premature success from duplicate OK");
    }
    device.reply("OK\r");
    device.incoming.insert(device.incoming.end(), {0x7e, 0, 2, 0x8a, 0, 0x75});
    require(modem.poll(device, now += 1ms) && modem.isReady(), "Ready only after ATCN OK");
    require(device.writes == std::vector<std::string>{"+++", "ATMY0234\r", "ATCH15\r", "ATAP1\r",
            "ATD60\r", "ATD70\r", "ATD6\r", "ATD7\r", "ATCN\r"},
            "Exact sequence and no ATWR or baud/PAN changes");
    require(modem.takeRemainingBytes() == BytesBuffer{0x7e, 0, 2, 0x8a, 0, 0x75}, "Preserve final-read API bytes");
    require(modem.takeRemainingBytes().empty() && modem.poll(device, now), "Completion is idempotent");
  }

  void testFailures()
  {
    const auto start = XbeeModem::Clock::now();
    for (const bool timeout : {false, true}) {
      // Every awaited reply, including entry and ATCN, must be checked.
      for (int step = 0; step < 8; ++step) {
        XbeeModem modem(quickConfiguration(), start);
        ModemDevice device;
        auto now = start + 10ms;
        modem.poll(device, now);
        if (step != 0) {
          device.reply("OK\r");
          now += 10ms;
          modem.poll(device, now);
          for (int i = 1; i < step; ++i) {
            const auto &previous = device.writes.back();
            device.reply(previous == "ATD6\r" || previous == "ATD7\r" ? "0\r" : "OK\r");
            modem.poll(device, now += 1ms);
          }
        }
        const auto count = device.writes.size();
        if (timeout) now += 40ms;
        else device.reply("ERROR\r");
        const auto error = expectException<std::runtime_error>([&] { modem.poll(device, now); });
        require(error.find(timeout ? "timeout" : "ERROR") != std::string::npos && !modem.isReady(), "Actionable AT failure");
        device.reply("OK\r");
        expectException<std::runtime_error>([&] { modem.poll(device, now + 1ms); });
        require(device.writes.size() == count, "No further writes after failed initialization");
      }
    }

    for (const auto &response : {std::string("NOK\r"), std::string(65, 'x')}) {
      XbeeModem modem(quickConfiguration(), start);
      ModemDevice device;
      const auto now = enter(modem, device, start);
      device.reply(response);
      expectException<std::runtime_error>([&] { modem.poll(device, now + 1ms); });
      require(device.writes.size() == 2, "Unexpected/overlong AT response blocks next command");
    }

    XbeeModem modem(quickConfiguration(), start);
    ModemDevice device;
    modem.poll(device, start + 10ms);
    device.reply("NOK\r");
    modem.poll(device, start + 20ms);
    require(device.writes.size() == 1, "NOK is not an entry acknowledgement");
    expectException<std::runtime_error>([&] { modem.poll(device, start + 40ms); });

    for (const bool reading : {false, true}) {
      XbeeModem failed(quickConfiguration(), start);
      ModemDevice io;
      io.failRead = reading;
      io.failWrite = !reading;
      expectException<std::runtime_error>([&] { failed.poll(io, start + 10ms); });
      io.failRead = io.failWrite = false;
      expectException<std::runtime_error>([&] { failed.poll(io, start + 20ms); });
      require(io.writes.empty() && !failed.isReady(), "I/O failures are terminal");
    }
  }

  void testTransportGating()
  {
    tinyxml2::XMLDocument xml;
    xml.Parse(R"(<protocol><msg_class name="test" id="1"><message name="EMPTY" id="1"/></msg_class></protocol>)");
    MessageDictionary dictionary(xml.RootElement());
    Message message(dictionary.getDefinition("EMPTY"));
    auto deviceOwner = std::make_unique<ModemDevice>();
    auto &device = *deviceOwner;
    XbeeTransport transport(std::move(deviceOwner), dictionary);
    device.autoReplies = true;
    transport.startInitialization(quickConfiguration());
    require(!transport.isReady(), "Starting initialization blocks transport");
    expectException<std::logic_error>([&] { transport.sendMessage(message); });
    require(device.writes.empty(), "Blocked send writes no serial bytes");
    int received = 0, ready = 0;
    transport.bind("EMPTY", [&](const Message &) { ++received; });
    transport.onReady([&] { ++ready; });
    transport.start();
    device.context.run_for(100ms);
    require(transport.isReady() && ready == 1 && received == 1,
            "Input notifications and guard deadlines initialize the modem and preserve post-ATCN frames");
    transport.sendMessage(message);
    require(device.outgoing.front() == 0x7e, "Binary send allowed after initialization");
    device.autoReplies = false;
    std::vector<ReceiveError::Kind> failures;
    transport.onError([&](const ReceiveError &error) { failures.push_back(error.kind); });
    transport.startInitialization(quickConfiguration());
    device.context.restart();
    device.context.run_for(100ms);
    require(failures == std::vector<ReceiveError::Kind>{ReceiveError::Kind::Initialization} && !transport.isRunning(),
            "A reply deadline reports initialization failure and stops reception without polling");
    const auto count = device.writes.size();
    expectException<std::logic_error>([&] { transport.sendMessageTo16(message, 42); });
    require(device.writes.size() == count, "Failed initialization never enables binary sends");
  }

  void testValidation()
  {
    for (const int invalid : {0, 11, 24, 255}) {
      auto options = quickConfiguration();
      options.channel = static_cast<uint8_t>(invalid);
      expectException<std::invalid_argument>([&] { XbeeModem modem(options); });
    }
    for (const auto duration : {0ms, -1ms, 60001ms}) {
      auto options = quickConfiguration();
      options.guardTime = duration;
      expectException<std::invalid_argument>([&] { XbeeModem modem(options); });
      options = quickConfiguration();
      options.responseTimeout = duration;
      expectException<std::invalid_argument>([&] { XbeeModem modem(options); });
    }
  }

  void testPairedFlowControl()
  {
    for (const bool enabled : {false, true}) {
      for (const bool initialHost : {false, true}) {
        auto owner = std::make_unique<ModemDevice>();
        auto &device = *owner;
        device.hostFlowControl = initialHost;
        device.autoReplies = true;
        tinyxml2::XMLDocument xml;
        xml.Parse(R"(<protocol><msg_class name="test" id="1"><message name="EMPTY" id="1"/></msg_class></protocol>)");
        const MessageDictionary dictionary(xml.RootElement());
        XbeeTransport transport(std::move(owner), dictionary);
        auto options = quickConfiguration();
        options.hardwareFlowControl = enabled;
        transport.startInitialization(options);
        int ready = 0;
        transport.onReady([&] {
          ++ready;
          require(device.getHardwareFlowControl() == enabled &&
                  device.d6 == unsigned(enabled) && device.d7 == unsigned(enabled),
                  "Readiness exposes only the paired 00 or 11 state");
          expectException<std::logic_error>([&] { device.setHardwareFlowControl(!enabled); });
          transport.sendMessage(Message(dictionary.getDefinition("EMPTY")));
        });
        transport.start();
        device.context.run_for(100ms);
        require(ready == 1 && transport.isReady(), "Both paired settings complete successfully");
        require(device.flowChanges == std::vector<bool>{false, enabled},
                "Ignore unconfigured CTS during bootstrap, apply host setting only after ATCN");
        require(device.outgoing.front() == 0x7e, "First binary send follows verified paired readiness");

        options.hardwareFlowControl = !enabled;
        transport.startInitialization(options);
        require(!transport.isReady(), "Changing the shared option blocks sends during reinitialization");
        transport.onReady([&] { ++ready; });
        device.context.restart();
        device.context.run_for(100ms);
        require(ready == 2 && device.hostFlowControl == !enabled &&
                device.d6 == unsigned(!enabled) && device.d7 == unsigned(!enabled),
                "Reinitialization changes both ends in either direction");
      }
    }
  }

  void testFlowControlFailures()
  {
    const auto start = XbeeModem::TimePoint{};
    for (const bool enabled : {false, true}) {
      for (const auto &query : {"ATD6\r", "ATD7\r"}) {
        auto options = quickConfiguration();
        options.hardwareFlowControl = enabled;
        XbeeModem modem(options, start);
        ModemDevice device;
        auto now = enter(modem, device, start);
        while (device.writes.back() != query) {
          device.reply("OK\r");
          if (device.writes.back() == "ATD6\r") device.reply(enabled ? "1\r" : "0\r");
          require(!modem.poll(device, now += 1ms), "Readback precedes readiness");
        }
        device.reply(enabled ? "0\r" : "1\r");
        const auto error = expectException<std::runtime_error>([&] { modem.poll(device, now += 1ms); });
        require(error.find("RTS/CTS") != std::string::npos && !modem.isReady(),
                "A mismatched modem pin never enables either 10 or 01");
        require(device.flowChanges == std::vector<bool>{false}, "Host stays in bootstrap mode on bad radio readback");
      }
    }

    for (const bool ignore : {false, true}) {
      auto options = quickConfiguration();
      options.hardwareFlowControl = true;
      auto owner = std::make_unique<ModemDevice>();
      auto &device = *owner;
      device.failHostEnable = !ignore;
      device.ignoreHostSetting = ignore;
      device.autoReplies = true;
      tinyxml2::XMLDocument xml;
      xml.Parse(R"(<protocol><msg_class name="test" id="1"><message name="EMPTY" id="1"/></msg_class></protocol>)");
      const MessageDictionary dictionary(xml.RootElement());
      XbeeTransport transport(std::move(owner), dictionary);
      transport.startInitialization(options);
      int ready = 0;
      transport.onReady([&] { ++ready; });
      transport.start();
      expectException<std::runtime_error>([&] { device.context.run_for(100ms); });
      require(!transport.isReady() && ready == 0, "Host setting failure or ignored setting never publishes readiness");
      const auto count = device.writes.size();
      expectException<std::logic_error>([&] { transport.sendMessage(Message(dictionary.getDefinition("EMPTY"))); });
      expectException<std::logic_error>([&] { device.setHardwareFlowControl(true); });
      require(count == device.writes.size() && device.writes.back() == "ATCN\r", "No binary frames after partial configuration failure");
    }

    MemoryDevice generic;
    auto options = quickConfiguration();
    XbeeModem unsupported(options, start);
    const auto error = expectException<std::runtime_error>([&] { unsupported.poll(generic, start); });
    require(error.find("SerialDevice") != std::string::npos && generic.outgoing.empty(),
            "Even 00 fixed-baud initialization requires control of the host UART");

    ModemDevice stuckHost;
    stuckHost.hostFlowControl = true;
    stuckHost.ignoreHostSetting = true;
    XbeeModem failedBootstrap(options, start);
    expectException<std::runtime_error>([&] { failedBootstrap.poll(stuckHost, start); });
    require(stuckHost.writes.empty() && !failedBootstrap.isReady(),
            "An unchangeable host flow-control setting blocks initialization before AT writes");
  }

  void testStatusReinitialization()
  {
    tinyxml2::XMLDocument xml;
    xml.Parse(R"(<protocol><msg_class name="test" id="1"><message name="EMPTY" id="1"/></msg_class></protocol>)");
    const MessageDictionary dictionary(xml.RootElement());
    const BytesBuffer statusFrame{0x7e, 0, 2, 0x8a, 0, 0x75};
    const BytesBuffer messageFrame{0x7e, 0, 9, 0x81, 0, 42, 50, 0, 42, 0, 1, 1, 0xf6};
    for (const bool buffered : {false, true}) {
      for (const bool failure : {false, true}) {
        auto deviceOwner = std::make_unique<ModemDevice>();
        auto &device = *deviceOwner;
        XbeeTransport transport(std::move(deviceOwner), dictionary);
        int received = 0, ready = 0, statuses = 0, errors = 0;
        transport.bind("EMPTY", [&](const Message &) { ++received; });
        transport.onReady([&] { ++ready; });
        transport.onError([&](const ReceiveError &) { ++errors; });
        transport.setStatusCallback([&](const auto &status) {
          ++statuses;
          transport.startInitialization(quickConfiguration());
          require(std::get<XbeeTransport::ModemStatus>(status).status == 0,
                  "Status remains valid after initialization clears the receive buffer");
          if (failure) throw std::runtime_error("restart callback failure");
        });
        transport.start();
        device.context.poll();
        require(ready == 1, "An already configured modem is initially ready");
        device.autoReplies = true;
        device.incoming = statusFrame;
        if (buffered) {
          // Junk and an ignored API frame exercise a nonzero decoder cursor.
          device.incoming.insert(device.incoming.begin(), {0x55, 0x7e, 0, 1, 0x82, 0x7d});
          device.incoming.insert(device.incoming.end(), messageFrame.begin(), messageFrame.end());
          device.incoming.insert(device.incoming.end(), statusFrame.begin(), statusFrame.end());
        }
        if (failure) {
          const auto error = expectException<std::runtime_error>([&] { device.notify(); });
          require(error == "restart callback failure" && !transport.isRunning(),
                  "A reinitializing status callback can still propagate its failure and stop reception");
          transport.start();
        } else device.notify();
        require(!transport.isReady() && received == 0 && statuses == 1 && errors == 0,
                "Status-side initialization discards buffered frames without dispatch or decode errors");
        device.context.restart();
        device.context.run_for(100ms);
        require(transport.isReady() && transport.isRunning() && ready == 2 && received == 1 && statuses == 1 && errors == 0,
                "Status-side initialization completes and reception resumes without replaying stale frames");
        require(transport.getStatistics().decodingErrors == 0 && transport.getStatistics().receivedMessages == 1,
                "Only the fresh post-initialization message is counted");
      }
    }
  }
}

int main()
{
  try {
    testSequence();
    testFailures();
    testTransportGating();
    testValidation();
    testPairedFlowControl();
    testFlowControlFailures();
    testStatusReinitialization();
    std::cout << "AT guards, acknowledgements, deadlines, failures and transport gating passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
