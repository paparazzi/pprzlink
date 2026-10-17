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
 * @file FieldValue.h
 * @brief XML-selected field storage and checked value conversions.
 * @ingroup messages
 *
 * The stored variant matches the schema. Exact reads and explicit numeric conversions have separate contracts, including array size checks.
 */

#ifndef PPRZLINKCPP_FIELDVALUE_H
#define PPRZLINKCPP_FIELDVALUE_H

#include <pprzlink/MessageField.h>
#include <pprzlink/BytesBuffer.h>
#include <pprzlink/detail/CheckedNumber.h>
#include <algorithm>
#include <array>
#include <concepts>
#include <format>
#include <ranges>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace pprzlink {
  /// @brief Integral or floating-point C++ inputs accepted by checked conversion.
  /// @ingroup messages
  /// @tparam T Candidate numeric type, including char and bool.
  template<class T>
  concept Arithmetic = std::integral<T> || std::floating_point<T>;

  /// Array input: an iterable container with a value type and a size.
  /// std::string has its own overload to distinguish text from array elements.
  /// @ingroup messages
  /// @tparam Container Sized, const-iterable input with a value_type.
  template<class Container>
  concept FieldValueContainer =
    !std::same_as<Container, std::string> &&
    std::ranges::input_range<const Container> &&
    requires(const Container &container) {
      typename Container::value_type;
      { container.size() } -> std::convertible_to<size_t>;
    };

  /// Array output only needs clear/push_back; std::array has a separate overload.
  /// @ingroup messages
  /// @tparam Container Output type accepting elements of its declared value_type.
  template<class Container>
  concept FieldValueOutputContainer = !std::same_as<Container, std::string> &&
    requires(Container &container, typename Container::value_type element) {
      container.clear();
      container.push_back(element);
    };

  namespace detail {
    /// @brief Output types constructible and readable through a field's exact-read API.
    /// @ingroup internals
    /// @tparam Field Field-like type kept dependent until its definition is complete.
    /// @tparam Output Default-constructible destination type.
    template<class Field, class Output>
    concept ReadableFieldValue = std::default_initializable<Output> &&
      requires(const Field &field, Output &output) { field.getValue(output); };

    /// Translate the XML base type into a C++ type in one place.
    /// @ingroup internals
    /// @tparam Visitor Callable with a templated nullary operator for every supported base type.
    /// @param[in] type XML scalar base type.
    /// @param[in] visitor Type-dispatch callable; invoked once with the selected C++ type.
    /// @return The callable's result, preserving its declared reference/value category.
    /// @throws std::logic_error The base type is invalid.
    template<class Visitor>
    decltype(auto) visitBaseType(BaseType type, Visitor &&visitor)
    {
      switch (type) {
        case BaseType::CHAR: return visitor.template operator()<char>();
        case BaseType::INT8: return visitor.template operator()<int8_t>();
        case BaseType::INT16: return visitor.template operator()<int16_t>();
        case BaseType::INT32: return visitor.template operator()<int32_t>();
        case BaseType::INT64: return visitor.template operator()<int64_t>();
        case BaseType::UINT8: return visitor.template operator()<uint8_t>();
        case BaseType::UINT16: return visitor.template operator()<uint16_t>();
        case BaseType::UINT32: return visitor.template operator()<uint32_t>();
        case BaseType::UINT64: return visitor.template operator()<uint64_t>();
        case BaseType::FLOAT: return visitor.template operator()<float>();
        case BaseType::DOUBLE: return visitor.template operator()<double>();
        case BaseType::STRING: return visitor.template operator()<std::string>();
        case BaseType::NOT_A_TYPE: break;
      }
      throw std::logic_error("Invalid field base type");
    }
  }

  /// A field definition and its actual value. The variant type follows the XML type.
  /// @ingroup messages
  /// Numeric writes check ranges and fractions before storing a value. Exact reads
  /// require the XML-selected type; getValueAs() performs explicit numeric conversion.
  /// String arrays can be held/formatted but have no PPRZLINK binary representation.
  class FieldValue {
  public:
    /// @brief Scalar and array storage alternatives selected by the XML definition.
    using Storage = std::variant<
      char, int8_t, int16_t, int32_t, uint8_t, uint16_t, uint32_t, float, double, std::string,
      int64_t, uint64_t,
      std::vector<char>, std::vector<int8_t>, std::vector<int16_t>, std::vector<int32_t>,
      std::vector<uint8_t>, std::vector<uint16_t>, std::vector<uint32_t>,
      std::vector<int64_t>, std::vector<uint64_t>,
      std::vector<float>, std::vector<double>, std::vector<std::string>>;

    /// @brief Default construction is prohibited: a field needs a definition and a value.
    FieldValue() = delete;

    /// Convert an arithmetic scalar to the type declared by the field.
    /// @tparam T Arithmetic input type.
    /// @param[in] field Definition copied into this value.
    /// @param[in] input Numeric scalar to convert to the XML-selected type.
    /// @throws field_conversion_error A numeric conversion is fractional, non-finite for an integer, or out of range.
    /// @throws std::logic_error A scalar is supplied to an array field.
    template<Arithmetic T>
    FieldValue(const MessageField &field, T input) : field(field), value(makeScalar(field, input)) {}

    /// Build a string or a char array; text is never parsed as a number.
    /// @tparam T std::string input type.
    /// @param[in] field String or character-array definition copied into this value.
    /// @param[in] text Text bytes copied without encoding conversion.
    /// @throws std::logic_error The schema is neither a scalar string nor a character array.
    /// @throws std::length_error A fixed character array has a different extent.
    template<std::same_as<std::string> T>
    FieldValue(const MessageField &field, const T &text) : field(field), value(makeText(field, text)) {}

    /// @brief Copy zero-terminated text using the string constructor's rules.
    /// @tparam T char input element type.
    /// @param[in] field String or character-array definition to copy.
    /// @param[in] text Non-null, valid zero-terminated string.
    /// @throws std::exception Schema or extent validation fails as for the string constructor.
    template<std::same_as<char> T>
    FieldValue(const MessageField &field, const T *text) : FieldValue(field, std::string(text)) {}

    /// Convert array elements and check the fixed size, when specified in XML.
    /// @tparam Container Sized input container with a value_type.
    /// @param[in] field Array definition copied into this value.
    /// @param[in] input Elements converted individually to the XML base type.
    /// @throws std::logic_error The schema is scalar or an element cannot be converted.
    /// @throws std::length_error A fixed array has a different extent.
    /// @throws field_conversion_error A numeric element fails checked conversion.
    template<FieldValueContainer Container>
    FieldValue(const MessageField &field, const Container &input)
      : field(field), value(makeArray(field, input)) {}

    /// @brief Copy an arithmetic pointer/count range as an array field.
    /// @tparam T Arithmetic source element type.
    /// @param[in] field Array definition to copy.
    /// @param[in] input Pointer to a valid range of size readable elements.
    /// @param[in] size Number of input elements, not bytes.
    /// @throws std::exception Array schema, extent or element conversion validation fails.
    template<Arithmetic T>
    FieldValue(const MessageField &field, const T *input, size_t size)
      : FieldValue(field, std::span<const T>(input, size)) {}

    /// Read the exact stored type. A mismatch throws std::bad_variant_access.
    /// @tparam T Exact XML-selected arithmetic storage type.
    /// @param[out] output Destination assigned only after the type check succeeds.
    /// @throws field_type_mismatch The stored alternative differs from T.
    template<Arithmetic T>
    void getValue(T &output) const { output = stored<T>(); }

    /// @brief Copy an exactly stored scalar string.
    /// @tparam T std::string destination type.
    /// @param[out] output Destination receiving the stored text.
    /// @throws field_type_mismatch The field is not a scalar string.
    template<std::same_as<std::string> T>
    void getValue(T &output) const { output = stored<T>(); }

    /// Borrow the complete bytes of a scalar string or character array without copying.
    /// The view remains valid until this FieldValue is replaced, assigned or destroyed.
    /// @tparam T std::string_view destination type.
    /// @param[out] output View assigned only after the stored type is checked.
    /// @throws field_type_mismatch The field is neither a scalar string nor a character array.
    template<std::same_as<std::string_view> T>
    void getValue(T &output) const
    {
      if (const auto *text = std::get_if<std::string>(&value)) {
        output = *text;
      } else if (const auto *characters = std::get_if<std::vector<char>>(&value)) {
        output = characters->empty() ? std::string_view{} :
                                      std::string_view(characters->data(), characters->size());
      } else {
        throw field_type_mismatch(std::format("Field '{}' has type {}, requested string_view",
                                              getName(), getType().toString()));
      }
    }

    /// @brief Copy array elements into a clear/push_back output container.
    /// @tparam Container Destination whose value_type exactly matches the stored elements.
    /// @param[out] output Container cleared after the type check, then populated in order.
    /// @throws field_type_mismatch The exact element type differs or the field is scalar.
    template<FieldValueOutputContainer Container>
    void getValue(Container &output) const
    {
      const auto &elements = stored<std::vector<typename Container::value_type>>();
      output.clear();
      for (const auto &element : elements) output.push_back(element);
    }

    /// A fixed output array must have exactly the stored number of elements.
    /// @tparam T Exact stored element type.
    /// @tparam Size Required destination extent.
    /// @param[out] output Array populated after both type and length checks succeed.
    /// @throws field_type_mismatch The exact element type differs or the field is scalar.
    /// @throws std::length_error The stored extent differs from Size.
    template<class T, size_t Size>
    void getValue(std::array<T, Size> &output) const
    {
      const auto &elements = stored<std::vector<T>>();
      if (elements.size() != Size) {
        throw std::length_error(std::format("Field {} has {} elements, requested {}",
                                           getName(), elements.size(), Size));
      }
      std::ranges::copy(elements, output.begin());
    }

    /// Borrow an array through a read-only span without copying its elements.
    /// The view remains valid until this FieldValue is replaced, assigned or destroyed.
    /// @tparam T Exact stored array element type.
    /// @param[out] output View of the complete array, assigned only after the type check succeeds.
    /// @throws field_type_mismatch The field is scalar or its element type differs from T.
    template<class T>
      requires (!std::is_volatile_v<T> && !std::same_as<T, bool>)
    void getValue(std::span<const T> &output) const
    {
      const auto &elements = stored<std::vector<T>>();
      output = std::span<const T>(elements);
    }

    /// Return a value or borrowed view using the same rules as the output overloads.
    /// @tparam T Default-constructible output type supported by getValue(T&).
    /// @return An owned value, or a view valid until this value is replaced, assigned or destroyed.
    /// @throws std::exception The corresponding exact-read type or extent check fails.
    template<class T>
      requires detail::ReadableFieldValue<FieldValue, T>
    [[nodiscard]] T getValue() const
    {
      T output{};
      getValue(output);
      return output;
    }

    /// Explicit numeric conversion. Fractional-to-integer and out-of-range reads throw.
    /// Floating-point destinations follow normal rounding; this is not a lossless conversion API.
    /// @tparam T Requested numeric destination type.
    /// @return Checked numeric conversion without modifying the stored value.
    /// @throws field_type_mismatch The stored value is text or an array.
    /// @throws field_conversion_error Conversion is out of range or not an exact finite integer when required.
    template<Arithmetic T>
    [[nodiscard]] T getValueAs() const
    {
      return std::visit([this]<class Stored>(const Stored &item) -> T {
        if constexpr (Arithmetic<Stored>) return detail::checkedNumber<T>(item, getName());
        else throw field_type_mismatch(std::format("Field '{}' has type {}, requested numeric {}",
                                                  getName(), getType().toString(), detail::valueTypeName<T>()));
      }, value);
    }

    /// @brief Borrow the copied field definition.
    /// @return Definition reference valid for this FieldValue's lifetime.
    [[nodiscard]] const MessageField &getField() const { return field; }
    /// @brief Borrow the XML type selecting the stored variant alternative.
    /// @return Type reference valid for this FieldValue's lifetime.
    [[nodiscard]] const FieldType &getType() const { return field.getType(); }
    /// @brief Borrow the field's XML name.
    /// @return Name reference valid for this FieldValue's lifetime.
    [[nodiscard]] const std::string &getName() const { return field.getName(); }
    /// Read-only access for visitors; callers cannot change the stored type.
    /// @return Variant reference valid until this FieldValue is replaced or destroyed.
    [[nodiscard]] const Storage &getValue() const { return value; }

    /// Compatibility entry points delegated to the binary codec.
    /// @param[in,out] buffer Destination byte sequence to append to.
    /// @return Number of bytes appended, including any count prefix.
    /// @throws std::exception Binary type or count validation, or allocation, fails.
    size_t addToBuffer(BytesBuffer &buffer) const;
    /// @brief Inspect the size of this concrete binary field encoding.
    /// @return Bytes including any one-byte count prefix.
    /// @throws std::length_error A dynamic count exceeds 255.
    /// @throws std::logic_error Arrays of strings have no binary encoding.
    [[nodiscard]] size_t getByteSize() const;

  private:
    MessageField field;
    Storage value;

    static void checkArraySize(const MessageField &field, size_t size);
    static Storage makeText(const MessageField &field, const std::string &text);

    template<class T>
    const T &stored() const
    {
      // Also handles requested arithmetic/container types absent from Storage.
      return std::visit([this]<class Stored>(const Stored &item) -> const T& {
        if constexpr (std::same_as<T, Stored>) return item;
        else throw field_type_mismatch(std::format("Field '{}' has type {}, requested {}",
                                                  getName(), getType().toString(), detail::valueTypeName<T>()));
      }, value);
    }

    template<class To, class From>
    static To convertElement(const From &input, const std::string &fieldName)
    {
      if constexpr (std::same_as<To, std::string>) {
        std::ostringstream stream;
        stream << input;
        return stream.str();
      } else if constexpr (Arithmetic<From>) {
        return detail::checkedNumber<To>(input, fieldName);
      } else {
        throw std::logic_error("Cannot convert this array element to a numeric field");
      }
    }

    template<Arithmetic T>
    static Storage makeScalar(const MessageField &field, T input)
    {
      if (field.getType().isArray()) {
        throw std::logic_error("Cannot build array field " + field.getName() + " from a scalar");
      }
      return detail::visitBaseType(field.getType().getBaseType(), [&]<class Target>() -> Storage {
        return convertElement<Target>(input, field.getName());
      });
    }

    template<FieldValueContainer Container>
    static Storage makeArray(const MessageField &field, const Container &input)
    {
      checkArraySize(field, input.size());
      return detail::visitBaseType(field.getType().getBaseType(), [&]<class Target>() -> Storage {
        std::vector<Target> result;
        result.reserve(input.size());
        for (const auto &element : input) {
          // Read through value_type so proxy ranges such as vector<bool> are checked too.
          result.push_back(convertElement<Target>(static_cast<typename Container::value_type>(element), field.getName()));
        }
        return result;
      });
    }
  };
}

/// Human-readable output with numeric int8/uint8 values; does not mutate the field.
/// @ingroup codecs
/// @param[in,out] stream Destination with preserved formatting flags.
/// @param[in] value Populated field rendered in diagnostic syntax.
/// @return The destination stream after appending the field.
std::ostream& operator<<(std::ostream &stream, const pprzlink::FieldValue &value);

#endif // PPRZLINKCPP_FIELDVALUE_H
