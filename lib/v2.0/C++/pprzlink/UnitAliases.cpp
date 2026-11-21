// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file UnitAliases.cpp
 * @brief Editable XML-to-SI affine rules, optional scopes and prefix eligibility.
 * @ingroup internals
 *
 * Add {"XML spelling", "SI symbol", multiplier, offset}. The default slope is 1
 * and intercept is 0. SI = XML * multiplier + offset. Empty SI symbols recognize
 * opaque legacy labels but prohibit SI access. Scoped rules override global rules.
 */
#include "detail/UnitConversion.h"
#include <numbers>

namespace pprzlink::detail {
  namespace {
    /// @brief Mark a linear rule as eligible for the small SI-prefix resolver.
    /// @param[in] xml Exact base spelling, without a prefix.
    /// @param[in] si Canonical SI dimension/symbol.
    /// @param[in] multiplier Base XML-to-SI slope.
    /// @param[in] power Power of the prefixed quantity; 2/3 for area/volume.
    /// @return Immutable prefix-eligible rule.
    constexpr UnitAlias prefixed(std::string_view xml, std::string_view si,
                                  double multiplier = 1.0, unsigned int power = 1)
    { return {xml, si, multiplier, 0.0, {}, {}, power}; }
  }

  std::span<const UnitAlias> unitAliases() noexcept
  {
    constexpr double degrees = std::numbers::pi / 180.0;
    static constexpr UnitAlias aliases[] = {
      // Coherent SI bases and common derived units. Exact rules take precedence.
      {"1", "1"}, {"none", "1"}, {"%", "1", 0.01}, {"percent", "1", 0.01},
      prefixed("m", "m"), prefixed("s", "s"), {"kg", "kg"}, prefixed("g", "kg", 0.001),
      prefixed("A", "A"), prefixed("K", "K"), prefixed("mol", "mol"), prefixed("cd", "cd"),
      prefixed("rad", "rad"), prefixed("sr", "sr"), prefixed("Hz", "Hz"),
      prefixed("Pa", "Pa"), prefixed("V", "V"), prefixed("W", "W"), prefixed("J", "J"),
      prefixed("C", "C"), prefixed("T", "T"), prefixed("N", "N"), prefixed("S", "S"),
      prefixed("F", "F"), prefixed("H", "H"), prefixed("ohm", "ohm"), prefixed("Ohm", "ohm"),
      prefixed("Ω", "ohm"), prefixed("Bq", "Hz"), prefixed("Gy", "Gy"), prefixed("Sv", "Sv"),
      prefixed("m^2", "m^2", 1.0, 2), prefixed("m2", "m^2", 1.0, 2), prefixed("m²", "m^2", 1.0, 2),
      prefixed("m^3", "m^3", 1.0, 3), prefixed("m3", "m^3", 1.0, 3), prefixed("m³", "m^3", 1.0, 3),
      prefixed("m/s", "m/s"), prefixed("m/s^2", "m/s^2"), prefixed("m/s2", "m/s^2"),
      prefixed("m/s²", "m/s^2"), prefixed("m/s-2", "m/s^2"), prefixed("m/s^3", "m/s^3"),
      prefixed("rad/s", "rad/s"), prefixed("rad/sec", "rad/s"),
      prefixed("rad/s^2", "rad/s^2"), prefixed("rad/s2", "rad/s^2"), prefixed("rad/s²", "rad/s^2"),
      {"kg/m^3", "kg/m^3"}, {"kg/m3", "kg/m^3"},
      prefixed("g/m^3", "kg/m^3", 0.001), prefixed("g/m3", "kg/m^3", 0.001),
      prefixed("deg", "rad", degrees), prefixed("deg/s", "rad/s", degrees),
      prefixed("deg/s^2", "rad/s^2", degrees), prefixed("deg/s2", "rad/s^2", degrees),
      prefixed("deg/s²", "rad/s^2", degrees),
      {"1/s", "Hz"}, {"hz", "Hz"}, {"pascal", "Pa"}, {"volts", "V"},
      {"sec", "s"}, {"min", "s", 60.0}, {"h", "s", 3600.0}, {"hour", "s", 3600.0},
      {"weeks", "s", 604800.0}, {"s (Unix time)", "s"},
      {"usec", "s", 1e-6}, {"mus", "s", 1e-6}, {"msec", "s", 0.001},
      {"decisec", "s", 0.1}, {"deciV", "V", 0.1},
      {"Ah", "C", 3600.0}, {"deciAh", "C", 360.0}, {"Wh", "J", 3600.0},
      prefixed("bar", "Pa", 100000.0), prefixed("Bar", "Pa", 100000.0),
      {"gauss", "T", 1e-4}, {"mGauss", "T", 1e-7},
      {"ft", "m", 0.3048}, {"km/h", "m/s", 1.0 / 3.6},
      {"rpm", "rad/s", 2.0 * std::numbers::pi / 60.0},
      {"decideg", "rad", degrees * 0.1}, {"centideg", "rad", degrees * 0.01}, {"deg_wind", "rad", degrees},

      // Fixed-point encodings are explicit; they are not inferred from a spelling.
      {"1e7deg", "rad", degrees * 1e-7}, {"1e2_deg", "rad", degrees * 0.01},
      {"1e4rad", "rad", 1e-4}, {"2^8m", "m", 1.0 / 256.0}, {"2^12rad", "rad", 1.0 / 4096.0},

      // Absolute temperatures: the intercept is applied after the number scale.
      {"degC", "K", 1.0, 273.15}, {"Celcius", "K", 1.0, 273.15},
      {"deg_celsius", "K", 1.0, 273.15}, {"deg_celcius", "K", 1.0, 273.15},
      {"deg celcius", "K", 1.0, 273.15}, {"deg C", "K", 1.0, 273.15}, {"°C", "K", 1.0, 273.15},
      {"dC", "K", 0.1, 273.15}, {"10x_deg_celsius", "K", 0.1, 273.15},
      {"100x_deg_celsius", "K", 0.01, 273.15},
      {"degF", "K", 5.0 / 9.0, 273.15 - 32.0 * (5.0 / 9.0)},
      {"°F", "K", 5.0 / 9.0, 273.15 - 32.0 * (5.0 / 9.0)},
      {"C", "K", 1.0, 273.15, "ESC", "temperature"},
      {"C", "K", 1.0, 273.15, "ESC", "temperature_dev"},
      {"C", "K", 1.0, 273.15, "BATTERY_MONITOR", "bus_temp"},
      {"C", "K", 1.0, 273.15, "ENGINE_STATUS", "temp"},
      {"C", "K", 1.0, 273.15, "IMCU_REMOTE_BARO", "pitot_temp"},
      {"P", "Pa", 1.0, 0.0, "BARO_MS5534A", "pressure"},

      {"events", "1"}, {"Fragments", "1"}, {"packets/s", "Hz"}, {"msgs/s", "Hz"},
      // Known protocol/count/calibration labels: recognized, without an SI conversion.
      {"adc", ""}, {"pprz", ""}, {"ticks", ""}, {"dshot", ""}, {"subpixels", ""},
      {"rel_hum", ""}, {"Poles/s", ""}, {"bool", ""}, {"byte_mask", ""}, {"url", ""},
      {"-", ""}, {"SI (m or deg)", ""}, {"dB", ""}, {"dbHz", ""},
      {"bytes", ""}, {"Bytes", ""}, {"bytes/s", ""}, {"MB", ""},
      // Existing XML uses these explicitly as application-specific display labels.
      {"motor", ""}, {"foo", ""},
    };
    return aliases;
  }
}
