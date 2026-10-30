// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file UnitConversion.h
 * @brief Private affine unit rules and immutable conversions shared by schema copies.
 * @ingroup internals
 */
#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace pprzlink::detail {
  /// One editable XML-to-SI affine rule; the most specific matching scope wins.
  struct UnitAlias {
    std::string_view xml; ///< Exact Paparazzi unit spelling.
    std::string_view si; ///< Canonical coherent SI symbol/dimension; empty disables conversion.
    double multiplier = 1.0; ///< XML-to-SI slope, finite and nonzero.
    double offset = 0.0; ///< SI intercept, added after multiplying the XML number.
    std::string_view message = {}; ///< Optional message selector; empty matches all messages.
    std::string_view field = {}; ///< Optional field selector; empty matches all fields.
    unsigned int prefixPower = 0; ///< SI prefixes allowed when 1..3; scales areas/volumes by the proper power.
  };

  /// Borrow the immutable rules in UnitAliases.cpp; specific selectors override general rules.
  /// @return Rules with process lifetime, without external parser or registry state.
  std::span<const UnitAlias> unitAliases() noexcept;

  /// Prepared affine conversion, including any explicit XML coefficient.
  struct UnitConversion {
    double multiplier = 1.0; ///< XML-to-SI slope after composing metadata.
    double offset = 0.0; ///< SI intercept, unaffected by an XML number coefficient.
    std::string siSymbol; ///< Readable coherent SI symbol, including "1" for dimensionless values.
  };

  /// Resolve declared metadata once; known opaque units permit raw access only.
  /// @param[in] unit Original XML unit, possibly absent.
  /// @param[in] altUnit Original alternative unit, possibly absent.
  /// @param[in] coefficient Explicit XML number-to-alternative-unit multiplier, if present.
  /// @param[in] message Message name used for scoped aliases.
  /// @param[in] field Field name used for scoped aliases.
  /// @return Immutable conversion, or null when metadata cannot describe a coherent SI conversion.
  /// @throws bad_message_file A declared unit is unknown or incompatible with its alternative.
  std::shared_ptr<const UnitConversion> prepareUnitConversion(
      std::string_view unit, std::string_view altUnit, std::optional<double> coefficient,
      std::string_view message, std::string_view field);
}
