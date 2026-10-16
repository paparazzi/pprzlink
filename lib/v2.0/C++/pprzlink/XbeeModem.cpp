// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * @file XbeeModem.cpp
 * @brief Guard-time and AT-response state machine with baud probing.
 * @ingroup xbee
 *
 * Only a missing command-mode entry reply advances to the next candidate rate. Errors latch; baud verification and ATWR precede later MY/CH/AP changes.
 */

#include "XbeeModem.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace pprzlink {
  /// @brief Internal legacy/S2C baud codes and ATBD reply validation.
  /// @ingroup internals
  namespace {
    /// @brief Standard UART rates indexed by their legacy/S2C ATBD code.
    constexpr std::array<unsigned int, 8> standardBaudrates{
        1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};

    /// @brief Map a supported target rate to its ATBD code.
    /// @param[in] rate One of the eight supported standard UART rates.
    /// @return Index in standardBaudrates, suitable for an ATBD command.
    /// @throws std::invalid_argument The requested rate is unsupported.
    unsigned int baudrateCode(unsigned int rate)
    {
      const auto found = std::ranges::find(standardBaudrates, rate);
      if (found == standardBaudrates.end()) {
        throw std::invalid_argument("XBee autobaud target must be 1200, 2400, 4800, 9600, 19200, 38400, 57600 or 115200");
      }
      return static_cast<unsigned int>(found - standardBaudrates.begin());
    }

    /// @brief Parse a complete hexadecimal AT reply with an optional 0x prefix.
    /// @param[in] text Modem response without its carriage-return terminator.
    /// @param[in] command AT command name used in error diagnostics.
    /// @return Parsed unsigned value.
    /// @throws std::runtime_error The reply is empty, malformed or outside the unsigned range.
    unsigned int parseHexReply(std::string_view text, std::string_view command)
    {
      if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
      unsigned int value = 0;
      const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
      if (text.empty() || error != std::errc{} || end != text.data() + text.size()) {
        throw std::runtime_error(std::format("invalid hexadecimal {} reply", command));
      }
      return value;
    }

    /// @brief Compare a modem ATBD value with the host rate used to discover it.
    /// @param[in] code Standard ATBD index or a custom numeric rate.
    /// @param[in] rate Host UART rate to verify.
    /// @return Exact standard-rate match, or a supported custom value within five percent.
    bool matchesBaudrate(unsigned int code, unsigned int rate)
    {
      if (code < standardBaudrates.size()) return standardBaudrates[code] == rate;
      // Custom BD replies report the actual rate, which can differ due to clock rounding.
      const auto difference = code > rate ? code - rate : rate - code;
      return code >= 0x80 && code <= 250000 && difference <= rate / 20;
    }
  }

  XbeeModem::XbeeModem(const XbeeConfiguration &options, TimePoint now)
    : configuration(options), guardDeadline(now), responseDeadline(now)
  {
    using namespace std::chrono_literals;
    if (options.channel && (*options.channel < 0x0c || *options.channel > 0x17)) {
      throw std::invalid_argument("XBee channel must be in [0x0c, 0x17]");
    }
    if (options.guardTime < 1ms || options.guardTime > 60s ||
        options.responseTimeout < 1ms || options.responseTimeout > 60s) {
      throw std::invalid_argument("XBee guard time and response timeout must be in [1, 60000] ms");
    }
    if (options.targetBaudrate) {
      (void)baudrateCode(*options.targetBaudrate);
      baudrates.push_back(*options.targetBaudrate);
      for (const auto rate : {9600u, 57600u, 115200u, 38400u, 19200u, 4800u, 2400u, 1200u}) {
        if (rate != *options.targetBaudrate) baudrates.push_back(rate);
      }
    }
    guardDeadline = now + options.guardTime;
    commands.push_back(std::format("ATMY{:04X}\r", options.localAddress));
    if (options.channel) commands.push_back(std::format("ATCH{:02X}\r", *options.channel));
    commands.push_back("ATAP1\r");
    commands.push_back(options.hardwareFlowControl ? "ATD61\r" : "ATD60\r");
    commands.push_back(options.hardwareFlowControl ? "ATD71\r" : "ATD70\r");
    commands.push_back("ATD6\r");
    commands.push_back("ATD7\r");
    commands.push_back("ATCN\r");
  }

  std::string XbeeModem::operation() const
  {
    auto name = stage == Stage::Command ? pendingCommand.substr(0, pendingCommand.size() - 1)
                                       : "entering command mode (+++)";
    if (currentBaudrate) name += std::format(" at {} baud", currentBaudrate);
    return name;
  }

  void XbeeModem::fail(const std::string &reason)
  {
    failure = "XBee initialization failed during " + operation() + ": " + reason;
    stage = Stage::Failed;
    throw std::runtime_error(failure);
  }

  void XbeeModem::beginBaudrate(Device &device, unsigned int baudrate, TimePoint now)
  {
    auto *serial = dynamic_cast<SerialDevice*>(&device);
    if (!serial) throw std::invalid_argument("XBee autobaud requires a SerialDevice");
    serial->resetBaudrate(baudrate);
    currentBaudrate = baudrate;
    stage = Stage::GuardBefore;
    entered = false;
    reply.clear();
    replyHasBinary = false;
    guardDeadline = now + configuration.guardTime;
  }

  std::optional<std::string> XbeeModem::readReply(const BytesBuffer &bytes, bool entering, size_t &consumed)
  {
    consumed = 0;
    for (const auto byte : bytes) {
      ++consumed;
      if (byte == '\n' && reply.empty()) continue;
      if (byte == '\r') {
        auto line = std::exchange(reply, {});
        const bool hadBinary = std::exchange(replyHasBinary, false);
        if (line == "ERROR" || (entering && hadBinary && line.ends_with("ERROR"))) fail("modem returned ERROR");
        if (line.empty()) continue;
        if (entering) {
          if (line == "OK" || (hadBinary && line.ends_with("OK"))) return "OK";
        } else {
          const bool flowQuery = command == Command::Configure &&
              (pendingCommand == "ATD6\r" || pendingCommand == "ATD7\r");
          if (command != Command::ReadBaudrate && command != Command::VerifyBaudrate && !flowQuery && line != "OK") {
            fail("unexpected AT response (expected OK)");
          }
          return line;
        }
      } else {
        if (byte < 0x20 || byte > 0x7e) replyHasBinary = true;
        if (reply.size() == 64) {
          if (!entering) fail("AT response exceeds 64 bytes");
          reply.erase(0, 1); // Bounded window while discarding pre-command radio traffic.
        }
        reply.push_back(static_cast<char>(byte));
      }
    }
    return std::nullopt;
  }

  void XbeeModem::sendCommand(Device &device, TimePoint now, Command next)
  {
    stage = Stage::Command;
    command = next;
    reply.clear();
    replyHasBinary = false;
    switch (command) {
      case Command::ReadBaudrate:
      case Command::VerifyBaudrate: pendingCommand = "ATBD\r"; break;
      case Command::SetBaudrate:
        pendingCommand = std::format("ATBD{:X}\r", baudrateCode(*configuration.targetBaudrate));
        break;
      case Command::ApplyBaudrate: pendingCommand = "ATCN\r"; break;
      case Command::SaveBaudrate: pendingCommand = "ATWR\r"; break;
      case Command::Configure: pendingCommand = commands.at(commandIndex); break;
    }
    device.writeBuffer(BytesBuffer(pendingCommand.begin(), pendingCommand.end()));
    responseDeadline = now + configuration.responseTimeout;
  }

  bool XbeeModem::finishCommand(Device &device, TimePoint now, const std::string &line)
  {
    switch (command) {
      case Command::ReadBaudrate:
        if (!matchesBaudrate(parseHexReply(line, "ATBD"), currentBaudrate)) {
          fail("ATBD reply does not match the detected baud rate");
        }
        baudrateInfo = XbeeBaudrateInfo{currentBaudrate, currentBaudrate, false};
        sendCommand(device, now, currentBaudrate == *configuration.targetBaudrate
                                ? Command::Configure : Command::SetBaudrate);
        break;
      case Command::SetBaudrate:
        sendCommand(device, now, Command::ApplyBaudrate);
        break;
      case Command::ApplyBaudrate:
        // Legacy 802.15.4 applies BD after returning ATCN's OK at the old rate.
        // Re-enter at the new rate and query BD before committing anything to flash.
        verifyingBaudrate = true;
        beginBaudrate(device, *configuration.targetBaudrate, now);
        break;
      case Command::VerifyBaudrate:
        if (parseHexReply(line, "ATBD") != baudrateCode(*configuration.targetBaudrate)) {
          fail("ATBD verification failed at the new baud rate; flash was not written");
        }
        baudrateInfo->configured = currentBaudrate;
        sendCommand(device, now, Command::SaveBaudrate);
        break;
      case Command::SaveBaudrate:
        // ATWR saves all current settings. Keep this before changing MY/CH/AP/D6/D7.
        baudrateInfo->savedToFlash = true;
        sendCommand(device, now);
        break;
      case Command::Configure:
        if (pendingCommand == "ATD6\r" || pendingCommand == "ATD7\r") {
          if (parseHexReply(line, pendingCommand.substr(0, 4)) != (configuration.hardwareFlowControl ? 1u : 0u))
            fail("RTS/CTS readback differs from the paired host/modem setting");
        }
        if (++commandIndex == commands.size()) {
          // ATCN has acknowledged all radio changes. Complete and verify the
          // host half before publishing readiness or permitting binary sends.
          auto &serial = dynamic_cast<SerialDevice &>(device);
          serial.configureXbeeHardwareFlowControl(configuration.hardwareFlowControl);
          stage = Stage::Ready;
          return true;
        }
        sendCommand(device, now);
        break;
    }
    return false;
  }

  /// @details Guard timing and reply parsing progress only when polled. A failed
  /// entry attempt can advance the baud probe, but command/verification failures
  /// latch immediately. Bytes following the final OK are retained for API decoding.
  bool XbeeModem::poll(Device &device, TimePoint now)
  {
    if (stage == Stage::Ready) return true;
    if (stage == Stage::Failed) throw std::runtime_error(failure);
    try {
      if (!flowControlPrepared) {
        auto *serial = dynamic_cast<SerialDevice *>(&device);
        if (!serial) throw std::invalid_argument("Paired XBee RTS/CTS configuration requires a SerialDevice");
        // Ignore CTS during command-mode bootstrap: D7 may still be disabled.
        // Claim the setting even in 00 mode to prevent later host-only changes.
        serial->configureXbeeHardwareFlowControl(false);
        flowControlPrepared = true;
      }
      if (!baudrates.empty() && !currentBaudrate) beginBaudrate(device, baudrates.front(), now);
      if (stage == Stage::GuardAfter && !entered && now >= responseDeadline) {
        if (!baudrates.empty() && !verifyingBaudrate && baudrateIndex + 1 < baudrates.size()) {
          beginBaudrate(device, baudrates[++baudrateIndex], now);
          return false;
        }
        fail(verifyingBaudrate ? "no reply at the new baud rate; flash was not written"
                              : "timeout waiting for OK (all configured baud rates tried)");
      }
      if (stage == Stage::Command && now >= responseDeadline) fail("timeout waiting for AT reply");
      const auto bytes = device.readAll();
      switch (stage) {
        case Stage::GuardBefore:
          // Drain stale replies before +++; never purge input after an AT command.
          if (now >= guardDeadline) {
            device.writeBuffer(BytesBuffer{'+', '+', '+'});
            stage = Stage::GuardAfter;
            guardDeadline = now + configuration.guardTime;
            responseDeadline = guardDeadline + configuration.responseTimeout;
          }
          return false;
        case Stage::GuardAfter: {
          size_t consumed = 0;
          if (!entered) entered = readReply(bytes, true, consumed).has_value();
          if (entered && now >= guardDeadline) {
            sendCommand(device, now, baudrates.empty() ? Command::Configure
                                    : verifyingBaudrate ? Command::VerifyBaudrate : Command::ReadBaudrate);
          }
          return false;
        }
        case Stage::Command: {
          size_t consumed = 0;
          const auto line = readReply(bytes, false, consumed);
          if (!line) return false;
          if (finishCommand(device, now, *line)) {
            remainingBytes.assign(bytes.begin() + consumed, bytes.end());
            return true;
          }
          // Extra bytes cannot acknowledge a command that had not been sent.
          return false;
        }
        case Stage::Ready:
        case Stage::Failed:
          break;
      }
    } catch (const std::exception &error) {
      if (stage != Stage::Failed) fail(error.what());
      throw;
    }
    return false;
  }

  std::optional<XbeeBaudrateInfo> XbeeModem::getBaudrateInfo() const noexcept
  {
    return isReady() ? baudrateInfo : std::nullopt;
  }

  std::optional<XbeeModem::TimePoint> XbeeModem::nextDeadline() const noexcept
  {
    switch (stage) {
      case Stage::GuardBefore: return guardDeadline;
      case Stage::GuardAfter: return entered ? guardDeadline : responseDeadline;
      case Stage::Command: return responseDeadline;
      case Stage::Ready:
      case Stage::Failed: return std::nullopt;
    }
    return std::nullopt;
  }

  BytesBuffer XbeeModem::takeRemainingBytes()
  {
    return std::exchange(remainingBytes, {});
  }
}
