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
 * @file MessageFieldTypes.h
 * @brief PPRZLINK base types and scalar/fixed/dynamic array schemas.
 * @ingroup messages
 *
 * A scalar has no array extent, zero denotes an unspecified dynamic extent, and a positive extent denotes a fixed array.
 */

#ifndef PPRZLINKCPP_MESSAGEFIELDTYPES_H
#define PPRZLINKCPP_MESSAGEFIELDTYPES_H

#include <string>
#include <optional>

namespace pprzlink {

  /// @brief Scalar element types recognized in an XML field declaration.
  /// @ingroup messages
  enum class BaseType {
    NOT_A_TYPE, ///< Invalid/uninitialized base type; not a wire type.
    CHAR, ///< One character byte.
    INT8, ///< Signed eight-bit integer.
    INT16, ///< Signed sixteen-bit integer.
    INT32, ///< Signed thirty-two-bit integer.
    UINT8, ///< Unsigned eight-bit integer.
    UINT16, ///< Unsigned sixteen-bit integer.
    UINT32, ///< Unsigned thirty-two-bit integer.
    FLOAT, ///< Four-byte floating-point value.
    DOUBLE, ///< Eight-byte floating-point value.
    STRING, ///< Variable-length text with a one-byte binary count.
    INT64, ///< Signed sixty-four-bit integer.
    UINT64 ///< Unsigned sixty-four-bit integer.
  };

  /// @brief Obtain the fixed data width of one base-type element.
  /// @param[in] type Valid scalar base type.
  /// @return Data bytes per element; zero for variable-length strings.
  /// @throws std::logic_error The type is NOT_A_TYPE or otherwise unknown.
  size_t sizeofBaseType(BaseType type);

  /// An XML type (e.g. float, int16[] or char[5]), without a field name or value.
  /// @ingroup messages
  class FieldType {
  public:
    /// @brief Parse a scalar, fixed array or dynamic array type declaration.
    /// @param[in] typeString Canonical base name with an optional [N] or [] suffix.
    /// @throws bad_message_file Unknown base name, malformed extent, zero fixed extent or size overflow.
    explicit FieldType(const std::string &typeString);

    /// @brief Inspect the scalar element type.
    /// @return The base type independent of any array suffix.
    [[nodiscard]] BaseType getBaseType() const;

    /// @brief Distinguish scalar fields from arrays.
    /// @return True for both fixed and dynamic arrays.
    [[nodiscard]] bool isArray() const;

    /// Zero denotes a dynamic array; calling this on a scalar throws logic_error.
    /// @return Fixed element count, or zero for an unspecified dynamic extent.
    /// @throws std::logic_error This field is scalar.
    [[nodiscard]] size_t getArraySize() const;

    /// @brief Render the canonical XML type spelling.
    /// @return Base name followed by [] or [N] for an array.
    [[nodiscard]] std::string toString() const;

  private:
    BaseType baseType;
    std::optional<size_t> arraySize; // nullopt: scalar; 0: dynamic; positive: fixed.
  };
}
#endif // PPRZLINKCPP_MESSAGEFIELDTYPES_H
