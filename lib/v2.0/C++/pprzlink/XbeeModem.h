// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * @file XbeeModem.h
 * @brief Polled 802.15.4 AT initialization and baud-rate configuration.
 * @ingroup xbee
 *
 * The application services device I/O and owns the AT dialogue exclusively. Optional steady-clock timestamps support deterministic polling.
 */

#ifndef PPRZLINKCPP_XBEEMODEM_H
#define PPRZLINKCPP_XBEEMODEM_H

#include <pprzlink/Device.h>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace pprzlink {
  /// @brief Settings for a polled 802.15.4 modem command-mode dialogue.
  /// @ingroup xbee
  struct XbeeConfiguration {
    uint16_t localAddress = 0x100; ///< Radio MY address configured after baud verification/save.
    std::optional<uint8_t> channel; ///< Optional CH channel in [0x0c, 0x17]; absent leaves it unchanged.
    std::chrono::milliseconds guardTime{2000}; ///< Before/after +++ silence interval, in [1, 60000] ms.
    std::chrono::milliseconds responseTimeout{2000}; ///< AT reply deadline, in [1, 60000] ms; does not bound writes.
    /// Detect the modem's baud rate, switch to this standard rate and save changes.
    /// Requires a SerialDevice. nullopt keeps the existing baud rate without probing.
    /// Supported rates are 1200, 2400, 4800, 9600, 19200, 38400, 57600 and 115200.
    std::optional<unsigned int> targetBaudrate{57600};
    /// Single choice for both host RTS/CTS and modem D6/D7: false = 00, true = 11.
    /// Initialization requires a SerialDevice, verifies both modem pins, then
    /// applies/verifies the host setting before isReady(). No independent host
    /// flow-control changes are allowed afterwards; reinitialize to change it.
    bool hardwareFlowControl = false;
  };

  /// @brief Baud discovery and persistence result from successful initialization.
  /// @ingroup xbee
  struct XbeeBaudrateInfo {
    unsigned int detected; ///< Initial host rate at which the modem answered command-mode entry.
    unsigned int configured; ///< Target host rate verified against the ATBD reply.
    bool savedToFlash; ///< Whether the initializer received the successful ATWR reply.
  };

  /** Configure an 802.15.4 modem through UART AT commands.
   * @ingroup xbee
   * Exclusively use the device during initialization; service its I/O loop and poll
   * regularly. No sleeps or owned thread. The default escape character '+' is assumed.
   * Writes use Device's synchronous contract; response deadlines do not bound writes.
   * ATWR saves all current settings, before this initializer changes MY/CH/AP/D6/D7.
   * Bootstrap temporarily disables host flow control so an unconfigured CTS pin
   * cannot block command writes. Only the final, verified 00/11 pair is ready.
   * A partial I/O/AT failure leaves initialization failed, not a ready mixed pair;
   * changing two physical endpoints is not an atomic hardware transaction.
   */
  class XbeeModem {
  public:
    using Clock = std::chrono::steady_clock; ///< Monotonic clock used for guards and reply deadlines.
    using TimePoint = Clock::time_point; ///< Real or test-injected timestamp on Clock's timeline.

    /// @brief Validate settings and prepare the command sequence without performing I/O.
    /// @param[in] configuration Channel, timing, MY address and optional autobaud target.
    /// @param[in] now Initial timestamp, defining the first guard deadline.
    /// @throws std::invalid_argument Unsupported baud target, channel or timing bounds.
    explicit XbeeModem(const XbeeConfiguration &configuration, TimePoint now = Clock::now());

    /// Returns true only after all replies, baud verification/save and final ATCN.
    /// Errors leave sends blocked. Only missing entry replies advance to another baud.
    /// Optional timestamps use steady_clock and allow deterministic timer tests.
    /// @param[in,out] device Exclusively used SerialDevice, including in fixed-baud mode.
    /// @param[in] now Nondecreasing timestamp for guard/reply progression.
    /// @return True when initialization is complete; false while a guard or reply is pending.
    /// @throws std::runtime_error AT errors, timeouts, device incompatibility and I/O failures are latched with context.
    bool poll(Device &device, TimePoint now = Clock::now());
    /// @brief Inspect whether the final command-mode exit succeeded.
    /// @return True only in the successful terminal state.
    bool isReady() const noexcept { return stage == Stage::Ready; }
    /// @brief Next guard or response deadline; input arrival can advance the dialogue sooner.
    /// @return Absolute monotonic deadline, or nullopt in a terminal state.
    std::optional<TimePoint> nextDeadline() const noexcept;
    /// Available after successful initialization with baud detection enabled.
    /// @return Owned detected/configured/saved values, otherwise std::nullopt.
    std::optional<XbeeBaudrateInfo> getBaudrateInfo() const noexcept;
    /// Preserve API bytes received in the same serial read as the final OK.
    /// @return Owned remaining binary input; subsequent calls return an empty buffer.
    BytesBuffer takeRemainingBytes();

  private:
    enum class Stage { GuardBefore, GuardAfter, Command, Ready, Failed };
    enum class Command { Configure, ReadBaudrate, SetBaudrate, ApplyBaudrate,
                         VerifyBaudrate, SaveBaudrate };
    void beginBaudrate(Device &device, unsigned int baudrate, TimePoint now);
    void sendCommand(Device &device, TimePoint now, Command next = Command::Configure);
    bool finishCommand(Device &device, TimePoint now, const std::string &line);
    std::optional<std::string> readReply(const BytesBuffer &bytes, bool entering, size_t &consumed);
    [[noreturn]] void fail(const std::string &reason);
    std::string operation() const;

    XbeeConfiguration configuration;
    std::vector<std::string> commands;
    std::vector<unsigned int> baudrates;
    size_t baudrateIndex = 0;
    unsigned int currentBaudrate = 0;
    bool verifyingBaudrate = false;
    std::optional<XbeeBaudrateInfo> baudrateInfo;
    size_t commandIndex = 0;
    Command command = Command::Configure;
    std::string pendingCommand;
    Stage stage = Stage::GuardBefore;
    TimePoint guardDeadline;
    TimePoint responseDeadline;
    bool entered = false;
    bool replyHasBinary = false;
    bool flowControlPrepared = false;
    std::string reply;
    std::string failure;
    BytesBuffer remainingBytes;
  };
}
#endif // PPRZLINKCPP_XBEEMODEM_H
