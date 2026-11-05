// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file XbeeAddress.h
 * @brief Validated radio destinations with explicit network/IEEE address semantics.
 * @ingroup xbee
 */
#ifndef PPRZLINKCPP_XBEEADDRESS_H
#define PPRZLINKCPP_XBEEADDRESS_H

#include <concepts>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace pprzlink {
  /// A radio destination independent of the PPRZLINK message's receiver ID.
  /// Named factories accept integer literals and validate before narrowing.
  /// The kind is explicit even when an IEEE address has a small numeric value.
  /// @ingroup xbee
  class XbeeAddress {
  public:
    /// @brief Radio addressing format, independent of the numeric magnitude.
    enum class Kind {
      Network, ///< Short sixteen-bit network address (MY).
      Ieee ///< Sixty-four-bit IEEE radio address (SH/SL).
    };

    /// @brief Create a short network destination without truncating the input.
    /// @tparam Integer Signed or unsigned integer type, excluding bool.
    /// @param[in] address Network address in [0, 0xffff], including broadcast 0xffff.
    /// @return Validated network destination.
    /// @throws std::out_of_range The value is negative or exceeds sixteen bits.
    template<std::integral Integer>
      requires (!std::same_as<std::remove_cv_t<Integer>, bool>)
    [[nodiscard]] static constexpr XbeeAddress network(Integer address)
    {
      const auto value = unsignedValue(address);
      if (value > std::numeric_limits<std::uint16_t>::max())
        throw std::out_of_range("XBee network address must fit in 16 bits");
      return XbeeAddress(Kind::Network, value);
    }

    /// @brief Create an IEEE destination, including values below 0x10000.
    /// @tparam Integer Signed or unsigned integer type, excluding bool.
    /// @param[in] address Nonnegative value representable in sixty-four bits.
    /// @return Validated IEEE destination; the numeric value never selects the kind.
    /// @throws std::out_of_range The value is negative or exceeds sixty-four bits.
    template<std::integral Integer>
      requires (!std::same_as<std::remove_cv_t<Integer>, bool>)
    [[nodiscard]] static constexpr XbeeAddress ieee(Integer address)
    {
      return XbeeAddress(Kind::Ieee, unsignedValue(address));
    }

    /// @brief Inspect which radio addressing format was requested.
    /// @return Network or IEEE, as selected by the factory.
    [[nodiscard]] constexpr Kind kind() const noexcept { return addressKind; }
    /// @brief Inspect the validated numeric destination.
    /// @return Full address value without changing its format.
    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return addressValue; }
    /// @brief Compare both the address format and its numeric value.
    /// @param[in] other Radio address to compare.
    /// @return True only when both format and value match.
    constexpr bool operator==(const XbeeAddress &other) const = default;

  private:
    constexpr XbeeAddress(Kind kind, std::uint64_t value) noexcept
      : addressKind(kind), addressValue(value) {}

    template<std::integral Integer>
    static constexpr std::uint64_t unsignedValue(Integer address)
    {
      if constexpr (std::is_signed_v<Integer>) {
        if (address < 0) throw std::out_of_range("XBee address must not be negative");
      }
      if constexpr (std::numeric_limits<Integer>::digits > std::numeric_limits<std::uint64_t>::digits) {
        if (address > static_cast<Integer>(std::numeric_limits<std::uint64_t>::max()))
          throw std::out_of_range("XBee address must fit in 64 bits");
      }
      return static_cast<std::uint64_t>(address);
    }

    Kind addressKind; ///< Validated addressing format selected by the factory.
    std::uint64_t addressValue; ///< Nonnegative destination value fitting that format.
  };
}
#endif // PPRZLINKCPP_XBEEADDRESS_H
