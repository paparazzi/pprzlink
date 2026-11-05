#include "TestSupport.h"
#include <boost/asio/post.hpp>
#include <pprzlink/XbeeTransport.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <vector>

using namespace pprzlink;
using namespace std::chrono_literals;

namespace {
  constexpr std::array<unsigned int, 8> rates{1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};

  struct Fault {
    std::string command;
    unsigned int occurrence;
    std::string reply; // Empty means no response.
  };

  // Model the documented modem behavior: ATCN replies at the old rate, then
  // applies BD. Only ATWR changes the rate retained across a simulated restart.
  class Radio : public SerialDevice {
  public:
    explicit Radio(unsigned int rate) : liveBaud(rate), storedBaud(rate),
      bd(static_cast<unsigned int>(std::ranges::find(rates, rate) - rates.begin())) {}

    void resetBaudrate(unsigned int rate) override
    {
      if (failReset) throw std::runtime_error("Cannot change host baud rate");
      hostBaud = rate;
      resets.push_back(rate);
      incoming.clear();
    }
    boost::asio::io_context context;
    ReceiveCallback callback;
    bool receiving = false;
    void setReceiveCallback(ReceiveCallback value) override { callback = std::move(value); }
    void startReception() override { receiving = true; notifyInput(); }
    void stopReception() override { receiving = false; }
    boost::asio::any_io_executor getExecutor() override { return context.get_executor(); }
    void notifyInput()
    {
      if (receiving && callback && !incoming.empty()) boost::asio::post(context, [this] {
        if (receiving && callback && !incoming.empty()) { auto observer = callback; observer(); }
      });
    }
    size_t availableBytes() override { return incoming.size(); }
    BytesBuffer readAll() override
    {
      if (!fragmentReplies || incoming.empty()) return std::exchange(incoming, {});
      BytesBuffer result{incoming.front()};
      incoming.erase(incoming.begin());
      notifyInput();
      return result;
    }
    void writeBuffer(const BytesBuffer &bytes) override
    {
      const std::string command(bytes.begin(), bytes.end());
      writes.emplace_back(hostBaud, command);
      if (silent || hostBaud != liveBaud) return;
      const auto occurrence = ++occurrences[command];
      if (fault && command == fault->command && occurrence == fault->occurrence) {
        incoming.assign(fault->reply.begin(), fault->reply.end());
        notifyInput();
        return;
      }
      std::string response = "OK\r";
      if (command == "+++") {
        inCommandMode = true;
      } else {
        require(inCommandMode, "AT commands require acknowledged command-mode entry");
        if (command == "ATBD\r") {
          response = std::format("{:X}\r", queryOverride.value_or(bd));
        } else if (command.starts_with("ATBD")) {
          const auto code = static_cast<unsigned int>(std::stoul(command.substr(4), nullptr, 16));
          require(code < rates.size(), "Only supported standard baud codes");
          pendingBd = code;
        } else if (command == "ATCN\r") {
          inCommandMode = false;
          if (pendingBd) {
            bd = *pendingBd;
            liveBaud = rates[bd];
            pendingBd.reset();
          }
        } else if (command == "ATWR\r") {
          require(hostBaud == liveBaud && occurrences["ATBD\r"] >= 2,
                  "Verify communication at the new baud rate before persisting");
          require(!runtimeConfigurationStarted, "Save before changing MY/CH/AP");
          storedBaud = liveBaud;
          ++flashWrites;
        } else if (command == "ATD6\r" || command == "ATD7\r") {
          response = std::to_string(command == "ATD6\r" ? d6 : d7) + "\r";
        } else if (command == "ATD60\r" || command == "ATD61\r" ||
                   command == "ATD70\r" || command == "ATD71\r") {
          (command[3] == '6' ? d6 : d7) = command[4] - '0';
          runtimeConfigurationStarted = true;
        } else if (command.starts_with("ATMY") || command.starts_with("ATCH") || command == "ATAP1\r") {
          runtimeConfigurationStarted = true;
        } else {
          throw std::runtime_error("Unexpected radio command: " + command);
        }
      }
      incoming.assign(response.begin(), response.end());
      notifyInput();
    }

    unsigned int hostBaud = 0, liveBaud, storedBaud, bd, flashWrites = 0;
    unsigned int d6 = 0, d7 = 1;
    std::optional<unsigned int> pendingBd, queryOverride;
    std::optional<Fault> fault;
    bool silent = false, failReset = false, fragmentReplies = true;
    std::vector<unsigned int> resets;
    std::vector<std::pair<unsigned int, std::string>> writes;
    std::map<std::string, unsigned int> occurrences;
  protected:
    void applyHardwareFlowControl(bool enabled) override { hostFlowControl = enabled; }
    bool readHardwareFlowControl() const override { return hostFlowControl; }
  private:
    bool hostFlowControl = false;
    BytesBuffer incoming;
    bool inCommandMode = false, runtimeConfigurationStarted = false;
  };

  XbeeConfiguration quickConfiguration()
  {
    XbeeConfiguration config; // Keep the public default: automatic discovery -> 57600.
    config.guardTime = 10ms;
    config.responseTimeout = 20ms;
    return config;
  }

  void complete(XbeeModem &modem, Radio &radio)
  {
    const auto start = XbeeModem::TimePoint{};
    for (auto elapsed = 0ms; elapsed < 1000ms; elapsed += 1ms) {
      if (modem.poll(radio, start + elapsed)) return;
    }
    throw std::runtime_error("Initializer did not terminate");
  }

  void testDiscoveryAndPersistence()
  {
    for (const bool flowControl : {false, true})
    for (const auto initial : rates) {
      Radio radio(initial);
      auto config = quickConfiguration();
      config.channel = 0x15;
      config.hardwareFlowControl = flowControl;
      XbeeModem modem(config, {});
      require(!modem.getBaudrateInfo(), "No success report before initialization");
      complete(modem, radio);
      const auto info = modem.getBaudrateInfo();
      require(info && info->detected == initial && info->configured == 57600,
              "Detect every standard rate and configure the default target");
      require(info->savedToFlash == (initial != 57600) &&
              radio.flashWrites == (initial != 57600 ? 1u : 0u), "Write flash only when changing baud rate");
      require(radio.hostBaud == 57600 && radio.liveBaud == 57600 && radio.storedBaud == 57600,
              "Host, running modem and stored baud rates agree");
      require(radio.getHardwareFlowControl() == flowControl && radio.d6 == unsigned(flowControl) &&
              radio.d7 == unsigned(flowControl), "Autobaud also verifies the paired host/modem RTS/CTS setting");
      require(radio.resets.front() == 57600 && radio.writes.back().second == "ATCN\r",
              "Try target first and finish by leaving command mode");
      if (initial == 57600) {
        require(radio.resets.size() == 1 && radio.occurrences["ATBD6\r"] == 0,
                "Already configured modem needs no scan or baud change");
      } else {
        require(radio.occurrences["ATBD6\r"] == 1 && radio.occurrences["ATCN\r"] == 2,
                "Apply BD then re-enter command mode for verification and save");
        const auto apply = std::ranges::find_if(radio.writes, [](const auto &write) { return write.second == "ATCN\r"; });
        require(apply->first == initial && (apply + 1)->first == 57600 && (apply + 1)->second == "+++",
                "Wait for ATCN at old baud before switching the host and re-entering");
      }

      Radio rebooted(radio.storedBaud);
      XbeeModem next(config, {});
      complete(next, rebooted);
      require(rebooted.resets == std::vector<unsigned int>{57600} && rebooted.flashWrites == 0,
              "Next startup finds saved rate immediately and avoids another flash write");
    }

    auto config = quickConfiguration();
    config.targetBaudrate = 115200;
    Radio radio(57600);
    XbeeModem overridden(config, {});
    complete(overridden, radio);
    require(radio.resets.front() == 115200 && radio.occurrences["ATBD7\r"] == 1 &&
            radio.storedBaud == 115200, "Explicit target overrides default and uses its BD code");

    Radio roundedCustom(115200);
    roundedCustom.queryOverride = 111111; // Legacy S1 custom BD can report its rounded clock.
    XbeeModem custom(quickConfiguration(), {});
    // Once changed to a standard code, subsequent reads report that code.
    const auto start = XbeeModem::TimePoint{};
    for (auto elapsed = 0ms; elapsed < 1000ms && !custom.isReady(); elapsed += 1ms) {
      if (roundedCustom.pendingBd) roundedCustom.queryOverride.reset();
      custom.poll(roundedCustom, start + elapsed);
    }
    require(custom.isReady() && roundedCustom.storedBaud == 57600, "Recognize a rounded custom BD reply");
  }

  void testFailures()
  {
    for (const auto &reply : {std::string("ERROR\r"), std::string()}) {
      for (const auto &fault : std::vector<Fault>{
          {"ATBD\r", 1, reply}, {"ATBD6\r", 1, reply}, {"ATCN\r", 1, reply},
          {"+++", 2, reply}, {"ATBD\r", 2, reply}, {"ATWR\r", 1, reply}}) {
        Radio radio(9600);
        radio.fault = fault;
        XbeeModem modem(quickConfiguration(), {});
        const auto error = expectException<std::runtime_error>([&] { complete(modem, radio); });
        require(error.find(reply.empty() ? "timeout" : "ERROR") != std::string::npos ||
                (reply.empty() && error.find("no reply") != std::string::npos), "Clear failure diagnostic");
        require(!modem.isReady() && !modem.getBaudrateInfo() && radio.flashWrites == 0,
                "Failed negotiation never claims a saved baud rate or enables sends");
        const auto writes = radio.writes.size();
        expectException<std::runtime_error>([&] { modem.poll(radio, XbeeModem::TimePoint{} + 2s); });
        require(radio.writes.size() == writes, "Failure is terminal, without additional commands");
      }
    }

    for (const auto &bad : {"junk\r", "3junk\r", "FFFFFFFFFFFFFFFF\r", "6\r"}) {
      Radio radio(9600);
      radio.fault = Fault{"ATBD\r", 1, bad};
      XbeeModem modem(quickConfiguration(), {});
      expectException<std::runtime_error>([&] { complete(modem, radio); });
      require(radio.flashWrites == 0 && radio.occurrences["ATBD6\r"] == 0, "Invalid or inconsistent BD blocks changes");
    }
    Radio wrongVerification(9600);
    wrongVerification.fault = Fault{"ATBD\r", 2, "3\r"};
    XbeeModem verification(quickConfiguration(), {});
    expectException<std::runtime_error>([&] { complete(verification, wrongVerification); });
    require(wrongVerification.occurrences["ATWR\r"] == 0, "Never save an unverified new baud rate");

    Radio absent(9600);
    absent.silent = true;
    XbeeModem scan(quickConfiguration(), {});
    const auto error = expectException<std::runtime_error>([&] { complete(scan, absent); });
    auto tried = absent.resets;
    std::ranges::sort(tried);
    require(tried == std::vector<unsigned int>(rates.begin(), rates.end()) &&
            absent.writes.size() == rates.size() && error.find("all configured baud rates") != std::string::npos,
            "Probe each candidate once and stop with an error if no modem answers");

    Radio failedPort(57600);
    failedPort.failReset = true;
    XbeeModem failed(quickConfiguration(), {});
    expectException<std::runtime_error>([&] { complete(failed, failedPort); });
    require(failedPort.writes.empty(), "Host baud configuration failure sends nothing");

    MemoryDevice generic;
    XbeeModem nonSerial(quickConfiguration(), {});
    const auto typeError = expectException<std::runtime_error>([&] { nonSerial.poll(generic, {}); });
    require(typeError.find("SerialDevice") != std::string::npos, "Autobaud requires a reconfigurable serial device");
    for (const auto target : {0u, 12345u, 230400u}) {
      auto config = quickConfiguration();
      config.targetBaudrate = target;
      expectException<std::invalid_argument>([&] { XbeeModem invalid(config); });
    }
  }

  void testTransportGating()
  {
    tinyxml2::XMLDocument xml;
    xml.Parse(R"(<protocol><msg_class name="test" id="1"><message name="EMPTY" id="1"/></msg_class></protocol>)");
    MessageDictionary dictionary(xml.RootElement());
    Message message(dictionary.getDefinition("EMPTY"));
    auto device = std::make_unique<Radio>(9600);
    auto &radio = *device;
    radio.fault = Fault{"ATWR\r", 1, "ERROR\r"};
    XbeeTransport transport(std::move(device), dictionary);
    transport.startInitialization(quickConfiguration());
    expectException<std::logic_error>([&] { transport.sendMessage(message); });
    transport.start();
    expectException<std::runtime_error>([&] { radio.context.run_for(1s); });
    expectException<std::logic_error>([&] { transport.sendMessageTo16(message, 42); });
    require(!transport.isReady() && !transport.getBaudrateInfo() && radio.writes.back().second == "ATWR\r",
            "A failed flash write keeps binary sends blocked");
  }
}

int main()
{
  try {
    testDiscoveryAndPersistence();
    testFailures();
    testTransportGating();
    std::cout << "Default autobaud, baud verification, flash persistence and failure gating passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
