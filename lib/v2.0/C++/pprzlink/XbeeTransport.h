// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * @file XbeeTransport.h
 * @brief XBee AP=1 framing, radio status and PPRZLINK payload transport.
 * @ingroup xbee
 *
 * RF data contains the v2 message payload without the PPRZ serial envelope. Sending writes a radio frame but does not confirm RF delivery.
 */

#ifndef PPRZLINKCPP_XBEETRANSPORT_H
#define PPRZLINKCPP_XBEETRANSPORT_H

#include <pprzlink/Transport.h>
#include <pprzlink/RadioRetries.h>
#include <pprzlink/XbeeAddress.h>
#include <pprzlink/XbeeModem.h>
#include <boost/asio/steady_timer.hpp>
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <variant>

namespace pprzlink {
  /** XBee API transport, without escaping (AP=1): legacy 802.15.4 or 868 frames.
   * @ingroup xbee
   * Call startInitialization() or use an already configured modem. XML describes
   * PPRZLINK messages; it does not select or configure the radio transport.
   * Calls must be serialized, as for Transport. No automatic host-side retries.
   */
  class XbeeTransport : public Transport {
  public:
    /// @brief Radio frame family, selected independently of XML message schemas.
    enum class Api {
      Legacy802154, ///< TX16/TX64 and RX16/RX64 802.15.4 API frames.
      Series868 ///< Extended TX 0x10, RX 0x90 and status 0x8b frames.
    };

    /// @brief Per-frame radio ACK/retry choice, independent of host-side retries.
    /// Legacy 802.15.4 controls MAC ACK/retries. This does not change modem
    /// parameters (MM/RR/TO) or the host's decision to resend after a failure.
    /// Enabled is the send default. Broadcast addresses never request MAC ACKs.
    /// XML link routing is handled by applications such as link++.
    using RadioRetries = pprzlink::RadioRetries;
    /// @brief Maximum RF bytes, including the four-byte PPRZLINK header.
    static constexpr size_t maximumRfPayloadSize = 100;

    /// @brief Delivery status of a radio transmission, correlated by its frame ID.
    struct TransmitStatus {
      uint8_t frameId; ///< Transmit frame identifier selected by the sender.
      uint8_t status; ///< Modem result; zero without ACKs does not confirm remote reception.
      uint8_t retries = 0; ///< Retry count reported by extended 0x8b status frames only.
    };
    /// @brief Modem lifecycle/network status indication.
    struct ModemStatus { uint8_t status; ///< Modem-defined status code.
    };
    /// @brief Response to an API AT command, independent of message payloads.
    struct AtCommandResponse {
      uint8_t frameId; ///< API command frame identifier.
      std::array<char, 2> command; ///< Two-character AT command name.
      uint8_t status; ///< Modem-defined command result; zero is success.
      BytesBuffer value; ///< Raw command response bytes after the status byte.
    };
    /// @brief Value-owned alternatives for non-message radio API indications.
    using RadioStatus = std::variant<TransmitStatus, ModemStatus, AtCommandResponse>;
    /// @brief Synchronous observer borrowing a RadioStatus during its invocation.
    using StatusCallback = std::function<void(const RadioStatus &)>;

    /// @brief Radio source/RSSI/options metadata accompanying a received message.
    using ReceiveInfo = XbeeReceiveInfo;

    /// @brief Own a stream and assume an already configured AP=1 modem until initialization is requested.
    /// @param[in] device Non-null byte stream transferred into the transport.
    /// @param[in] dictionary Borrowed, unchanged schemas that must outlive the transport.
    /// @param[in] api Radio API frame family, independent of the XML.
    /// @throws std::invalid_argument The device pointer is null.
    XbeeTransport(std::unique_ptr<Device> device, const MessageDictionary &dictionary,
                  Api api = Api::Legacy802154);

    /// Start/restart AT initialization before exchanging messages. Clears buffered
    /// frames and metadata. Do not access the device directly during initialization.
    /// Settings are validated before replacing prior state or discarding input.
    /// hardwareFlowControl configures/verifies host RTS/CTS and modem D6/D7
    /// together; readiness and binary sends stay blocked on any partial failure.
    /// The device must be a SerialDevice, even with autobaud disabled.
    /// @param[in] configuration AT settings, optional autobaud target and paired RTS/CTS choice.
    /// @param[in] now Initial monotonic timestamp, real or test-injected.
    /// @throws std::invalid_argument Channel, timing or baud settings are invalid.
    void startInitialization(const XbeeConfiguration &configuration = {},
                             XbeeModem::TimePoint now = XbeeModem::Clock::now());
    /// @brief Cancel protocol deadlines and disable callbacks before decoder destruction.
    ~XbeeTransport() override;
    /// @brief Cancel asynchronous reads and initialization deadlines.
    void stop() override;
    /// @brief Observe successful initialization on the receive executor.
    /// @param[in] callback Application callback retained until replacement.
    void onReady(std::function<void()> callback) { readyCallback = std::move(callback); }
    /// True also for the default constructor's already-configured-modem mode.
    /// @return Whether message transmission is currently permitted.
    bool isReady() const noexcept { return !initialization || initialization->isReady(); }
    /// Detection and flash-save result, available after successful initialization.
    /// @return Owned baud result or std::nullopt when absent, disabled or not yet completed.
    std::optional<XbeeBaudrateInfo> getBaudrateInfo() const noexcept
    {
      return initialization ? initialization->getBaudrateInfo() : std::nullopt;
    }

    /// Use the PPRZLINK receiver as radio destination (64-bit in 868 mode).
    /// Receiver 255 broadcasts to 0xffff in either mode.
    /// Returns bytes written to the serial device, not confirmation of RF delivery.
    /// This override uses RadioRetries::Enabled, including polymorphic calls.
    /// @param[in] message Populated message with valid v2 IDs and at most 100 RF payload bytes.
    /// @return Complete AP=1 frame bytes synchronously written to the device.
    /// @throws std::exception Not ready, invalid message/size, sanity check or I/O failure.
    size_t sendMessage(const Message &message) override;
    /// @brief Send using the message receiver, with an explicit per-frame radio retry choice.
    /// @param[in] message Populated message with at most 100 RF payload bytes.
    /// @param[in] radioRetries Radio ACK/retry choice for this frame only.
    /// @return Complete AP=1 frame bytes synchronously written to the device.
    /// @throws std::invalid_argument The enum value is invalid.
    /// @throws std::exception Readiness, validation, encoding or I/O failure.
    size_t sendMessage(const Message &message, RadioRetries radioRetries);
    /// Caller-owned frame ID for tracking/retries; zero disables the status response.
    /// The application must not reuse an outstanding ID for a different message.
    /// @param[in] message Populated message; its receiver selects destination/broadcast addressing.
    /// @param[in] frameId Explicit radio frame ID, including zero to disable status responses.
    /// @param[in] radioRetries Radio ACK/retry choice; Enabled preserves normal transmit options.
    /// @return Complete radio frame bytes written, not delivery confirmation.
    /// @throws std::exception Readiness, validation, encoding or I/O failure.
    size_t sendMessageWithId(const Message &message, uint8_t frameId,
                             RadioRetries radioRetries = RadioRetries::Enabled);
    /// @brief Send to an explicit network or IEEE radio destination.
    /// The address factory selects TX16/TX64 in legacy mode. Series868 requires
    /// an IEEE destination and emits a 0x10 frame. The PPRZLINK receiver is unchanged.
    /// @param[in] message Populated PPRZLINK message.
    /// @param[in] destination Validated radio address from XbeeAddress::network() or ieee().
    /// @param[in] radioRetries Radio ACK/retry choice; Enabled preserves normal transmit options.
    /// @return Complete AP=1 frame bytes synchronously written to the device.
    /// @throws std::invalid_argument A network destination is used with Series868, or the retry enum is invalid.
    /// @throws std::exception Readiness, payload validation, encoding or I/O failure.
    size_t sendMessageTo(const Message &message, XbeeAddress destination,
                        RadioRetries radioRetries = RadioRetries::Enabled);
    /// @brief Send to an explicit radio destination with a caller-owned frame ID.
    /// Destination, status tracking and radio ACK/retries are independent; the
    /// PPRZLINK receiver is unchanged. Do not reuse an outstanding ID for a different message.
    /// @param[in] message Populated PPRZLINK message.
    /// @param[in] destination Network/IEEE destination; Series868 requires IEEE addressing.
    /// @param[in] frameId Explicit radio frame ID; zero disables status responses.
    /// @param[in] radioRetries Radio ACK/retry choice; Enabled preserves normal transmit options.
    /// @return Complete AP=1 frame bytes written, not delivery confirmation.
    /// @throws std::invalid_argument A network destination is used with Series868, or the retry enum is invalid.
    /// @throws std::exception Readiness, payload validation, encoding or I/O failure.
    size_t sendMessageToWithId(const Message &message, XbeeAddress destination, uint8_t frameId,
                              RadioRetries radioRetries = RadioRetries::Enabled);
    /// Compatibility wrapper for sendMessageTo(message, XbeeAddress::network(destination), radioRetries).
    /// @param[in] message Populated PPRZLINK message.
    /// @param[in] destination Explicit sixteen-bit radio address.
    /// @param[in] radioRetries Radio ACK/retry choice; Enabled preserves normal transmit options.
    /// @return Complete TX16 frame bytes written to the device.
    /// @throws std::invalid_argument This transport uses the 868 frame family.
    /// @throws std::exception Readiness, payload validation or I/O failure.
    size_t sendMessageTo16(const Message &message, uint16_t destination,
                          RadioRetries radioRetries = RadioRetries::Enabled);
    /// @brief Compatibility wrapper for sendMessageTo(message, XbeeAddress::ieee(destination), radioRetries).
    /// @param[in] message Populated PPRZLINK message, with its own independent receiver ID.
    /// @param[in] destination Radio destination address used by TX64 or extended 868 TX.
    /// @param[in] radioRetries Radio ACK/retry choice; Enabled preserves normal transmit options.
    /// @return Complete radio frame bytes written to the device.
    /// @throws std::exception Readiness, validation, encoding or I/O failure.
    size_t sendMessageTo64(const Message &message, uint64_t destination,
                          RadioRetries radioRetries = RadioRetries::Enabled);

    /// Extra validation of the complete API frame immediately before Device::writeBuffer.
    /// Failures throw without writing any bytes. Applications choose how to report them.
    /// @param[in] enabled Whether each complete transmit frame is additionally validated.
    void setSanityChecksEnabled(bool enabled) noexcept { sanityChecksEnabled = enabled; }
    /// OCaml simulators sometimes label RX16-shaped frames with type 0x01.
    /// Disabled by default, so ordinary transports do not decode echoed TX frames.
    /// @param[in] enabled Whether legacy RX16-shaped indications labelled 0x01 are accepted.
    void setSimulatedReceiveEnabled(bool enabled) noexcept { simulatedReceiveEnabled = enabled; }
    /// Validate a captured TX16/TX64/TX868 AP=1 frame and its PPRZLINK payload against the XML.
    /// No I/O; throws wrong_message_format with the reason for rejection.
    /// @param[in] frame Exactly one complete AP=1 transmit frame, without trailing data.
    /// @throws wrong_message_format Invalid delimiter, length, type, options, checksum or XML payload.
    void validateTransmitFrame(std::span<const uint8_t> frame) const;

    /// IDs cycle through 1..255. Match status events to this ID; do not keep
    /// more than 255 outstanding transmissions. An explicit zero disables TX status.
    /// @return Last successfully written frame ID; initially zero.
    uint8_t getLastFrameId() const noexcept { return lastFrameId; }
    /// Metadata of the last successfully decoded message (not its PPRZLINK sender ID).
    /// @return Borrowed optional radio metadata, updated by later receive calls.
    const std::optional<ReceiveInfo>& getLastReceiveInfo() const noexcept { return receiveInfo; }

    /// Called on the receive executor as radio status frames arrive.
    /// The frame is consumed before the callback, which may stop reception or restart initialization.
    /// The argument is borrowed for the call. Do not reenter reception or replace
    /// this callback from inside it. Exceptions propagate after consuming the frame.
    /// @param[in] callback Observer moved into the transport; an empty function disables notifications.
    void setStatusCallback(StatusCallback callback) { statusCallback = std::move(callback); }

  private:
    void receiveAvailable() override;
    void receptionStarted() override;
    bool progressInitialization(XbeeModem::TimePoint now);
    void scheduleInitialization();
    bool decodeMessage();
    std::optional<RadioStatus> decodeFrame(std::span<const uint8_t> data);
    size_t sendTo(const Message &message, uint64_t destination, bool addressIs64Bit,
                  std::optional<uint8_t> frameId, RadioRetries radioRetries);

    BytesBuffer transportBuffer;
    std::unique_ptr<Message> currentMessage;
    std::optional<ReceiveInfo> receiveInfo;
    StatusCallback statusCallback;
    std::exception_ptr statusFailure;
    std::function<void()> readyCallback;
    bool readyNotified = false;
    std::shared_ptr<boost::asio::steady_timer> initializationTimer;
    std::optional<XbeeModem> initialization;
    uint8_t lastFrameId = 0;
    bool sanityChecksEnabled = false;
    bool simulatedReceiveEnabled = false;
    Api api;
  };
}
#endif // PPRZLINKCPP_XBEETRANSPORT_H
