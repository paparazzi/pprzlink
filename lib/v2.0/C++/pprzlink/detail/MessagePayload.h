// SPDX-License-Identifier: LGPL-3.0-or-later

/**
 * @file MessagePayload.h
 * @brief Transport-independent PPRZLINK v2 headers and XML payloads.
 * @ingroup internals
 *
 * The four-byte header contains sender, receiver, class/component and message ID. A decoded payload must contain exactly the fields declared by its schema.
 */

#pragma once

#include <pprzlink/Message.h>
#include <pprzlink/MessageDictionary.h>
#include <charconv>
#include <span>

namespace pprzlink::detail {
  /// @brief Bytes in the sender/receiver/class-component/message-ID v2 header.
  inline constexpr size_t messageHeaderSize = 4;

  /// @brief Convert a sender representation to its binary byte identifier.
  /// @param[in] sender Numeric byte or complete decimal text in [0, 255].
  /// @return Byte-sized sender ID.
  /// @throws wrong_message_format Text cannot be parsed as a complete decimal byte ID.
  inline uint8_t binarySender(const Message::SenderId &sender)
  {
    if (const auto *id = std::get_if<uint8_t>(&sender)) return *id;
    const auto &text = std::get<std::string>(sender);
    unsigned int id = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), id);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size() || id > 255) {
      throw wrong_message_format("Binary sender must be an integer in [0, 255]: " + text);
    }
    return static_cast<uint8_t>(id);
  }

  /// @brief Encode a v2 header and XML fields without any transport envelope.
  /// @param[in] message Populated message whose identifiers fit their header widths.
  /// @param[in] maximumSize Limit including the four-byte message header.
  /// @return Owned payload with fields in schema order.
  /// @throws std::length_error The message exceeds the selected payload limit.
  /// @throws wrong_message_format Sender, class or component IDs cannot be encoded.
  /// @throws std::exception A field is unset, unsupported or invalid for binary encoding.
  inline BytesBuffer encodeMessagePayload(const Message &message, size_t maximumSize)
  {
    const auto fieldSize = message.getByteSize();
    if (maximumSize < messageHeaderSize || fieldSize > maximumSize - messageHeaderSize) {
      throw std::length_error("PprzLink message exceeds transport payload limit");
    }
    if (message.getClassId() > 0x0f || message.getComponentId() > 0x0f) {
      throw wrong_message_format("Class and component IDs must fit in four bits");
    }
    const auto classComponent = static_cast<uint8_t>(message.getClassId() | (message.getComponentId() << 4));
    BytesBuffer payload{binarySender(message.getSenderId()), message.getReceiverId(),
                        classComponent, message.getDefinition().getId()};
    payload.reserve(messageHeaderSize + fieldSize);
    for (size_t i = 0; i < message.getDefinition().getNbFields(); ++i) message.addFieldToBuffer(i, payload);
    return payload;
  }

  /// @brief Decode one exact v2 message payload using its class/message identifiers.
  /// @param[in] dictionary Borrowed schemas selecting the header's message type.
  /// @param[in] payload Exactly one header and its fields, without delimiter or checksums.
  /// @return Owned message with copied schema, addressing and populated values.
  /// @throws std::exception Truncated/invalid fields, unknown schema or unexpected trailing bytes.
  inline Message decodeMessagePayload(const MessageDictionary &dictionary, std::span<const uint8_t> payload)
  {
    if (payload.size() < messageHeaderSize) throw wrong_message_format("Truncated PprzLink header");
    const auto &definition = dictionary.getDefinition(payload[2] & 0x0f, payload[3]);
    Message message(definition);
    message.setSenderId(payload[0]);
    message.setReceiverId(payload[1]);
    message.setComponentId(payload[2] >> 4);
    size_t offset = messageHeaderSize;
    for (size_t i = 0; i < definition.getNbFields(); ++i) message.addFieldFromBuffer(i, payload, offset);
    if (offset != payload.size()) {
      throw wrong_message_format("Unexpected trailing payload in " + definition.getName());
    }
    return message;
  }
}
