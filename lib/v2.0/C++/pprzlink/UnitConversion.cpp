// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file UnitConversion.cpp
 * @brief Strict table resolution, bounded SI prefixes and affine metadata composition.
 * @ingroup internals
 *
 * No algebraic unit-expression parser is used. Compound spellings must be listed
 * explicitly. Unknown declarations fail while XML schemas are loaded, before I/O.
 */
#include "detail/UnitConversion.h"
#include <pprzlink/exceptions/pprzlink_exception.h>
#include <array>
#include <cmath>
#include <format>

namespace pprzlink::detail {
  namespace {
    /// @brief Find the most specifically scoped exact spelling.
    /// @param[in] spelling Complete unit spelling to resolve.
    /// @param[in] message Message name used by legacy scoped overrides.
    /// @param[in] field Field name used by legacy scoped overrides.
    /// @return Matching static rule, or null if unknown.
    const UnitAlias *find(std::string_view spelling, std::string_view message, std::string_view field)
    {
      const UnitAlias *selected = nullptr;
      int specificity = -1;
      for (const auto &alias : unitAliases()) {
        if (alias.xml != spelling || (!alias.message.empty() && alias.message != message) ||
            (!alias.field.empty() && alias.field != field)) continue;
        const int rank = static_cast<int>(!alias.message.empty()) + static_cast<int>(!alias.field.empty());
        if (rank > specificity) { selected = &alias; specificity = rank; }
      }
      return selected;
    }

    /// @brief Resolve an exact rule or one usual SI prefix on a registered linear base.
    /// @param[in] spelling Complete unit spelling; empty means unspecified.
    /// @param[in] attribute XML attribute name used in diagnostics.
    /// @param[in] message Owning message name.
    /// @param[in] field Owning field name.
    /// @return A recognized rule, or nullopt for an unspecified declaration.
    /// @throws bad_message_file Unknown spelling or invalid conversion rule.
    std::optional<UnitAlias> resolve(std::string_view spelling, std::string_view attribute,
                                    std::string_view message, std::string_view field)
    {
      if (spelling.empty()) return std::nullopt;
      std::optional<UnitAlias> result;
      if (const auto *exact = find(spelling, message, field)) result = *exact;
      else {
        // Longest prefixes first; case and u/Greek micro spellings are significant.
        static constexpr std::pair<std::string_view, double> prefixes[]{
          {"da", 1e1}, {"G", 1e9}, {"M", 1e6}, {"k", 1e3},
          {"h", 1e2}, {"d", 1e-1}, {"c", 1e-2}, {"m", 1e-3},
          {"u", 1e-6}, {"µ", 1e-6}, {"μ", 1e-6}, {"n", 1e-9}, {"p", 1e-12},
        };
        for (const auto &[prefix, multiplier] : prefixes) {
          if (!spelling.starts_with(prefix)) continue;
          // Prefixes use global physical bases; a scoped C temperature is not mC.
          const auto *base = find(spelling.substr(prefix.size()), {}, {});
          if (!base || base->si.empty() || !base->prefixPower || base->offset != 0.0) continue;
          result = *base;
          for (unsigned int power = 0; power < base->prefixPower; ++power) result->multiplier *= multiplier;
          break;
        }
      }
      if (!result) throw bad_message_file(std::format(
        "message '{}' field '{}': unknown {} unit '{}'; add its rule to UnitAliases.cpp",
        message, field, attribute, spelling));
      if (!std::isfinite(result->multiplier) || result->multiplier == 0.0 || !std::isfinite(result->offset))
        throw bad_message_file(std::format("message '{}' field '{}': invalid affine rule for unit '{}'",
                                           message, field, spelling));
      return result;
    }
  }

  std::shared_ptr<const UnitConversion> prepareUnitConversion(
      std::string_view unit, std::string_view altUnit, std::optional<double> coefficient,
      std::string_view message, std::string_view field)
  {
    auto resolved = resolve(unit, "unit", message, field);
    const auto alternative = resolve(altUnit, "alt_unit", message, field);
    if (resolved && alternative && !resolved->si.empty() && !alternative->si.empty() &&
        resolved->si != alternative->si) throw bad_message_file(std::format(
          "message '{}' field '{}': incompatible unit '{}' and alt_unit '{}' ({} versus {})",
          message, field, unit, altUnit, resolved->si, alternative->si));
    if (resolved && resolved->si.empty()) return {};
    if (coefficient) {
      if (!alternative || alternative->si.empty()) return {};
      resolved = alternative;
      resolved->multiplier *= *coefficient; // The intercept is not scaled by XML number metadata.
    }
    if (!resolved || resolved->si.empty()) return {};
    if (!std::isfinite(resolved->multiplier) || resolved->multiplier == 0.0)
      throw bad_message_file(std::format("message '{}' field '{}': composed unit multiplier is invalid", message, field));
    return std::make_shared<const UnitConversion>(resolved->multiplier, resolved->offset, std::string(resolved->si));
  }
}
