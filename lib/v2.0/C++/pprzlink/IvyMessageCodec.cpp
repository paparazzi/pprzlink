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
 * @file IvyMessageCodec.cpp
 * @brief Ivy regular expressions and checked field parsing.
 * @ingroup codecs
 *
 * XML types determine capture arity. Numeric parsing checks ranges while preserving explicit floating-point NaN/infinity tokens; fixed array lengths are checked by FieldValue.
 */

#include "IvyMessageCodec.h"
#include "TextCodec.h"
#include <charconv>
#include <cmath>
#include <locale>
#include <regex>
#include <sstream>

namespace pprzlink::ivy_codec {
  std::string escapeRegexp(std::string_view text)
  {
    std::string escaped;
    for (char c : text) {
      if (std::string_view(R"(\.^$|()[]{}*+?)").find(c) != std::string_view::npos) {
        escaped += '\\';
      }
      escaped += c;
    }
    return escaped;
  }

  std::string unquote(std::string_view text)
  {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
      text.remove_prefix(1);
      text.remove_suffix(1);
    }
    return std::string(text);
  }

  /// @brief Internal runtime-typed numeric and array parsing.
  /// @ingroup internals
  namespace {
    /// @brief Parse one complete checked numeric token, including legacy separators/bases.
    /// @tparam T Exact XML-selected arithmetic destination type.
    /// @param[in] text Complete token; integer prefixes 0x/0o/0b and underscore separators are supported.
    /// @return A value within T's range, or an explicitly spelled floating-point NaN/infinity.
    /// @throws wrong_message_format Syntax, signedness, bounds or special-number validation fails.
    template<class T>
    T parseNumber(std::string_view text)
    {
      const std::string_view original = text;
      std::string normalized;
      if (text.find('_') != std::string_view::npos) {
        normalized.assign(text);
        std::erase(normalized, '_');
        text = normalized;
      }
      if (text.starts_with('+')) {
        text.remove_prefix(1);
        if (text.starts_with('-')) {
          throw wrong_message_format("Invalid number: " + std::string(original));
        }
      }
      T value{};
      int base = 10;
      bool negative = false;
      if constexpr (std::integral<T>) {
        if (text.starts_with('-')) { negative = true; text.remove_prefix(1); }
        if (text.starts_with("0x") || text.starts_with("0X")) base = 16;
        else if (text.starts_with("0o") || text.starts_with("0O")) base = 8;
        else if (text.starts_with("0b") || text.starts_with("0B")) base = 2;
        if (base != 10) text.remove_prefix(2);
        uint64_t magnitude = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), magnitude, base);
        const uint64_t limit = static_cast<uint64_t>(std::numeric_limits<T>::max()) +
          (negative && std::is_signed_v<T> ? uint64_t{1} : uint64_t{0});
        if (text.empty() || error != std::errc{} || end != text.data() + text.size() ||
            magnitude > limit || (negative && !std::is_signed_v<T>))
          throw wrong_message_format("Invalid or out-of-range number: " + std::string(original));
        if (negative) {
          if (magnitude == limit) return std::numeric_limits<T>::min();
          return static_cast<T>(-static_cast<int64_t>(magnitude));
        }
        return static_cast<T>(magnitude);
      }
      const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
      if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        throw wrong_message_format("Invalid or out-of-range number: " + std::string(original));
      }
      if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value)) {
          const auto magnitude = text.starts_with('-') ? text.substr(1) : text;
          // These are the forms emitted by to_chars and printf, including %F/%E.
          // Overflow remains an error above; arbitrary NaN payload syntax is not wire text.
          if (magnitude != "nan" && magnitude != "NAN" && magnitude != "inf" && magnitude != "INF")
            throw wrong_message_format("Invalid non-finite number: " + std::string(original));
        }
      }
      return value;
    }

    /// @brief Decode a scalar or comma-separated numeric array into its XML field.
    /// @tparam T Exact scalar element type.
    /// @param[in,out] msg Message receiving the decoded value after validation.
    /// @param[in] field Definition selecting scalar versus fixed/dynamic array behavior.
    /// @param[in] text Complete scalar or array text.
    /// @throws std::exception Numeric syntax/ranges, array extent or trailing comma validation fails.
    template<class T>
    void addNumericField(Message &msg, const MessageField &field, std::string_view text)
    {
      if (!field.getType().isArray()) {
        msg.addField(field.getName(), parseNumber<T>(text));
        return;
      }
      std::vector<T> values;
      while (!text.empty()) {
        const auto comma = text.find(',');
        values.push_back(parseNumber<T>(text.substr(0, comma)));
        if (comma == std::string_view::npos) {
          break;
        }
        text.remove_prefix(comma + 1);
        if (text.empty()) {
          throw wrong_message_format("Trailing comma in field " + field.getName());
        }
      }
      msg.addField(field.getName(), values);
    }
  }

  std::string messageRegexp(const MessageDefinition &def)
  {
    static const std::map<BaseType, std::string> typeRegex = {
      {BaseType::CHAR, "."},
      {BaseType::INT8, "[+-]?[0-9]+"},
      {BaseType::INT16, "[+-]?[0-9]+"},
      {BaseType::INT32, "[+-]?[0-9]+"},
      {BaseType::INT64, "[+-]?[0-9]+"},
      {BaseType::UINT8, "[+]?[0-9]+"},
      {BaseType::UINT16, "[+]?[0-9]+"},
      {BaseType::UINT32, "[+]?[0-9]+"},
      {BaseType::UINT64, "[+]?[0-9]+"},
      {BaseType::FLOAT, R"([+-]?(?:(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|nan|NAN|inf|INF))"},
      {BaseType::DOUBLE, R"([+-]?(?:(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|nan|NAN|inf|INF))"},
      {BaseType::STRING, R"((?:"[^"]*"|[^ ]+))"}
    };
    std::ostringstream regexp;
    regexp << escapeRegexp(def.getName());
    for (size_t i = 0; i < def.getNbFields(); ++i) {
      const auto &type = def.getField(i).getType();
      const auto iter = typeRegex.find(type.getBaseType());
      if (iter == typeRegex.end()) {
        throw wrong_message_format("Unknown type for field " + def.getField(i).getName());
      }
      const auto &base = iter->second;
      if (!type.isArray()) {
        regexp << " (" << base << ")";
      } else if (type.getBaseType() == BaseType::CHAR) {
        regexp << R"( ("[^"]*"))";
      } else if (type.getArraySize() == 0) {
        regexp << " ((?:" << base << "(?:," << base << ")*)?)";
      } else {
        regexp << " (" << base << "(?:," << base << "){" << type.getArraySize() - 1 << "})";
      }
    }
    // Legacy senders append a space even for messages with no fields.
    regexp << (def.getNbFields() == 0 ? " *$" : "$");
    return regexp.str();
  }

  std::string serializeMessage(const Message &msg)
  {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    const auto &sender = msg.getSenderId();
    if (std::holds_alternative<std::string>(sender)) {
      stream << std::get<std::string>(sender);
    } else {
      stream << static_cast<unsigned int>(std::get<uint8_t>(sender));
    }
    const auto &def = msg.getDefinition();
    stream << ' ' << def.getName();
    for (size_t i = 0; i < def.getNbFields(); ++i) {
      stream << ' ';
      writeIvyField(stream, msg.getField(i));
    }
    return stream.str();
  }

  Message parseFields(const MessageDefinition &def, std::string_view sender,
                      std::span<const std::string_view> fields)
  {
    if (fields.size() != def.getNbFields()) {
      throw wrong_message_format("Wrong number of fields in message " + def.getName());
    }
    Message msg(def);
    msg.setSenderId(unquote(sender));
    for (size_t i = 0; i < fields.size(); ++i) {
      const auto &field = def.getField(i);
      const auto text = fields[i];
      switch (field.getType().getBaseType()) {
        case BaseType::CHAR:
          if (field.getType().isArray()) {
            msg.addField(field.getName(), unquote(text));
          } else {
            if (text.size() != 1) {
              throw wrong_message_format("Expected one character in " + field.getName());
            }
            msg.addField(field.getName(), text.front());
          }
          break;
        case BaseType::STRING: msg.addField(field.getName(), unquote(text)); break;
        case BaseType::INT8: addNumericField<int8_t>(msg, field, text); break;
        case BaseType::INT16: addNumericField<int16_t>(msg, field, text); break;
        case BaseType::INT32: addNumericField<int32_t>(msg, field, text); break;
        case BaseType::INT64: addNumericField<int64_t>(msg, field, text); break;
        case BaseType::UINT8: addNumericField<uint8_t>(msg, field, text); break;
        case BaseType::UINT16: addNumericField<uint16_t>(msg, field, text); break;
        case BaseType::UINT32: addNumericField<uint32_t>(msg, field, text); break;
        case BaseType::UINT64: addNumericField<uint64_t>(msg, field, text); break;
        case BaseType::FLOAT: addNumericField<float>(msg, field, text); break;
        case BaseType::DOUBLE: addNumericField<double>(msg, field, text); break;
        case BaseType::NOT_A_TYPE:
          throw wrong_message_format("Unknown type for field " + field.getName());
      }
    }
    return msg;
  }

  Message parseMessageBody(const MessageDefinition &definition, std::string_view sender,
                           std::string_view body)
  {
    const std::regex regexp("^" + messageRegexp(definition));
    std::match_results<std::string_view::const_iterator> matches;
    if (!std::regex_match(body.begin(), body.end(), matches, regexp)) {
      throw wrong_message_format("Wrong fields in message " + definition.getName());
    }
    std::vector<std::string_view> fields;
    for (size_t i = 1; i < matches.size(); ++i) {
      fields.emplace_back(matches[i].first, matches[i].second);
    }
    return parseFields(definition, sender, fields);
  }
}
