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
 * @file MessageFieldTypes.cpp
 * @brief XML type-name parsing and canonical type spelling.
 * @ingroup messages
 *
 * Array extents must be positive decimal integers or an empty dynamic suffix. Size overflow and unsupported base names are rejected.
 */

#include "MessageFieldTypes.h"
#include <pprzlink/exceptions/pprzlink_exception.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <limits>
#include <string_view>

namespace pprzlink {
  /// @brief Internal canonical base-type names and fixed widths.
  /// @ingroup internals
  namespace {
    /// @brief One entry in the base-type descriptor table.
    struct TypeInfo {
      BaseType type; ///< XML base-type enumerator.
      std::string_view name; ///< Canonical lowercase XML spelling.
      size_t size; ///< Fixed element bytes, or zero for a scalar string.
    };
    /// @brief Supported names/widths in one table shared by parsing and diagnostics.
    constexpr std::array types{
      TypeInfo{BaseType::CHAR, "char", 1},
      TypeInfo{BaseType::INT8, "int8", 1},
      TypeInfo{BaseType::INT16, "int16", 2},
      TypeInfo{BaseType::INT32, "int32", 4},
      TypeInfo{BaseType::INT64, "int64", 8},
      TypeInfo{BaseType::UINT8, "uint8", 1},
      TypeInfo{BaseType::UINT16, "uint16", 2},
      TypeInfo{BaseType::UINT32, "uint32", 4},
      TypeInfo{BaseType::UINT64, "uint64", 8},
      TypeInfo{BaseType::FLOAT, "float", 4},
      TypeInfo{BaseType::DOUBLE, "double", 8},
      TypeInfo{BaseType::STRING, "string", 0}
    };

    /// @brief Borrow the canonical descriptor for a valid base type.
    /// @param[in] type Base type to resolve.
    /// @return Static-lifetime table entry.
    /// @throws std::logic_error The type is unknown or NOT_A_TYPE.
    const TypeInfo &typeInfo(BaseType type)
    {
      const auto found = std::ranges::find(types, type, &TypeInfo::type);
      if (found == types.end()) throw std::logic_error("Unknown PprzLink base type");
      return *found;
    }
  }

  FieldType::FieldType(const std::string &typeString) : baseType(BaseType::NOT_A_TYPE)
  {
    const std::string_view text(typeString);
    const auto bracket = text.find('[');
    const auto found = std::ranges::find(types, text.substr(0, bracket), &TypeInfo::name);
    const auto invalid = [&] { return bad_message_file("Invalid field type '" + typeString + "'"); };
    if (found == types.end()) throw invalid();
    baseType = found->type;
    if (bracket == std::string_view::npos) return; // Scalar.
    if (!text.ends_with(']')) throw invalid();
    const auto count = text.substr(bracket + 1, text.size() - bracket - 2);
    if (count.empty()) {
      arraySize = 0; // Dynamic array: [] only, never [0].
      return;
    }
    size_t size = 0;
    const auto [end, error] = std::from_chars(count.data(), count.data() + count.size(), size);
    if (error != std::errc{} || end != count.data() + count.size() || size == 0 ||
        size > std::numeric_limits<size_t>::max() / std::max(size_t{1}, found->size)) {
      throw invalid();
    }
    arraySize = size;
  }

  BaseType FieldType::getBaseType() const { return baseType; }
  bool FieldType::isArray() const { return arraySize.has_value(); }

  size_t FieldType::getArraySize() const
  {
    if (!arraySize) throw std::logic_error("Scalar field has no array size");
    return *arraySize;
  }

  std::string FieldType::toString() const
  {
    const auto name = typeInfo(baseType).name;
    if (!arraySize) return std::string(name);
    if (*arraySize == 0) return std::format("{}[]", name);
    return std::format("{}[{}]", name, *arraySize);
  }

  size_t sizeofBaseType(BaseType type) { return typeInfo(type).size; }
}
