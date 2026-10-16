// SPDX-License-Identifier: GPL-2.0-or-later
#include "Options.h"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace link_app {
  namespace {
#ifdef __APPLE__
    constexpr auto defaultBroadcastAddress = "224.255.255.255";
#else
    constexpr auto defaultBroadcastAddress = "127.255.255.255";
#endif
    int integer(std::string text)
    {
      std::erase(text, '_'); // OCaml's int_of_string accepts digit separators.
      std::string_view digits(text);
      bool negative = false;
      if (digits.starts_with('-') || digits.starts_with('+')) {
        negative = digits.front() == '-';
        digits.remove_prefix(1);
      }
      int base = 10;
      if (digits.starts_with("0x") || digits.starts_with("0X")) base = 16;
      else if (digits.starts_with("0o") || digits.starts_with("0O")) base = 8;
      else if (digits.starts_with("0b") || digits.starts_with("0B")) base = 2;
      if (base != 10) digits.remove_prefix(2);
      long long value = 0;
      const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value, base);
      if (negative) value = -value;
      if (digits.empty() || error != std::errc{} || end != digits.data() + digits.size() ||
          value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        throw std::invalid_argument("invalid integer: " + text);
      return static_cast<int>(value);
    }
  }

  Options::Options() : broadcastAddress(defaultBroadcastAddress)
  {
    const auto environment = std::getenv("IVY_BUS");
    ivyBus = environment ? environment : broadcastAddress + ":2010";
  }

  std::expected<Options, std::string> Options::parse(int argc, char **argv)
  {
    Options options;
    try {
      for (int i = 1; i < argc; ++i) {
        std::string_view option(argv[i]);
        std::optional<std::string_view> inlineArgument;
        if (option.starts_with('-')) {
          const auto equals = option.find('=');
          if (equals != std::string_view::npos) {
            inlineArgument = option.substr(equals + 1);
            option = option.substr(0, equals);
          }
        }
        bool argumentConsumed = false;
        const auto argument = [&]() -> std::string {
          argumentConsumed = true;
          if (inlineArgument) return std::string(*inlineArgument);
          if (++i == argc) throw std::invalid_argument(std::string(option) + " needs an argument");
          return argv[i];
        };
        if (option == "-help" || option == "--help") {
          if (inlineArgument) throw std::invalid_argument("help takes no argument");
          options.help = true;
          break;
        }
        else if (option == "-b") options.ivyBus = argument();
        else if (option == "-d") options.device = argument();
        else if (option == "-s") options.baudrate = argument();
        else if (option == "-socat") {
          options.socatAction = argument();
          if (*options.socatAction != "start" && *options.socatAction != "stop")
            throw std::invalid_argument("-socat expects start or stop");
        }
        else if (option == "-transport") options.transport = argument();
        else if (option == "-ch") options.channel = integer(argument());
        else if (option == "-fg") options.trafficStatistics = true;
        else if (option == "-noac_info") options.aircraftInfo = false;
        else if (option == "-nouplink") options.uplink = false;
        else if (option == "-uplink") options.uplink = true;
        else if (option == "-hfc") options.hardwareFlowControl = true;
        else if (option == "-local_timestamp") options.localTimestamp = true;
        else if (option == "-udp") options.udp = true;
        else if (option == "-udp_port") options.udpPort = integer(argument());
        else if (option == "-udp_uplink_port") options.udpUplinkPort = integer(argument());
        else if (option == "-udp_broadcast") options.udpBroadcast = true;
        else if (option == "-udp_broadcast_addr") options.broadcastAddress = argument();
        else if (option == "-xbee_addr") options.xbeeAddress = integer(argument());
        else if (option == "-xbee_retries") options.xbeeRetries = integer(argument());
        else if (option == "-xbee_868") options.xbee868 = true;
        else if (option == "-redlink") options.redundantLink = true;
        else if (option == "-id") options.linkId = integer(argument());
        else if (option == "-status_period") options.statusPeriod = integer(argument());
        else if (option == "-ping_period") options.pingPeriod = integer(argument());
        else if (option == "-ac_timeout") options.aircraftTimeout = integer(argument());
        else if (option.starts_with('-')) throw std::invalid_argument("unknown option: " + std::string(option));
        if (inlineArgument && !argumentConsumed)
          throw std::invalid_argument(std::string(option) + " takes no argument");
        // Arg.parse in link.ml deliberately ignores anonymous arguments.
      }
      return options;
    } catch (const std::invalid_argument &error) {
      return std::unexpected(error.what());
    }
  }

  void Options::validate() const
  {
    if (transport != "pprz" && transport != "xbee")
      throw std::invalid_argument("transport must be pprz or xbee");
    if (statusPeriod < 3 || pingPeriod <= 0)
      throw std::invalid_argument("status_period must be >= 3 ms and ping_period must be > 0 ms");
    if (udpPort < 0 || udpPort > 65535 || udpUplinkPort < 0 || udpUplinkPort > 65535)
      throw std::invalid_argument("UDP ports must be in [0, 65535]");
    if (xbeeAddress < 0 || xbeeAddress > 65535 || xbeeRetries < 0)
      throw std::invalid_argument("Invalid XBee address or retry limit");
    if (channel && (*channel < 0x0c || *channel > 0x17))
      throw std::invalid_argument("XBee channel must be in [0x0c, 0x17]");
    if (udp && transport == "xbee")
      throw std::invalid_argument("XBee API needs a serial device; UDP carries PPRZ frames");
    if (!udp && device.starts_with("/dev")) {
      constexpr std::string_view speeds[] = {"0", "50", "75", "110", "134", "150", "200", "300",
        "600", "1200", "1800", "2400", "4800", "9600", "19200", "38400", "57600", "115200",
        "230400", "460800", "921600", "1500000", "3000000"};
      if (std::ranges::find(speeds, baudrate) == std::end(speeds))
        throw std::invalid_argument("Unsupported serial baudrate: " + baudrate);
    }
  }

  std::string Options::messagesPath()
  {
    if (const auto directory = std::getenv("PPRZLINK_DIR")) return std::string(directory) + "/messages.xml";
    if (const auto home = std::getenv("PAPARAZZI_HOME")) return std::string(home) + "/var/messages.xml";
    return "/usr/share/pprzlink/messages.xml";
  }

  std::string Options::usage()
  {
    const Options defaults;
    return std::format(R"(Usage:
  -b <ivy bus> Default is {}
  -d <port> Default is /dev/ttyUSB0
  -fg Enable traffic statistics on standard output
  -noac_info Disable ACINFO and ACINFO_LLA uplink
  -nouplink Disable uplink, including PING
  -s <baudrate> Default is 9600
  -socat <start|stop> Manage a background socat pair of virtual serial ports (Linux)
  -ch <channel> Default does not change configuration
  -hfc Enable RTS/CTS on the serial host and XBee modem; disabled by default
  -local_timestamp Add local timestamp to messages sent over Ivy
  -transport <transport> pprz or xbee; default is pprz
  -udp Listen on UDP instead of serial
  -udp_port <port> Default is 4242
  -udp_uplink_port <port> Default is 4243
  -udp_broadcast Broadcast UDP uplink
  -udp_broadcast_addr <address> Default is {}
  -uplink Enable uplink (deprecated; enabled by default)
  -xbee_addr <address> Local MY address; default is 256
  -xbee_retries <count> Maximum host attempts including the initial send; default is 10
    Unicast only; broadcast messages are never resent by link++
  -xbee_868 Use XBee 868 API frames
  -redlink Enable redundant-link telemetry encapsulation
  -id <id> Link identifier; default is -1
  -status_period <ms> LINK_REPORT period; default is 1000
  -ping_period <ms> PING period; default is 5000
  -ac_timeout <ms> Aircraft timeout; default is 5000; <= 0 disables expiry
  -help  Display this list of options
  --help Display this list of options
)", defaults.ivyBus, defaults.broadcastAddress);
  }
}
