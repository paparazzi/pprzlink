// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "AircraftRegistry.h"
#include "IvyBridge.h"
#include "Options.h"
#include "XbeeTransmitter.h"
#include <pprzlink/UdpTransport.h>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>

namespace link_app {
  class LinkAgent {
  public:
    explicit LinkAgent(Options options);
    void run();

  private:
    void openChannel();
    void received(pprzlink::ReceivedMessage packet);
    void uplink(pprzlink::Message message, bool broadcast);
    bool sendTarget(pprzlink::Message &message, uint8_t id);
    void sendBroadcast(pprzlink::Message &message);
    bool sendRadio(const pprzlink::Message &message);
    void publish(std::string text, std::optional<uint8_t> telemetrySender = std::nullopt);
    void scheduleTransmission();
    void scheduleStatus();
    void scheduleAge();
    void schedulePing(std::chrono::milliseconds delay);

    boost::asio::io_context context;
    Options options;
    pprzlink::MessageDictionary dictionary;
    AircraftRegistry aircraft;
    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    std::unique_ptr<pprzlink::Transport> transport;
    std::unique_ptr<pprzlink::UdpTransport> udp;
    std::unique_ptr<XbeeTransmitter> xbee;
    boost::asio::steady_timer transmitTimer{context}, statusTimer{context}, ageTimer{context}, pingTimer{context};
    boost::asio::signal_set signals;
    std::unique_ptr<IvyBridge> ivy; // Destroy first, joining its thread before application state.
  };
}
