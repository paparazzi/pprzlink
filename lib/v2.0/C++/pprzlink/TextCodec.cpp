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
 * @file TextCodec.cpp
 * @brief Variant-aware formatting of scalar and array values.
 * @ingroup codecs
 *
 * Ivy numbers use locale-independent charconv formatting, with shortest round-trippable floating-point text. Diagnostic formatting uses a temporary stream to preserve caller state.
 */

#include <pprzlink/TextCodec.h>
#include <array>
#include <charconv>
#include <iomanip>

namespace pprzlink {
  /// @brief Internal field formatting without changing the caller stream's format state.
  /// @ingroup internals
  namespace {
    /// @brief Array punctuation policy for wire text versus diagnostic text.
    enum class TextFormat {
      Ivy, ///< Comma-separated arrays without numeric array braces.
      Debug ///< Diagnostic numeric/string arrays enclosed in braces.
    };

    /// @brief Append an Ivy number without locale formatting or temporary allocations.
    /// @tparam T Stored numeric type; floating-point values keep their original precision.
    /// @param[in,out] stream Destination for the complete numeric token.
    /// @param[in] value Number to render; finite floats use their shortest round-trippable form.
    /// @throws std::runtime_error The fixed buffer cannot hold the rendered number.
    template<class T>
    void writeIvyNumber(std::ostream &stream, T value)
    {
      // 64 bytes cover shortest float/double text and all XML integer widths.
      std::array<char, 64> buffer;
      const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
      if (error != std::errc{}) throw std::runtime_error("Cannot format Ivy number");
      stream.write(buffer.data(), end - buffer.data());
    }

    /// @brief Append one scalar or string using the selected text policy.
    /// @tparam T Stored scalar/string type.
    /// @param[in,out] stream Temporary formatting stream.
    /// @param[in] value Read-only value; int8/uint8 are promoted for numeric printing.
    /// @param[in] format Quoting policy for strings and wire/diagnostic output.
    template<class T>
    void writeElement(std::ostream &stream, const T &value, TextFormat format)
    {
      if constexpr (Arithmetic<T> && !std::same_as<T, char>) {
        if (format == TextFormat::Ivy) {
          writeIvyNumber(stream, value);
        } else if constexpr (std::same_as<T, int8_t> || std::same_as<T, uint8_t>) {
          stream << static_cast<int>(value);
        } else if constexpr (std::floating_point<T>) {
          stream << std::fixed << value;
        } else {
          stream << value;
        }
      } else if constexpr (std::same_as<T, std::string>) {
        const bool quoted = format == TextFormat::Ivy &&
                            (value.empty() || value.find(' ') != std::string::npos);
        if (quoted) stream.put('"');
        stream.write(value.data(), value.size());
        if (quoted) stream.put('"');
      } else {
        stream.put(value);
      }
    }

    /// @brief Append Ivy text directly; isolate diagnostic formatting in a temporary stream.
    /// @param[in,out] stream Destination whose formatting flags/locale/precision remain unchanged.
    /// @param[in] storage Read-only scalar or array alternative.
    /// @param[in] format Array punctuation and string quoting policy.
    /// @return The destination after appending the completed text.
    std::ostream &writeField(std::ostream &stream, const FieldValue::Storage &storage, TextFormat format)
    {
      const auto append = [&](std::ostream &text) {
        std::visit([&]<class T>(const T &value) {
          if constexpr (Arithmetic<T> || std::same_as<T, std::string>) {
            writeElement(text, value, format);
          } else if constexpr (std::same_as<T, std::vector<char>>) {
            text.put('"');
            if (!value.empty()) text.write(value.data(), value.size());
            text.put('"');
          } else {
            if (format == TextFormat::Debug) text.put('{');
            bool first = true;
            for (const auto &element : value) {
              if (!first) text.put(',');
              first = false;
              writeElement(text, element, format);
            }
            if (format == TextFormat::Debug) text.put('}');
          }
        }, storage);
      };
      if (format == TextFormat::Ivy) {
        append(stream);
        return stream;
      }
      std::ostringstream text;
      text.imbue(stream.getloc());
      text.precision(stream.precision());
      append(text);
      return stream << text.str();
    }
  }

  std::ostream &writeIvyField(std::ostream &stream, const FieldValue::Storage &value)
  {
    return writeField(stream, value, TextFormat::Ivy);
  }

  std::ostream &writeDebugField(std::ostream &stream, const FieldValue::Storage &value)
  {
    return writeField(stream, value, TextFormat::Debug);
  }

  std::ostream &writeIvyField(std::ostream &stream, const FieldValue &field)
  {
    return writeIvyField(stream, field.getValue());
  }

  std::ostream &writeDebugField(std::ostream &stream, const FieldValue &field)
  {
    return writeDebugField(stream, field.getValue());
  }
}

std::ostream &operator<<(std::ostream &stream, const pprzlink::FieldValue &field)
{
  return pprzlink::writeDebugField(stream, field);
}
