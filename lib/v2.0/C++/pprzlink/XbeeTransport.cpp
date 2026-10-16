// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * @file XbeeTransport.cpp
 * @brief Radio frame encoding, validation and receive recovery.
 * @ingroup xbee
 *
 * Radio addresses use network byte order while XML fields use their binary codec. Status callbacks run synchronously after their API frame is consumed.
 */

#include "XbeeTransport.h"
#include "detail/MessagePayload.h"
#include <algorithm>
#include <boost/asio/post.hpp>
#include <format>
#include <utility>

namespace pprzlink {
  /// @brief Internal AP=1 envelope inspection, checksums and radio-address decoding.
  /// @ingroup internals
  namespace {
    constexpr uint8_t startByte = 0x7e; ///< AP=1 start delimiter.
    constexpr uint8_t tx64 = 0x00, ///< Legacy TX with an eight-byte destination address.
                      tx16 = 0x01; ///< Legacy TX with a two-byte destination address.
    constexpr uint8_t rx64 = 0x80, ///< Legacy RX with an eight-byte source address.
                      rx16 = 0x81; ///< Legacy RX with a two-byte source address.
    constexpr uint8_t atResponse = 0x88, ///< API AT command response.
                      txStatus = 0x89, ///< Legacy transmit status response.
                      modemStatus = 0x8a; ///< Modem lifecycle/network indication.
    constexpr size_t envelopeSize = 4; ///< Delimiter, two length bytes and checksum.
    constexpr size_t maximumFrameDataSize = 14 + XbeeTransport::maximumRfPayloadSize; ///< Largest supported extended header plus RF bytes.

    /// @brief Compute the AP=1 complement checksum of frame data.
    /// @param[in] data Bytes after the length field and before the checksum.
    /// @return Byte that makes the wrapping sum of data and checksum equal to 0xff.
    uint8_t checksum(std::span<const uint8_t> data)
    {
      uint8_t sum = 0;
      for (const auto byte : data) sum += byte;
      return static_cast<uint8_t>(0xff - sum);
    }

    /// @brief Read the network-order API frame-data length.
    /// @param[in] frame Input beginning at the delimiter and containing at least three bytes.
    /// @return Declared frame-data bytes, excluding the API envelope.
    size_t frameDataSize(std::span<const uint8_t> frame)
    {
      return (static_cast<size_t>(frame[1]) << 8) | frame[2];
    }

    /// @brief Envelope outcome before decoding a radio indication.
    enum class FrameStatus {
      Incomplete, ///< More bytes are needed for the candidate frame.
      BadLength, ///< Declared length is zero or exceeds the supported maximum.
      BadChecksum, ///< Complete candidate fails checksum verification.
      Complete ///< Candidate envelope is ready for radio/payload decoding.
    };

    /// @brief Validate a leading candidate within bounded radio frame sizes.
    /// @param[in] frame Input beginning at the 0x7e delimiter.
    /// @return Candidate completeness or its envelope rejection reason.
    FrameStatus inspectFrame(std::span<const uint8_t> frame)
    {
      if (frame.size() < 3) return FrameStatus::Incomplete;
      const auto size = frameDataSize(frame);
      // This transport handles legacy 802.15.4 frames with at most 100 RF bytes.
      // Reject corrupt lengths before waiting for/allocating a 64 KiB frame.
      if (size == 0 || size > maximumFrameDataSize) return FrameStatus::BadLength;
      if (frame.size() < size + envelopeSize) return FrameStatus::Incomplete;
      if (checksum(frame.subspan(3, size)) != frame[3 + size]) return FrameStatus::BadChecksum;
      return FrameStatus::Complete;
    }

    /// @brief Decode a two- or eight-byte big-endian radio address.
    /// @param[in] bytes Address bytes supplied by a validated radio header.
    /// @return Numeric address independent of the PPRZLINK message sender.
    uint64_t readAddress(std::span<const uint8_t> bytes)
    {
      uint64_t address = 0;
      for (const auto byte : bytes) address = (address << 8) | byte;
      return address;
    }
  }

  XbeeTransport::XbeeTransport(std::unique_ptr<Device> device, const MessageDictionary &dictionary, Api api)
    : Transport(std::move(device), dictionary, Kind::Xbee), api(api)
  {
    transportBuffer.reserve(maximumFrameDataSize + envelopeSize);
  }

  XbeeTransport::~XbeeTransport()
  {
    shutdown();
    try { stop(); } catch (...) {}
  }

  void XbeeTransport::stop()
  {
    auto lock = lockReceiver();
    if (initializationTimer) initializationTimer->cancel();
    Transport::stop();
  }

  void XbeeTransport::receptionStarted()
  {
    boost::asio::post(device->getExecutor(), guarded([this] {
      try { receiveAvailable(); } catch (...) { stop(); throw; }
    }));
  }

  void XbeeTransport::receiveAvailable()
  {
    if (!progressInitialization(XbeeModem::Clock::now()) || !isRunning()) return;
    if (!readyNotified) {
      readyNotified = true;
      auto callback = readyCallback;
      if (callback) callback();
    }
    if (!isRunning() || !isReady()) return;
    const auto bytes = receiveBytes();
    transportBuffer.insert(transportBuffer.end(), bytes.begin(), bytes.end());
    while (isRunning()) {
      bool decoded;
      try { decoded = decodeMessage(); }
      catch (...) {
        if (statusFailure) { auto error = std::exchange(statusFailure, {}); stop(); std::rethrow_exception(error); }
        reportError(std::current_exception(), ReceiveError::Kind::Decode);
        continue;
      }
      if (!decoded) break;
      auto message = std::move(currentMessage);
      deliver(*message, pprzlink::ReceiveInfo{lastReceivedFrameSize, receiveInfo, std::nullopt});
    }
  }

  void XbeeTransport::scheduleInitialization()
  {
    if (!initialization || !isRunning()) return;
    if (!initializationTimer) initializationTimer = std::make_shared<boost::asio::steady_timer>(device->getExecutor());
    initializationTimer->cancel();
    if (const auto deadline = initialization->nextDeadline()) {
      initializationTimer->expires_at(*deadline);
      auto notification = guarded([this] {
        try { receiveAvailable(); } catch (...) { stop(); throw; }
      });
      initializationTimer->async_wait([timer = initializationTimer, notification](const boost::system::error_code &error) mutable {
        if (!error) notification();
      });
    }
  }

  size_t XbeeTransport::sendMessage(const Message &message)
  {
    return sendMessage(message, RadioRetries::FromDefinition);
  }

  size_t XbeeTransport::sendMessage(const Message &message, RadioRetries radioRetries)
  {
    const uint16_t destination = message.getReceiverId() == 255 ? 0xffff : message.getReceiverId();
    return api == Api::Series868 ? sendMessageTo64(message, destination, radioRetries)
                                 : sendMessageTo16(message, destination, radioRetries);
  }

  size_t XbeeTransport::sendMessageWithId(const Message &message, uint8_t frameId, RadioRetries radioRetries)
  {
    const uint64_t destination = message.getReceiverId() == 255 ? 0xffff : message.getReceiverId();
    return sendTo(message, destination, api == Api::Series868, frameId, radioRetries);
  }

  size_t XbeeTransport::sendMessageTo(const Message &message, XbeeAddress destination, RadioRetries radioRetries)
  {
    const bool addressIs64Bit = destination.kind() == XbeeAddress::Kind::Ieee;
    if (!addressIs64Bit && api == Api::Series868)
      throw std::invalid_argument("868 API requires 64-bit addressing");
    return sendTo(message, destination.value(), addressIs64Bit, std::nullopt, radioRetries);
  }

  size_t XbeeTransport::sendMessageTo16(const Message &message, uint16_t destination, RadioRetries radioRetries)
  {
    return sendMessageTo(message, XbeeAddress::network(destination), radioRetries);
  }

  size_t XbeeTransport::sendMessageTo64(const Message &message, uint64_t destination, RadioRetries radioRetries)
  {
    return sendMessageTo(message, XbeeAddress::ieee(destination), radioRetries);
  }

  size_t XbeeTransport::sendTo(const Message &message, uint64_t destination, bool addressIs64Bit,
                              std::optional<uint8_t> requestedId, RadioRetries radioRetries)
  {
    auto lock = lockReceiver();
    if (!isReady()) throw std::logic_error("XBee initialization must succeed before sending messages");
    if (radioRetries == RadioRetries::FromDefinition)
      radioRetries = message.getDefinition().getRadioRetries();
    uint8_t transmitOptions = 0;
    switch (radioRetries) {
      case RadioRetries::Enabled: break;
      case RadioRetries::Disabled: transmitOptions = 0x01; break;
      default: throw std::invalid_argument("Invalid XBee radio retry choice");
    }
    const auto payload = detail::encodeMessagePayload(message, maximumRfPayloadSize);
    const size_t addressSize = addressIs64Bit ? 8 : 2;
    const size_t dataSize = 3 + addressSize + payload.size() + (api == Api::Series868 ? 3 : 0);
    const uint8_t frameId = requestedId.value_or(lastFrameId == 255 ? 1 : static_cast<uint8_t>(lastFrameId + 1));
    BytesBuffer frame{startByte, static_cast<uint8_t>(dataSize >> 8), static_cast<uint8_t>(dataSize),
                      api == Api::Series868 ? uint8_t{0x10} : (addressIs64Bit ? tx64 : tx16), frameId};
    frame.reserve(dataSize + envelopeSize);
    for (size_t i = addressSize; i > 0; --i) frame.push_back(static_cast<uint8_t>(destination >> (8 * (i - 1))));
    if (api == Api::Series868) frame.insert(frame.end(), {0xff, 0xfe, 0x00});
    frame.push_back(transmitOptions); // Per-frame Disable ACK bit; no modem configuration change.
    // No 0x99/length/PPRZ checksum inside the XBee RF data.
    frame.insert(frame.end(), payload.begin(), payload.end());
    frame.push_back(checksum(std::span(frame).subspan(3)));
    if (sanityChecksEnabled) validateTransmitFrame(frame);
    device->writeBuffer(frame);
    lastFrameId = frameId;
    return frame.size();
  }

  void XbeeTransport::validateTransmitFrame(std::span<const uint8_t> frame) const
  {
    const auto reject = [](const std::string &reason) {
      throw wrong_message_format("XBee sanity check: " + reason);
    };
    if (frame.size() < envelopeSize) reject("truncated API envelope");
    if (frame[0] != startByte) reject("expected start delimiter 0x7e");
    const auto dataSize = frameDataSize(frame);
    if (dataSize != frame.size() - envelopeSize) {
      reject(std::format("length declares {} bytes, actual frame data has {}", dataSize, frame.size() - envelopeSize));
    }
    if (dataSize == 0) reject("missing API frame type");
    if (frame[3] != tx16 && frame[3] != tx64 && frame[3] != 0x10) {
      reject(std::format("unsupported transmit frame type 0x{:02x} (expected 0x00, 0x01 or 0x10)", frame[3]));
    }
    const size_t headerSize = frame[3] == 0x10 ? 14 : (frame[3] == tx16 ? 5 : 11);
    if (dataSize < headerSize) reject("truncated radio address/transmit header");
    const size_t payloadSize = dataSize - headerSize;
    if (payloadSize < detail::messageHeaderSize || payloadSize > maximumRfPayloadSize) {
      reject(std::format("RF payload has {} bytes; expected 4..100 including the PPRZLINK header", payloadSize));
    }
    if ((frame[3 + headerSize - 1] & ~(frame[3] == 0x10 ? 0x03 : 0x05)) != 0) reject("reserved transmit option bits are set");
    // Check the sum including the checksum, independently of the encoder's helper.
    uint8_t sum = 0;
    for (const auto byte : frame.subspan(3)) sum += byte;
    if (sum != 0xff) reject("invalid API checksum");
    try {
      (void)detail::decodeMessagePayload(dictionary, frame.subspan(3 + headerSize, payloadSize));
    } catch (const std::exception &error) {
      reject("PPRZLINK payload does not match the XML: " + std::string(error.what()));
    }
  }

  void XbeeTransport::startInitialization(const XbeeConfiguration &configuration, XbeeModem::TimePoint now)
  {
    auto lock = lockReceiver();
    // Validate before replacing a previous initialization or discarding received data.
    auto modem = XbeeModem(configuration, now);
    initialization = std::move(modem);
    transportBuffer.clear();
    currentMessage.reset();
    receiveInfo.reset();
    lastFrameId = 0;
    lastReceivedFrameSize = 0;
    readyNotified = false;
    if (isRunning()) receptionStarted();
  }

  bool XbeeTransport::progressInitialization(XbeeModem::TimePoint now)
  {
    if (!initialization) return true;
    try {
      if (!initialization->poll(*device, now)) { scheduleInitialization(); return false; }
    } catch (...) {
      auto error = std::current_exception();
      stop();
      reportError(error, ReceiveError::Kind::Initialization);
      return false;
    }
    if (initializationTimer) initializationTimer->cancel();
    const auto remaining = initialization->takeRemainingBytes();
    transportBuffer.insert(transportBuffer.end(), remaining.begin(), remaining.end());
    return true;
  }

  std::optional<XbeeTransport::RadioStatus> XbeeTransport::decodeFrame(std::span<const uint8_t> data)
  {
    const auto requireSize = [&](size_t size, bool exact = false) {
      if (data.size() < size || (exact && data.size() != size)) {
        throw wrong_message_format("Invalid XBee API frame size for type " + std::to_string(data[0]));
      }
    };
    switch (data[0]) {
      case tx16:
      case rx16:
      case rx64: {
        if (api != Api::Legacy802154) break;
        if (data[0] == tx16 && !simulatedReceiveEnabled) break;
        const bool addressIs64Bit = data[0] == rx64;
        const size_t addressSize = addressIs64Bit ? 8 : 2;
        const size_t headerSize = addressSize + 3;
        requireSize(headerSize + detail::messageHeaderSize);
        const auto payload = data.subspan(headerSize);
        if (payload.size() > maximumRfPayloadSize) throw wrong_message_format("XBee RF payload exceeds 100 bytes");
        auto message = std::make_unique<Message>(detail::decodeMessagePayload(dictionary, payload));
        receiveInfo = ReceiveInfo{readAddress(data.subspan(1, addressSize)), addressIs64Bit,
                                  data[1 + addressSize], data[2 + addressSize]};
        currentMessage = std::move(message);
        break;
      }
      case 0x90: {
        if (api != Api::Series868) break;
        requireSize(12 + detail::messageHeaderSize);
        const auto payload = data.subspan(12);
        if (payload.size() > maximumRfPayloadSize) throw wrong_message_format("XBee RF payload exceeds 100 bytes");
        currentMessage = std::make_unique<Message>(detail::decodeMessagePayload(dictionary, payload));
        receiveInfo = ReceiveInfo{readAddress(data.subspan(1, 8)), true, 0, data[11], false};
        break;
      }
      case 0x8b:
        if (api != Api::Series868) break;
        requireSize(7, true);
        return TransmitStatus{data[1], data[5], data[4]};
      case txStatus:
        requireSize(3, true);
        return TransmitStatus{data[1], data[2]};
      case modemStatus:
        requireSize(2, true);
        return ModemStatus{data[1]};
      case atResponse:
        requireSize(5);
        return AtCommandResponse{data[1], {static_cast<char>(data[2]), static_cast<char>(data[3])},
                                 data[4], BytesBuffer(data.begin() + 5, data.end())};
      default:
        break; // Other API indications (e.g. I/O samples) are not PPRZLINK messages.
    }
    return std::nullopt;
  }

  /// @details Consume status frames without exposing them as messages, retain
  /// incomplete envelopes and resynchronize iteratively on invalid candidates.
  /// Payload/callback exceptions consume their complete API frame before propagation.
  bool XbeeTransport::decodeMessage()
  {
    size_t cursor = 0;
    const auto discardThrough = [&](size_t end) {
      transportBuffer.erase(transportBuffer.begin(), transportBuffer.begin() + end);
    };
    while (isRunning() && cursor < transportBuffer.size()) {
      const auto start = std::find(transportBuffer.begin() + cursor, transportBuffer.end(), startByte);
      statistics.discardedBytes += static_cast<size_t>(start - (transportBuffer.begin() + cursor));
      cursor = static_cast<size_t>(start - transportBuffer.begin());
      const auto remaining = std::span<const uint8_t>(transportBuffer).subspan(cursor);
      switch (inspectFrame(remaining)) {
        case FrameStatus::Incomplete:
          discardThrough(cursor);
          return false;
        case FrameStatus::BadLength:
          ++statistics.lengthErrors;
          ++statistics.discardedBytes;
          ++cursor;
          continue;
        case FrameStatus::BadChecksum:
          ++statistics.checksumErrors;
          ++statistics.discardedBytes;
          ++cursor;
          continue;
        case FrameStatus::Complete:
          break;
      }
      const size_t dataSize = frameDataSize(remaining);
      cursor += dataSize + envelopeSize;
      std::optional<RadioStatus> status;
      try {
        status = decodeFrame(remaining.subspan(3, dataSize));
      } catch (...) {
        ++statistics.decodingErrors;
        statistics.discardedBytes += dataSize + envelopeSize;
        discardThrough(cursor);
        throw;
      }
      if (status) {
        // A status callback may restart initialization and clear the buffer.
        // Consume the frame and reset the cursor before calling application code.
        discardThrough(cursor);
        cursor = 0;
        if (statusCallback) {
          try { statusCallback(*status); }
          catch (...) { statusFailure = std::current_exception(); throw; }
        }
        continue;
      }
      if (currentMessage) {
        lastReceivedFrameSize = dataSize + envelopeSize;
        ++statistics.receivedMessages;
        statistics.receivedMessageBytes += lastReceivedFrameSize;
        discardThrough(cursor);
        return true;
      }
    }
    discardThrough(cursor);
    return false;
  }
}
