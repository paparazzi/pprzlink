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
 * @file BinaryCodec.h
 * @brief Little-endian scalars and XML-typed binary fields.
 * @ingroup codecs
 *
 * Dynamic arrays and strings carry one-byte counts; fixed arrays do not. Decoding commits offsets only after a complete field.
 */

#ifndef PPRZLINKCPP_BINARYCODEC_H
#define PPRZLINKCPP_BINARYCODEC_H

#include <pprzlink/FieldValue.h>
#include <bit>
#include <limits>

namespace pprzlink::binary {
  /// @brief Non-bool arithmetic scalars with supported one-, two-, four- or eight-byte representation.
  /// @ingroup codecs
  /// @tparam T Scalar C++ type to encode or decode.
  template<class T>
  concept Scalar = Arithmetic<T> && !std::same_as<T, bool> &&
                   (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

  static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big);

  /// Append the little-endian object representation, including floating-point bits.
  /// @tparam T Supported scalar type.
  /// @param[in,out] buffer Destination byte sequence to append to.
  /// @param[in] value Scalar whose object representation is encoded.
  template<Scalar T>
  void writeLittleEndian(BytesBuffer &buffer, T value)
  {
    auto bytes = std::bit_cast<std::array<uint8_t, sizeof(T)>>(value);
    if constexpr (std::endian::native == std::endian::big) std::ranges::reverse(bytes);
    buffer.insert(buffer.end(), bytes.begin(), bytes.end());
  }

  /// Read one scalar. A truncated buffer throws without changing offset.
  /// @tparam T Supported scalar type to decode.
  /// @param[in] buffer Input byte sequence, in little-endian order.
  /// @param[in,out] offset Cursor advanced by sizeof(T) only on success.
  /// @return Decoded scalar with its floating-point bit representation preserved.
  /// @throws std::out_of_range The cursor or remaining input cannot contain the scalar.
  template<Scalar T>
  [[nodiscard]] T readLittleEndian(std::span<const uint8_t> buffer, size_t &offset)
  {
    if (offset > buffer.size() || sizeof(T) > buffer.size() - offset) {
      throw std::out_of_range("Truncated PprzLink binary field");
    }
    std::array<uint8_t, sizeof(T)> bytes;
    std::copy_n(buffer.begin() + offset, bytes.size(), bytes.begin());
    if constexpr (std::endian::native == std::endian::big) std::ranges::reverse(bytes);
    offset += bytes.size();
    return std::bit_cast<T>(bytes);
  }

  /// Encoded size, including the one-byte count of dynamic arrays/strings.
  /// Counts above 255 and arrays of strings cannot be encoded.
  /// @param[in] field Populated, XML-typed field.
  /// @return Concrete binary data bytes, including any count prefix.
  /// @throws std::length_error A dynamic count exceeds 255.
  /// @throws std::logic_error The field is an array of strings.
  [[nodiscard]] size_t fieldSize(const FieldValue &field);
  /// @brief Validate a field and append its binary encoding.
  /// @param[in,out] buffer Destination to append to, without clearing existing bytes.
  /// @param[in] field Populated field whose XML type selects the representation.
  /// @return Number of appended bytes.
  /// @throws std::exception Field sizing/validation fails before encoding, or allocation fails.
  size_t writeField(BytesBuffer &buffer, const FieldValue &field);

  /// Decode a complete field; offset is only advanced after a successful decode.
  /// @param[in] field Definition copied into the returned value.
  /// @param[in] buffer Little-endian binary data containing this field.
  /// @param[in,out] offset Cursor committed only when the whole field is decoded.
  /// @return Owned definition and decoded value of the XML-selected C++ type.
  /// @throws std::out_of_range A scalar, count or array body is truncated.
  /// @throws std::logic_error Arrays of strings have no binary representation.
  [[nodiscard]] FieldValue readField(const MessageField &field,
                                    std::span<const uint8_t> buffer, size_t &offset);
}
#endif // PPRZLINKCPP_BINARYCODEC_H
