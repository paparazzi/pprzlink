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
 * @file MessageField.h
 * @brief Named XML fields, unit metadata and explicit coherent SI conversions.
 * @ingroup messages
 *
 * Fixed data size excludes variable-length count prefixes. XML display formats affect only the legacy Ivy serializer.
 */

#ifndef PPRZLINKCPP_MESSAGEFIELD_H
#define PPRZLINKCPP_MESSAGEFIELD_H

#include <string>
#include <memory>
#include <optional>
#include <cstdint>
#include <cmath>
#include <type_traits>
#include <pprzlink/MessageFieldTypes.h>
#include <pprzlink/detail/CheckedNumber.h>

namespace pprzlink {
  namespace detail { struct UnitConversion; }

  /// A field definition: its name and XML type. FieldValue holds an actual value.
  /// @ingroup messages
  class MessageField {
  public:
    /// @brief Copy a field name and an already parsed type.
    /// @param[in] name XML field name.
    /// @param[in] type Parsed type whose definition is copied.
    MessageField(const std::string &name, const FieldType &type);

    /// @brief Define a field from XML type text and an optional legacy display format.
    /// @param[in] name XML field name.
    /// @param[in] typeString Type declaration accepted by FieldType.
    /// @param[in] format Optional XML numeric display format; empty uses legacy defaults.
    /// @param[in] unit Original XML unit spelling; empty is unspecified.
    /// @param[in] altUnit Original alternative unit spelling; empty is unspecified.
    /// @param[in] altUnitCoef Explicit XML-to-alternative number scale; absent is distinct from 1.
    /// @param[in] messageName Optional owning message name for aliases and conversion diagnostics.
    /// @throws bad_message_file The type declaration is invalid.
    /// @throws bad_message_file An explicit coefficient is non-finite or zero.
    /// @throws bad_message_file A declared unit is unknown or incompatible with its alternative.
    MessageField(const std::string &name, const std::string &typeString,
                 std::string format = {}, std::string unit = {}, std::string altUnit = {},
                 std::optional<double> altUnitCoef = std::nullopt, std::string messageName = {});

    /// @brief Borrow the field name.
    /// @return Name reference valid for this definition's lifetime.
    [[nodiscard]] const std::string &getName() const;

    /// @brief Borrow the parsed type definition.
    /// @return Type reference valid for this definition's lifetime.
    [[nodiscard]] const FieldType &getType() const;

    /// Fixed data size; zero for strings/dynamic arrays. Excludes count prefixes.
    /// @return Fixed schema data bytes, not the size of a concrete encoded FieldValue.
    [[nodiscard]] size_t getSize() const;
    /// Optional XML display format; interpreted only by the legacy Ivy serializer.
    /// @return Borrowed format string; an empty value requests the serializer's default.
    [[nodiscard]] const std::string &getFormat() const noexcept { return format; }
    /// Borrow the original XML unit; empty means unspecified, not automatically dimensionless.
    /// @return Original spelling valid for this definition's lifetime.
    [[nodiscard]] const std::string &getUnit() const noexcept { return unit; }
    /// Borrow the original alternative unit; empty means unspecified.
    /// @return Original spelling valid for this definition's lifetime.
    [[nodiscard]] const std::string &getAltUnit() const noexcept { return altUnit; }
    /// Inspect only the coefficient explicitly written in the XML.
    /// @return Explicit coefficient, or nullopt; no inferred default is supplied.
    [[nodiscard]] std::optional<double> getAltUnitCoef() const noexcept { return altUnitCoef; }
    /// Borrow the coherent SI symbol (m, rad, K, etc.), or an empty string if unavailable.
    /// @return Symbol valid for this definition's lifetime.
    [[nodiscard]] const std::string &getSIUnit() const noexcept;
    /// Numeric scalars and homogeneous numeric arrays can share the same unit conversion.
    /// @return True when explicit metadata resolves to a supported coherent SI unit.
    [[nodiscard]] bool canConvertSI() const noexcept { return static_cast<bool>(unitConversion); }

    /// Convert a double in coherent SI units into the exact XML scalar/array-element type.
    /// Integer destinations round to nearest, with ties away from zero; no saturation.
    /// @tparam T Exact XML scalar or array-element arithmetic type.
    /// @param[in] value SI value (angles in radians, absolute temperatures in kelvins).
    /// @return Converted number in XML units and type.
    /// @throws field_type_mismatch T differs from the XML base type.
    /// @throws field_unit_error SI conversion is unsupported.
    /// @throws field_conversion_error The converted value exceeds the destination range.
    template<class T> requires std::is_arithmetic_v<T>
    [[nodiscard]] T fromSI(double value) const
    {
      checkNativeType<T>();
      const double native = valueFromSI(value);
      if constexpr (std::is_integral_v<T>) return detail::checkedNumber<T>(std::round(native), name);
      else return detail::checkedNumber<T>(native, name);
    }

    /// Convert an exact XML scalar/array-element value to a double in coherent SI units.
    /// Wide integers may lose precision when represented by double; raw getters remain exact.
    /// @tparam T Exact XML scalar or array-element arithmetic type.
    /// @param[in] value Number in XML units and type.
    /// @return SI value (angles in radians, absolute temperatures in kelvins).
    /// @throws field_type_mismatch T differs from the XML base type.
    /// @throws field_unit_error SI conversion is unsupported.
    /// @throws field_conversion_error A finite XML value overflows the SI double.
    template<class T> requires std::is_arithmetic_v<T>
    [[nodiscard]] double toSI(T value) const
    {
      checkNativeType<T>();
      return valueToSI(static_cast<double>(value));
    }
  private:
    std::string name;
    FieldType type;
    size_t size;
    std::string format;
    std::string unit;
    std::string altUnit;
    std::optional<double> altUnitCoef;
    std::string messageName;
    std::shared_ptr<const detail::UnitConversion> unitConversion;

    /// Require the same arithmetic alternative as an exact FieldValue read.
    template<class T> void checkNativeType() const
    {
      const auto base = type.getBaseType();
      const bool matches =
        (base == BaseType::CHAR && std::is_same_v<T, char>) ||
        (base == BaseType::INT8 && std::is_same_v<T, int8_t>) ||
        (base == BaseType::INT16 && std::is_same_v<T, int16_t>) ||
        (base == BaseType::INT32 && std::is_same_v<T, int32_t>) ||
        (base == BaseType::INT64 && std::is_same_v<T, int64_t>) ||
        (base == BaseType::UINT8 && std::is_same_v<T, uint8_t>) ||
        (base == BaseType::UINT16 && std::is_same_v<T, uint16_t>) ||
        (base == BaseType::UINT32 && std::is_same_v<T, uint32_t>) ||
        (base == BaseType::UINT64 && std::is_same_v<T, uint64_t>) ||
        (base == BaseType::FLOAT && std::is_same_v<T, float>) ||
        (base == BaseType::DOUBLE && std::is_same_v<T, double>);
      if (!matches) throw field_type_mismatch(std::format("{}: XML type {}, requested {}",
        unitContext(), type.toString(), detail::valueTypeName<T>()));
    }
    /// Format message, field and original XML unit for conversion errors.
    std::string unitContext() const;
    /// Convert a raw numeric value using the prepared immutable units.
    double valueToSI(double value) const;
    /// Convert an SI value before native integer rounding/range checks.
    double valueFromSI(double value) const;
  };
}

#endif // PPRZLINKCPP_MESSAGEFIELD_H
