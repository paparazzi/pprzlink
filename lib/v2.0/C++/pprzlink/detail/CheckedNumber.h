// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file CheckedNumber.h
 * @brief Shared range-checked numeric conversions and type descriptions.
 * @ingroup internals
 *
 * Integral destinations reject fractions and non-finite values. Floating-point conversions may round; same-type values retain their representation.
 */

#pragma once
#include <pprzlink/exceptions/pprzlink_exception.h>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <format>
#include <limits>
#include <string_view>
#include <type_traits>

namespace pprzlink::detail {
  /// @brief Describe a C++ value type for conversion diagnostics.
  /// @ingroup internals
  /// @tparam T Numeric, string or container type to describe.
  /// @return Readable type name; containers append [] to their element description.
  template<class T>
  std::string valueTypeName()
  {
    if constexpr (std::same_as<T, bool>) return "bool";
    else if constexpr (std::same_as<T, char>) return "char";
    else if constexpr (std::integral<T>)
      return std::format("{}int{}", std::is_signed_v<T> ? "" : "u", sizeof(T) * 8);
    else if constexpr (std::same_as<T, float>) return "float";
    else if constexpr (std::same_as<T, double>) return "double";
    else if constexpr (std::same_as<T, long double>) return "long double";
    else if constexpr (std::same_as<T, std::string>) return "string";
    else if constexpr (requires { typename T::value_type; })
      return valueTypeName<typename T::value_type>() + "[]";
    else return "requested C++ type";
  }

  /// Checked numeric conversion shared by writes and explicit converted reads.
  /// Floating-point destinations may round; integer destinations require an exact integer.
  /// @ingroup internals
  /// @tparam To Arithmetic destination type.
  /// @tparam From Arithmetic input type.
  /// @param[in] input Original value; same-type conversion preserves it without range conversion.
  /// @param[in] fieldName Diagnostic context included in a conversion error.
  /// @return Checked conversion; floating-point destinations can round and preserve non-finite values.
  /// @throws field_conversion_error A value is out of range, fractional or non-finite for an integer.
  template<class To, class From>
    requires std::is_arithmetic_v<To> && std::is_arithmetic_v<From>
  To checkedNumber(From input, std::string_view fieldName)
  {
    const auto reject = [&](std::string_view reason) -> void {
      throw field_conversion_error(std::format("Field '{}': cannot convert {} from {} to {} ({})",
        fieldName, input, valueTypeName<From>(), valueTypeName<To>(), reason));
    };

    if constexpr (std::same_as<To, From>) {
      return input; // In particular, preserve floating-point NaN bit patterns on binary reception.
    } else if constexpr (std::integral<To> && std::integral<From>) {
      // intmax_t/uintmax_t also handle char and bool, which std::in_range excludes.
      if constexpr (std::is_signed_v<From>) {
        const auto value = static_cast<intmax_t>(input);
        if constexpr (std::is_signed_v<To>) {
          if (value < std::numeric_limits<To>::min() || value > std::numeric_limits<To>::max())
            reject("out of range");
        } else if (value < 0 || static_cast<uintmax_t>(value) > std::numeric_limits<To>::max()) {
          reject("out of range");
        }
      } else if (static_cast<uintmax_t>(input) > static_cast<uintmax_t>(std::numeric_limits<To>::max())) {
        reject("out of range");
      }
    } else if constexpr (std::integral<To>) {
      const auto value = static_cast<long double>(input);
      if (!std::isfinite(value) || std::trunc(value) != value) reject("not a finite integer");
      // Use an exclusive power-of-two upper bound. Converting UINT64_MAX to
      // double first can round to 2^64 and incorrectly admit an undefined cast.
      const auto upper = std::ldexp(1.0L, std::numeric_limits<To>::digits);
      const auto lower = std::is_signed_v<To> ? -upper : 0.0L;
      if (value < lower || value >= upper || (std::same_as<To, bool> && value > 1)) reject("out of range");
    } else {
      const auto value = static_cast<long double>(input);
      const auto limit = static_cast<long double>(std::numeric_limits<To>::max());
      if (std::isfinite(value) && (value < -limit || value > limit)) reject("out of range");
    }
    return static_cast<To>(input);
  }
}
