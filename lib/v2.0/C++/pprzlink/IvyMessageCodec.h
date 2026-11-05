/*
 * Copyright 2019 garciafa
 * This file is part of PprzLinkCPP
 *
 * PprzLinkCPP is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PprzLinkCPP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with ModemTester.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/**
 * @file IvyMessageCodec.h
 * @brief Runtime XML schemas encoded as Ivy message text.
 * @ingroup codecs
 *
 * Text codecs operate without a bus. Standard and legacy OCaml-compatible formats are distinct entry points.
 */

#ifndef PPRZLINKCPP_IVYMESSAGECODEC_H
#define PPRZLINKCPP_IVYMESSAGECODEC_H

#include <pprzlink/Message.h>
#include <span>
#include <string_view>

/// Runtime XML message encoding/decoding, independent of an Ivy bus or event loop.
namespace pprzlink::ivy_codec {
  /// @brief Escape regex metacharacters so a string can be matched literally.
  /// @param[in] text Sender or message-name text, not an existing regular expression.
  /// @return Escaped text suitable for insertion into an Ivy regex.
  std::string escapeRegexp(std::string_view text);
  /// @brief Remove one pair of outer double quotes, when present.
  /// @param[in] text Quoted or unquoted text; backslash escapes are not interpreted.
  /// @return An owned copy of the unquoted text.
  std::string unquote(std::string_view text);
  /// Message name and capturing groups for XML fields, followed by the end anchor.
  /// @param[in] definition Schema selecting field types and capture count.
  /// @return Regex body without a leading sender capture or start anchor.
  /// @throws wrong_message_format An XML base type cannot be represented.
  std::string messageRegexp(const MessageDefinition &definition);
  /// Sender, message name and fields in Ivy wire format.
  /// Numeric text is locale-independent; finite floats use shortest round-trippable
  /// representations, including scientific notation when shorter.
  /// Floating-point NaN/infinity values use explicit text tokens; NaN payload bits
  /// are not preserved by text serialization.
  /// @param[in] message Populated message; fields are emitted in XML order.
  /// @return Text containing sender, name and field values, without a newline.
  /// @throws std::exception A declared field has no value or a formatting operation fails.
  std::string serializeMessage(const Message &message);
  /// OCaml-compatible XML display formats, including its unquoted empty strings.
  /// @param[in] message Populated message whose field formats are checked before use.
  /// @return Sender, name and fields in the legacy text representation.
  /// @throws wrong_message_format An XML numeric display format is unsupported or too large.
  /// @throws std::exception A declared field has no value or formatting fails.
  std::string serializeLegacyMessage(const Message &message);
  /// OCaml-style whitespace and quoted/pipe-delimited fields; numeric values remain checked.
  /// @param[in] definition Expected schema copied into the result.
  /// @param[in] sender Sender text stored separately from body.
  /// @param[in] body Message name and fields, without the sender.
  /// @return Complete message with a string sender identifier.
  /// @throws std::exception Bad names, quoting, arity, numeric values or array sizes.
  Message parseLegacyMessageBody(const MessageDefinition &definition, std::string_view sender,
                                std::string_view body);
  /// Decode captures supplied by a dynamic Ivy binding.
  /// Float/double scalars and arrays accept nan/inf, optional signs and uppercase
  /// NAN/INF. Integer fields reject these tokens; numeric overflow remains an error.
  /// @param[in] definition Expected schema copied into the result.
  /// @param[in] sender Sender capture; optional outer double quotes are removed.
  /// @param[in] fields One capture per XML field, in schema order.
  /// @return Populated message with a string sender identifier.
  /// @throws std::exception Bad arity, types, counts or invalid/out-of-range text numbers.
  Message parseFields(const MessageDefinition &definition, std::string_view sender,
                      std::span<const std::string_view> fields);
  /// Match and decode a message body (name and fields, without sender).
  /// @param[in] definition Expected schema copied into the result.
  /// @param[in] sender Sender text supplied separately from body.
  /// @param[in] body Full message name and field text matched against messageRegexp().
  /// @return Populated message with a string sender identifier.
  /// @throws std::exception Regex mismatch or field parsing fails.
  Message parseMessageBody(const MessageDefinition &definition, std::string_view sender,
                           std::string_view body);
}
#endif // PPRZLINKCPP_IVYMESSAGECODEC_H
