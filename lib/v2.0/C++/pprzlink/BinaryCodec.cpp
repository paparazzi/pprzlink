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
 * @file BinaryCodec.cpp
 * @brief Binary field sizing, validation and encoding.
 * @ingroup codecs
 *
 * Lengths and unsupported string arrays are checked before encoding. A local cursor keeps the caller offset unchanged when a field is truncated.
 */

#include <pprzlink/BinaryCodec.h>

namespace pprzlink::binary {
  /// @brief Internal binary extent and count validation.
  /// @ingroup internals
  namespace {
    /// @brief Determine whether this concrete value needs a one-byte count prefix.
    /// @param[in] field Populated XML-typed field.
    /// @return True for scalar strings or dynamic arrays, false for scalar numbers and fixed arrays.
    bool hasCount(const FieldValue &field)
    {
      const auto &type = field.getType();
      return !type.isArray() ? type.getBaseType() == BaseType::STRING : type.getArraySize() == 0;
    }

    /// @brief Check a complete array body without overflowing count * elementSize.
    /// @param[in] buffer Input binary bytes.
    /// @param[in] offset Cursor at the first element.
    /// @param[in] count Number of elements to read.
    /// @param[in] elementSize Positive byte width of one element.
    /// @throws std::out_of_range The cursor or array extent exceeds the remaining input.
    void checkRemaining(std::span<const uint8_t> buffer, size_t offset, size_t count, size_t elementSize)
    {
      if (offset > buffer.size() || count > (buffer.size() - offset) / elementSize) {
        throw std::out_of_range("Truncated PprzLink binary field");
      }
    }
  }

  size_t fieldSize(const FieldValue &field)
  {
    return std::visit([&]<class T>(const T &value) -> size_t {
      if constexpr (Arithmetic<T>) {
        return sizeof(T);
      } else if constexpr (std::same_as<T, std::vector<std::string>>) {
        throw std::logic_error("Arrays of strings have no PprzLink binary representation");
      } else {
        const bool count = hasCount(field);
        if (count && value.size() > std::numeric_limits<uint8_t>::max()) {
          throw std::length_error("Binary field " + field.getName() + " exceeds 255 elements");
        }
        return value.size() * sizeof(typename T::value_type) + (count ? 1 : 0);
      }
    }, field.getValue());
  }

  size_t writeField(BytesBuffer &buffer, const FieldValue &field)
  {
    const auto size = fieldSize(field); // Validate before appending any bytes.
    std::visit([&]<class T>(const T &value) {
      if constexpr (Arithmetic<T>) {
        writeLittleEndian(buffer, value);
      } else if constexpr (!std::same_as<T, std::vector<std::string>>) {
        if (hasCount(field)) writeLittleEndian(buffer, static_cast<uint8_t>(value.size()));
        for (const auto &element : value) writeLittleEndian(buffer, element);
      }
    }, field.getValue());
    return size;
  }

  /// @details Decode through a local cursor and construct the complete FieldValue
  /// before changing offset. Fixed arrays use their schema extent; strings and
  /// dynamic arrays read their count from the input.
  FieldValue readField(const MessageField &field, std::span<const uint8_t> buffer, size_t &offset)
  {
    auto cursor = offset;
    const auto &type = field.getType();
    auto result = detail::visitBaseType(type.getBaseType(), [&]<class T>() -> FieldValue {
      if constexpr (std::same_as<T, std::string>) {
        if (type.isArray()) {
          throw std::logic_error("Arrays of strings have no PprzLink binary representation");
        }
        const auto count = readLittleEndian<uint8_t>(buffer, cursor);
        checkRemaining(buffer, cursor, count, 1);
        std::string text;
        text.reserve(count);
        for (size_t i = 0; i < count; ++i) text += readLittleEndian<char>(buffer, cursor);
        return FieldValue(field, text);
      } else {
        if (!type.isArray()) return FieldValue(field, readLittleEndian<T>(buffer, cursor));
        const auto count = type.getArraySize() == 0
          ? readLittleEndian<uint8_t>(buffer, cursor) : type.getArraySize();
        checkRemaining(buffer, cursor, count, sizeof(T));
        std::vector<T> values;
        values.reserve(count);
        for (size_t i = 0; i < count; ++i) values.push_back(readLittleEndian<T>(buffer, cursor));
        return FieldValue(field, values);
      }
    });
    offset = cursor;
    return result;
  }
}
