// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file PprzFrameCodec.h
 * @brief PPRZLINK v2 serial envelopes and incremental frame decoding.
 * @ingroup codecs
 *
 * Frames use delimiter 0x99, a one-byte total length and two checksum bytes. The same decoder is reused by streams and UDP.
 */

#pragma once

#include <pprzlink/MessageDictionary.h>
#include <pprzlink/Message.h>
#include <pprzlink/TransportStatistics.h>
#include <pprzlink/ReceivedMessage.h>
#include <optional>
#include <span>

namespace pprzlink {
  /// Encode one PPRZLINK v2 serial frame without performing I/O.
  /// @ingroup codecs
  /// @param[in] message Fully populated message with byte-sized sender/receiver IDs and four-bit class/component IDs.
  /// @return Owned frame containing delimiter, total length, payload and both checksum bytes.
  /// @throws field_has_no_value At least one declared field is unset.
  /// @throws std::length_error The complete frame exceeds 255 bytes or a field exceeds its wire limit.
  /// @throws wrong_message_format A sender or class/component identifier cannot be encoded.
  /// @throws std::logic_error A field type has no binary representation.
  BytesBuffer encodePprzFrame(const Message &message);

  /// Incremental framing shared by serial streams and UDP. The dictionary is borrowed.
  /// @ingroup codecs
  /// Calls on a decoder must be serialized by its owner.
  class PprzFrameDecoder {
  public:
    /// @brief Create an empty decoder borrowing its dictionary.
    /// @param[in] dictionary Schemas that must remain alive and unchanged for this decoder's lifetime.
    explicit PprzFrameDecoder(const MessageDictionary &dictionary) : dictionary(dictionary) {}
    /// @brief Append input without trying to decode it.
    /// @param[in] bytes Raw bytes copied into the pending input buffer.
    void pushBytes(std::span<const uint8_t> bytes);
    /// Invalid payloads throw after consuming their frame; a later call can continue.
    /// Noise, bad lengths and bad checksums are discarded; incomplete input remains buffered.
    /// @return One complete message, or std::nullopt when no complete frame remains.
    /// @throws no_such_message A valid frame references an unknown schema.
    /// @throws std::exception A valid envelope contains invalid field data or trailing payload.
    std::optional<Message> nextMessage();
    /// @brief Consume a message together with its complete frame size.
    /// @return An owned result without peer/radio metadata, or std::nullopt.
    /// @throws std::exception The same payload errors as nextMessage().
    [[nodiscard]] std::optional<ReceivedMessage> tryReceive()
    {
      auto message = nextMessage();
      if (!message) return std::nullopt;
      return ReceivedMessage{std::move(*message), lastReceivedFrameSize, std::nullopt, std::nullopt};
    }
    /// End a datagram; do not concatenate it with bytes from the next datagram.
    /// Every remaining byte is added to TransportStatistics::discardedBytes.
    void discardPendingInput();
    /// @brief Borrow cumulative validation and recovery counters.
    /// @return Reference updated by later decoding, valid for this decoder's lifetime.
    const TransportStatistics &getStatistics() const noexcept { return statistics; }
    /// @brief Inspect the last successfully decoded frame length.
    /// @return Complete frame bytes, or zero before the first successful receive.
    size_t getLastReceivedFrameSize() const noexcept { return lastReceivedFrameSize; }

  private:
    const MessageDictionary &dictionary;
    BytesBuffer buffer;
    TransportStatistics statistics;
    size_t lastReceivedFrameSize = 0;
  };
}
