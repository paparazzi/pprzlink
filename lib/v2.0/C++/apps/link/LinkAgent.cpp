// SPDX-License-Identifier: GPL-2.0-or-later
#include "LinkAgent.h"
#include <pprzlink/BoostSerialPortDevice.h>
#include <pprzlink/IvyMessageCodec.h>
#include <pprzlink/PosixFileDevice.h>
#include <pprzlink/PprzTransport.h>
#include <boost/system/system_error.hpp>
#include <algorithm>
#include <csignal>
#include <format>
#include <iostream>
#include <stdexcept>
#include <syncstream>

namespace link_app {
  using namespace std::chrono_literals;

  namespace {
    double wallTime()
    {
      return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    uint8_t receiverId(const pprzlink::Message &message)
    {
      // The XML decides the integer type. Reject a non-integral or overflowing
      // destination instead of silently wrapping it to another aircraft.
      return std::visit([](const auto &value) -> uint8_t {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::integral<Value> && !std::same_as<Value, char>) {
          if (std::in_range<uint8_t>(value)) return static_cast<uint8_t>(value);
        }
        throw std::invalid_argument("ac_id must be an integer in [0, 255]");
      }, message.getField("ac_id"));
    }
  }

  LinkAgent::LinkAgent(Options options)
    : options(std::move(options)), dictionary(Options::messagesPath()), signals(context, SIGINT, SIGTERM)
  {
    this->options.validate();
    openChannel();
    ivy = std::make_unique<IvyBridge>(context, dictionary, this->options.ivyBus,
      this->options.uplink, this->options.aircraftInfo,
      [this](pprzlink::Message message, bool broadcast) { uplink(std::move(message), broadcast); });
    if (this->options.linkId != -1 && !this->options.redundantLink)
      std::cerr << "LINK WARNING: link id was set without -redlink\n";
  }

  void LinkAgent::openChannel()
  {
    if (options.udp) {
      const pprzlink::UdpOptions configuration{
        .local = {"0.0.0.0", static_cast<uint16_t>(options.udpPort)},
        .broadcast = options.udpBroadcast
      };
      udp = std::make_unique<pprzlink::UdpTransport>(context, dictionary, configuration);
      return;
    }
    std::unique_ptr<pprzlink::Device> device;
    if (options.device.starts_with("/dev")) {
      using Serial = pprzlink::BoostSerialPortDevice;
      std::unique_ptr<Serial> serial;
      try {
        serial = std::make_unique<Serial>(context, options.device);
      } catch (const boost::system::system_error &error) {
        throw std::runtime_error(std::format(
          "Cannot open serial port '{}': {}.\n"
          "Check that the device is connected and select its port with -d <port>.",
          options.device, error.code().message()));
      }
      serial->setBaudrate(Serial::Baudrate(std::stoul(options.baudrate)));
      serial->setDataBits(Serial::DataBits(8));
      serial->setParity(Serial::Parity(Serial::Parity::none));
      serial->setStopBits(Serial::StopBits(Serial::StopBits::one));
      serial->setFlowcontrol(Serial::Flowcontrol(options.transport == "pprz" && options.hardwareFlowControl
        ? Serial::Flowcontrol::hardware : Serial::Flowcontrol::none));
      device = std::move(serial);
    } else {
      device = std::make_unique<pprzlink::PosixFileDevice>(context, options.device);
    }
    if (options.transport == "pprz") {
      transport = std::make_unique<pprzlink::PprzTransport>(std::move(device), dictionary);
    } else {
      using Radio = pprzlink::XbeeTransport;
      auto radio = std::make_unique<Radio>(std::move(device), dictionary,
        options.xbee868 ? Radio::Api::Series868 : Radio::Api::Legacy802154);
      radio->setSimulatedReceiveEnabled(true);
      pprzlink::XbeeConfiguration configuration;
      configuration.targetBaudrate = std::nullopt; // Preserve link's fixed -s and never write flash.
      configuration.hardwareFlowControl = options.hardwareFlowControl;
      configuration.localAddress = static_cast<uint16_t>(options.xbeeAddress);
      if (options.channel) configuration.channel = static_cast<uint8_t>(*options.channel);
      radio->startInitialization(configuration);
      radio->onReady([enabled = options.hardwareFlowControl] {
        std::cerr << "XBee ready: RTS/CTS " << (enabled ? "enabled" : "disabled")
                  << " on host and modem; broadcasts are not resent by link++\n";
      });
      xbee = std::make_unique<XbeeTransmitter>(*radio, options.xbeeRetries);
      radio->setStatusCallback([this](const Radio::RadioStatus &status) {
        if (const auto result = std::get_if<Radio::TransmitStatus>(&status)) { xbee->status(*result); scheduleTransmission(); }
      });
      transport = std::move(radio);
    }
  }

  void LinkAgent::run()
  {
    signals.async_wait([this](const boost::system::error_code &error, int) { if (!error) context.stop(); });
    auto handler = [this](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
      received(pprzlink::ReceivedMessage{message, info.frameSize, info.xbee, info.udpPeer});
    };
    auto errors = [](const pprzlink::ReceiveError &error) {
      if (error.kind == pprzlink::ReceiveError::Kind::Decode) std::cerr << "Invalid link message: " << error.message << '\n';
      else std::rethrow_exception(error.exception);
    };
    if (udp) { udp->bind(pprzlink::ALL, handler); udp->onError(errors); udp->start(); }
    else { transport->bind(pprzlink::ALL, handler); transport->onError(errors); transport->start(); }
    scheduleStatus();
    scheduleAge();
    if (options.uplink) schedulePing(500ms + std::chrono::milliseconds(options.pingPeriod));
    context.run();
    ivy->checkError();
  }

  void LinkAgent::received(pprzlink::ReceivedMessage packet)
  {
    const auto &message = packet.message;
    const auto frameSize = packet.frameSize;
    if (message.getClassId() != dictionary.getClassId("telemetry")) return;
    const auto id = std::get<uint8_t>(message.getSenderId());
    try {
      publish(pprzlink::ivy_codec::serializeLegacyMessage(message), id);
    } catch (const pprzlink::pprzlink_exception &error) {
      std::cerr << "Cannot publish telemetry: " << error.what() << '\n';
      return;
    }
    if (options.trafficStatistics)
      std::cout << std::format("{:.3f} {}\n", wallTime(), frameSize) << std::flush;
    // OCaml LINK_REPORT uses the PPRZ checksum counter, including in XBee mode
    // (where it stays zero). The library exposes XBee errors separately.
    const uint64_t errors = udp ? udp->getStatistics().checksumErrors
      : (options.transport == "pprz" ? transport->getStatistics().checksumErrors : 0);
    aircraft.received(id, frameSize, errors, message.getDefinition().getName() == "PONG",
                      AircraftRegistry::Clock::now(), std::move(packet.udpPeer));
  }

  void LinkAgent::uplink(pprzlink::Message message, bool broadcast)
  {
    try {
      message.setSenderId(uint8_t{0});
      message.setComponentId(0);
      if (broadcast) sendBroadcast(message);
      else sendTarget(message, receiverId(message));
    } catch (const std::exception &error) {
      std::osyncstream(std::cerr) << "Cannot send command: " << error.what() << '\n';
    }
  }

  bool LinkAgent::sendTarget(pprzlink::Message &message, uint8_t id)
  {
    if (!aircraft.isLive(id, options.aircraftTimeout)) return false;
    message.setReceiverId(id);
    aircraft.transmitted(id);
    if (udp) {
      auto destination = *aircraft.all().at(id).udpPeer;
      destination.port = static_cast<uint16_t>(options.udpUplinkPort);
      if (options.udpBroadcast) destination.address = options.broadcastAddress;
      udp->sendMessage(message, destination);
    } else return sendRadio(message);
    return true;
  }

  void LinkAgent::sendBroadcast(pprzlink::Message &message)
  {
    message.setReceiverId(255);
    aircraft.broadcast();
    if (udp) {
      for (const auto &[id, state] : aircraft.all()) {
        if (!aircraft.isLive(id, options.aircraftTimeout)) continue;
        auto destination = *state.udpPeer;
        destination.port = static_cast<uint16_t>(options.udpUplinkPort);
        if (options.udpBroadcast) destination.address = options.broadcastAddress;
        udp->sendMessage(message, destination);
      }
    } else sendRadio(message);
  }

  bool LinkAgent::sendRadio(const pprzlink::Message &message)
  {
    if (xbee) {
      const bool sent = xbee->send(message);
      scheduleTransmission();
      return sent;
    }
    transport->sendMessage(message);
    return true;
  }

  void LinkAgent::publish(std::string text, std::optional<uint8_t> sender)
  {
    if (options.localTimestamp) {
      const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
      text = std::format("{:.6f} {}", elapsed, text);
    }
    if (text.size() > 10000) { std::cerr << "Discarding long Ivy message\n"; return; }
    if (sender && options.redundantLink) {
      std::ranges::replace(text, ' ', ';');
      text = std::format("redlink TELEMETRY_MESSAGE {} {} {}", *sender, options.linkId, text);
    }
    ivy->send(text);
  }

  void LinkAgent::scheduleTransmission()
  {
    transmitTimer.cancel();
    if (!xbee) return;
    if (const auto deadline = xbee->nextDeadline()) {
      transmitTimer.expires_at(*deadline);
      transmitTimer.async_wait([this](const boost::system::error_code &error) {
        if (!error) { xbee->poll(); scheduleTransmission(); }
      });
    }
  }

  void LinkAgent::scheduleStatus()
  {
    statusTimer.expires_after(std::chrono::milliseconds(options.statusPeriod));
    statusTimer.async_wait([this](const boost::system::error_code &error) {
      if (error) return;
      const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - started).count();
      for (auto &report : aircraft.reports(options.linkId, options.statusPeriod, seconds)) publish(std::move(report));
      scheduleStatus();
    });
  }

  void LinkAgent::scheduleAge()
  {
    ageTimer.expires_after(std::chrono::milliseconds(options.statusPeriod / 3));
    ageTimer.async_wait([this](const boost::system::error_code &error) {
      if (error) return;
      aircraft.age(options.statusPeriod / 3);
      scheduleAge();
    });
  }

  void LinkAgent::schedulePing(std::chrono::milliseconds delay)
  {
    pingTimer.expires_after(delay);
    pingTimer.async_wait([this](const boost::system::error_code &error) {
      if (error) return;
      for (const auto &[id, state] : aircraft.all()) {
        (void)state;
        pprzlink::Message ping(dictionary.getDefinition("PING"));
        ping.setSenderId(uint8_t{0});
        const auto sentAt = AircraftRegistry::Clock::now();
        if (sendTarget(ping, id)) aircraft.pinged(id, sentAt);
      }
      schedulePing(std::chrono::milliseconds(options.pingPeriod));
    });
  }
}
