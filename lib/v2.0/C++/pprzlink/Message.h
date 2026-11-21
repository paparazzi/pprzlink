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
 * @file Message.h
 * @brief Populated messages with schema copies and sender/receiver addressing.
 * @ingroup messages
 *
 * Fields remain unset until supplied. Single-field updates validate before replacement; grouped updates are ordered and not transactional as a whole.
 */

#ifndef PPRZLINKCPP_MESSAGE_H
#define PPRZLINKCPP_MESSAGE_H

#include <pprzlink/FieldValue.h>
#include <pprzlink/MessageDefinition.h>
#include <pprzlink/exceptions/pprzlink_exception.h>
#include <concepts>
#include <map>
#include <type_traits>
#include <utility>
#include <variant>
#include <span>
#include <vector>

namespace pprzlink {
  namespace detail {
    /// @brief Require complete name/value argument pairs before expanding a grouped setter.
    /// @ingroup internals
    /// @tparam Arguments Setter argument pack to count.
    template<class... Arguments>
    concept EvenNumberOfFieldArguments = (sizeof...(Arguments) % 2 == 0);

    /// @brief Recursively validate the name positions of a setter argument pack.
    /// @ingroup internals
    /// @tparam Arguments Alternating name/value types; the primary template rejects malformed packs.
    template<class... Arguments>
    struct FieldNameTypes : std::false_type {};

    /// @brief Accept an empty argument pack as the recursive base case.
    template<>
    struct FieldNameTypes<> : std::true_type {};

    /// @brief Require each name to be string-convertible, then check the remaining pairs.
    /// @tparam Name Current field-name argument type.
    /// @tparam Value Current value argument type, validated by FieldValue later.
    /// @tparam Remaining Remaining name/value pairs.
    template<class Name, class Value, class... Remaining>
    struct FieldNameTypes<Name, Value, Remaining...>
      : std::bool_constant<std::convertible_to<Name, std::string> &&
                           FieldNameTypes<Remaining...>::value> {};

    /// @brief Constraint selecting string-convertible field names in each pair.
    /// @ingroup internals
    /// @tparam Arguments Alternating name/value types to validate.
    template<class... Arguments>
    concept FieldNamesAreStrings = FieldNameTypes<Arguments...>::value;
  }

  /// A copy of a message definition, its populated FieldValues and sender/receiver IDs.
  /// Create from a dictionary definition, populate fields, then send through a link/transport.
  /// @ingroup messages
  class Message {
  public:
    /// @brief Text Ivy sender name or numeric binary sender identifier.
    using SenderId = std::variant<std::string, uint8_t>;

    /// @brief Create an empty schema/value set with all addressing IDs initially zero.
    /// Assign a schema-backed Message before using this placeholder for transmission.
    explicit Message();
    /// @brief Copy a schema without populating any field values.
    /// @param[in] def Definition copied into this message; its owner need not outlive the copy.
    explicit Message(const MessageDefinition &def);

    /// Validate and convert before inserting/replacing a field; no default value is created.
    /// @tparam ValueType Input type accepted by the XML-selected FieldValue constructor.
    /// @param[in] name Exact declared field name.
    /// @param[in] value Input converted with setField()'s validation rules.
    /// @throws std::exception Unknown field, schema/type mismatch, extent or numeric conversion failure.
    template<class ValueType>
    void addField(const std::string &name, ValueType value)
    {
      setField(name, std::move(value));
    }

    /// Set or replace a field and return this message for sending, without copying.
    /// The returned reference is valid while this message lives.
    /// Invalid input leaves the previous value unchanged.
    /// @tparam ValueType Input type accepted by the XML-selected FieldValue constructor.
    /// @param[in] name Exact declared field name.
    /// @param[in] value New scalar, string or array value.
    /// @return A const reference to this message, valid for its lifetime.
    /// @throws no_such_field The name is not declared by the schema.
    /// @throws std::exception FieldValue validation or conversion fails before replacement.
    template<class ValueType>
    const Message &setField(const std::string &name, ValueType value)
    {
      const auto &field = def.getField(name);
      fieldValues.insert_or_assign(name, FieldValue(field, value));
      return *this;
    }

    /// Apply multiple name/value pairs from left to right, using the same checks.
    /// Odd argument counts and non-string names are rejected at the call site.
    /// Return this message by const reference after all pairs succeed.
    /// If a pair throws, earlier pairs remain applied; the failing and subsequent
    /// pairs are unchanged.
    /// @tparam ValueType First value's input type.
    /// @tparam Remaining Types of the remaining alternating name/value arguments.
    /// @param[in] name First declared field name.
    /// @param[in] value First value converted according to its XML type.
    /// @param[in] remaining Additional name/value pairs in application order.
    /// @return A const reference to this message after all updates succeed.
    /// @throws std::exception A later update can fail after earlier pairs have already changed.
    template<class ValueType, class... Remaining>
      requires (sizeof...(Remaining) > 0) &&
               detail::EvenNumberOfFieldArguments<Remaining...> &&
               detail::FieldNamesAreStrings<Remaining...>
    const Message &setField(const std::string &name, ValueType value,
                            Remaining&&... remaining)
    {
      setField(name, std::move(value));
      return setField(std::forward<Remaining>(remaining)...);
    }

    /// Exact-type reads, or borrowed string_view/read-only span views of field storage.
    /// Views remain valid until field replacement or message destruction/assignment.
    /// Missing definitions/values retain their distinct exceptions.
    /// @tparam ValueType Scalar/string/array output type or borrowed view accepted by FieldValue.
    /// @param[in] name Exact field name.
    /// @param[out] value Destination populated by the exact-read overload.
    /// @throws no_such_field The field is not declared.
    /// @throws field_has_no_value The field is declared but unset.
    /// @throws std::exception The requested type or array extent does not match.
    template<class ValueType>
    void getField(const std::string &name, ValueType &value) const
    {
      fieldWithValue(name).getValue(value);
    }

    /// @brief Read an exact-type value by XML field position.
    /// @tparam ValueType Exact scalar/string/array output type, string_view or read-only span.
    /// @param[in] index Zero-based XML field position.
    /// @param[out] value Destination populated by the exact-read overload.
    /// @throws std::exception The position, value, type or array extent is invalid.
    template<class ValueType>
    void getField(size_t index, ValueType &value) const
    {
      getField(def.getField(index).getName(), value);
    }

    /// Return a value, or borrow field storage through a string_view or read-only span.
    /// Views remain valid until field replacement or message destruction/assignment.
    /// @tparam ValueType Output type supported by FieldValue::getValue().
    /// @param[in] name Exact field name.
    /// @return An owned scalar/string/container, or a borrowed string_view/read-only span.
    /// @throws std::exception Unknown/unset field or exact-read type/extent mismatch.
    template<class ValueType>
    [[nodiscard]] ValueType getField(const std::string &name) const
    {
      return fieldWithValue(name).getValue<ValueType>();
    }

    /// @brief Return a value or borrowed view by XML position.
    /// @tparam ValueType Exact stored output type, string_view or read-only span.
    /// @param[in] index Zero-based XML field position.
    /// @return An owned value, or a view valid until field replacement or message destruction/assignment.
    /// @throws std::exception Invalid position, unset value or exact-read mismatch.
    template<class ValueType>
    [[nodiscard]] ValueType getField(size_t index) const
    {
      return getField<ValueType>(def.getField(index).getName());
    }

    /// Request an explicit checked numeric conversion instead of an exact-type read.
    /// @tparam ValueType Requested arithmetic destination type.
    /// @param[in] name Exact field name.
    /// @return Checked numeric conversion; floating-point destinations may round.
    /// @throws std::exception Unknown/unset field, non-numeric storage or invalid numeric conversion.
    template<Arithmetic ValueType>
    [[nodiscard]] ValueType getFieldAs(const std::string &name) const
    {
      return fieldWithValue(name).getValueAs<ValueType>();
    }

    /// @brief Convert a numeric field selected by XML position.
    /// @tparam ValueType Requested arithmetic destination type.
    /// @param[in] index Zero-based XML field position.
    /// @return Checked numeric conversion without modifying the stored value.
    /// @throws std::exception Invalid position/value/type or numeric conversion failure.
    template<Arithmetic ValueType>
    [[nodiscard]] ValueType getFieldAs(size_t index) const
    {
      return getFieldAs<ValueType>(def.getField(index).getName());
    }

    /// Read the XML-selected variant without copying. Do not retain the reference
    /// beyond destruction/assignment of this message or replacement of this field.
    /// @param[in] name Exact field name.
    /// @return Borrowed, read-only variant for use with std::visit.
    /// @throws no_such_field The field is not declared.
    /// @throws field_has_no_value The field is unset.
    [[nodiscard]] const FieldValue::Storage &getField(const std::string &name) const
    {
      return fieldWithValue(name).getValue();
    }

    /// @brief Borrow the stored variant by XML field position.
    /// @param[in] index Zero-based XML field position.
    /// @return Read-only variant valid until field replacement or message destruction/assignment.
    /// @throws std::exception The position is invalid or its value is unset.
    [[nodiscard]] const FieldValue::Storage &getField(size_t index) const
    {
      return getField(def.getField(index).getName());
    }

    /// Borrow a field definition even if its value has not been set.
    /// @param[in] name Exact XML field name.
    /// @return Metadata valid until message destruction or assignment.
    /// @throws no_such_field The field is not declared.
    [[nodiscard]] const MessageField &getFieldDefinition(const std::string &name) const
    { return def.getField(name); }
    /// Borrow field metadata by XML position, independently of populated values.
    /// @param[in] index Zero-based XML field position.
    /// @return Metadata valid until message destruction or assignment.
    /// @throws no_such_field The position is outside the schema.
    [[nodiscard]] const MessageField &getFieldDefinition(size_t index) const
    { return def.getField(index); }

    /// Read an arithmetic scalar as a double in coherent SI units.
    /// Angles use radians, temperatures kelvins and percentages a dimensionless ratio.
    /// @param[in] name Exact XML field name.
    /// @return SI value of the actual stored number, including any prior quantization.
    /// @throws no_such_field The field is not declared.
    /// @throws field_has_no_value The field has not been set.
    /// @throws field_type_mismatch The field is an array or text.
    /// @throws field_unit_error SI conversion is unavailable.
    /// @throws field_conversion_error A finite value overflows the SI double.
    [[nodiscard]] double getFieldSI(const std::string &name) const;
    /// Read an SI arithmetic scalar by XML position.
    /// @param[in] index Zero-based XML field position.
    /// @return SI scalar value as double.
    /// @throws std::exception Same failures as the name-based SI getter.
    [[nodiscard]] double getFieldSI(size_t index) const
    { return getFieldSI(def.getField(index).getName()); }

    /// Return all elements of a homogeneous numeric array in coherent SI units.
    /// @param[in] name Exact XML field name.
    /// @return An owned vector of SI doubles with the stored array's length.
    /// @throws no_such_field The field is not declared.
    /// @throws field_has_no_value The field has not been set.
    /// @throws field_type_mismatch The field is scalar or contains text.
    /// @throws field_unit_error SI conversion is unavailable, even for an empty array.
    /// @throws field_conversion_error A finite element overflows the SI double.
    [[nodiscard]] std::vector<double> getFieldArraySI(const std::string &name) const;
    /// Return an SI array selected by XML position.
    /// @param[in] index Zero-based XML field position.
    /// @return An owned vector of SI doubles with the stored array's length.
    /// @throws std::exception Same failures as the name-based SI array getter.
    [[nodiscard]] std::vector<double> getFieldArraySI(size_t index) const
    { return getFieldArraySI(def.getField(index).getName()); }

    /// Assign an SI scalar after a complete successful conversion.
    /// @param[in] name Exact XML field name.
    /// @param[out] value SI double, unchanged if reading fails.
    /// @throws std::exception Same failures as the returning SI getter.
    void getFieldSI(const std::string &name, double &value) const { value = getFieldSI(name); }
    /// Assign an SI scalar selected by XML position.
    /// @param[in] index Zero-based XML field position.
    /// @param[out] value SI double, unchanged if reading fails.
    /// @throws std::exception Same failures as the returning SI getter.
    void getFieldSI(size_t index, double &value) const { value = getFieldSI(index); }
    /// Convert all elements of a homogeneous numeric array before replacing the output.
    /// @param[in] name Exact XML field name.
    /// @param[out] values SI doubles; unchanged on failure.
    /// @throws std::exception Missing field/value, wrong shape/type, unsupported units or overflow.
    void getFieldSI(const std::string &name, std::vector<double> &values) const;
    /// Read a homogeneous SI array selected by XML position.
    /// @param[in] index Zero-based XML field position.
    /// @param[out] values SI doubles; unchanged on failure.
    /// @throws std::exception Same failures as the name-based SI array getter.
    void getFieldSI(size_t index, std::vector<double> &values) const
    { getFieldSI(def.getField(index).getName(), values); }

    /// Convert an SI double into the XML unit/type before replacing a scalar.
    /// Integer storage rounds to nearest, with ties away from zero and no saturation.
    /// @param[in] name Exact XML field name.
    /// @param[in] value SI double (angles radians, absolute temperatures kelvins).
    /// @return This message by const reference, as with setField().
    /// @throws std::exception Missing field, wrong shape/type, unsupported units or numeric overflow.
    /// The previous field value is unchanged if conversion fails.
    const Message &setFieldSI(const std::string &name, double value);
    /// Convert homogeneous array elements and validate their extent before replacement.
    /// @param[in] name Exact XML field name.
    /// @param[in] values SI doubles, accepted from spans, vectors and std::arrays.
    /// @return This message by const reference, as with setField().
    /// @throws std::exception Wrong shape/extent, unsupported units or an element conversion failure.
    /// The previous field value is unchanged if any element conversion fails.
    const Message &setFieldSI(const std::string &name, std::span<const double> values);

    /// Binary codec wrappers; offset advances only after a complete field is stored.
    /// @param[in] index Zero-based XML field position.
    /// @param[in] buffer Little-endian binary field input.
    /// @param[in,out] offset Cursor committed only after decoding and storing the entire value.
    /// @throws std::exception Invalid position, truncated data or unsupported binary type.
    void addFieldFromBuffer(size_t index, const BytesBuffer &buffer, size_t &offset);
    /// @brief Decode a field from a borrowed binary input span.
    /// @param[in] index Zero-based XML field position.
    /// @param[in] buffer Input bytes copied into the new field value.
    /// @param[in,out] offset Cursor committed only after complete decoding and storage.
    /// @throws std::exception Invalid position, truncated data or unsupported binary type.
    void addFieldFromBuffer(size_t index, std::span<const uint8_t> buffer, size_t &offset);
    /// @brief Append one populated field's binary representation.
    /// @param[in] index Zero-based XML field position.
    /// @param[in,out] buffer Byte sequence to append to.
    /// @return Number of appended bytes, including any count prefix.
    /// @throws std::exception Invalid position, unset value or unsupported/oversized binary field.
    size_t addFieldToBuffer(size_t index, BytesBuffer &buffer) const;

    /// Same missing-field/value exceptions as getField; references belong to this message.
    /// @param[in] index Zero-based XML field position.
    /// @return Borrowed FieldValue valid until field replacement or message destruction/assignment.
    /// @throws std::exception Invalid position or unset field.
    [[nodiscard]] const FieldValue &getRawValue(size_t index) const;
    /// @brief Borrow a populated field by name.
    /// @param[in] name Exact field name.
    /// @return FieldValue reference valid until replacement or message destruction/assignment.
    /// @throws no_such_field The field is not declared.
    /// @throws field_has_no_value The field is unset.
    [[nodiscard]] const FieldValue &getRawValue(const std::string &name) const;
    /// @brief Count the fields that have actual values.
    /// @return Populated value count, not necessarily the schema's field count.
    [[nodiscard]] size_t getNbValues() const;
    /// @brief Borrow the message's owned schema copy.
    /// @return Definition reference valid until message destruction or assignment.
    [[nodiscard]] const MessageDefinition &getDefinition() const;
    /// @brief Describe field names and values, showing NOTSET for unpopulated fields.
    /// @return Diagnostic text; this is not the Ivy wire serializer.
    [[nodiscard]] std::string toString() const;

    /// @brief Borrow the numeric or textual sender identifier.
    /// @return Variant reference valid until sender replacement or message destruction/assignment.
    const SenderId &getSenderId() const;
    /// @brief Inspect the binary receiver identifier.
    /// @return Byte-sized receiver ID; 255 is used as broadcast by the transports.
    uint8_t getReceiverId() const;
    /// @brief Inspect the stored component identifier.
    /// @return Byte-sized value; binary codecs require it to fit in four bits.
    uint8_t getComponentId() const;
    /// @brief Inspect the class identifier from the owned schema.
    /// @return Four-bit XML class identifier.
    uint8_t getClassId() const;
    /// @brief Replace the sender with a numeric byte ID or text Ivy name.
    /// @param[in] senderId Value copied into the message; binary encoding requires text to be a decimal byte ID.
    void setSenderId(const SenderId &senderId);
    /// @brief Replace the binary receiver identifier.
    /// @param[in] receiverId Byte-sized destination ID, including broadcast value 255.
    void setReceiverId(uint8_t receiverId);
    /// @brief Store a byte-sized component identifier.
    /// @param[in] componentId Value stored here; binary encoding separately requires [0, 15].
    void setComponentId(uint8_t componentId);

    /// Numeric header IDs use the same checked conversion as numeric fields.
    /// String sender names remain available for Ivy through the SenderId overload.
    /// @tparam ValueType Arithmetic input type.
    /// @param[in] senderId Numeric ID checked against the byte range before replacing the sender.
    /// @throws field_conversion_error The ID is fractional, non-finite or outside [0, 255].
    template<Arithmetic ValueType>
    void setSenderId(ValueType senderId)
    {
      setSenderId(SenderId{detail::checkedNumber<uint8_t>(senderId, "sender_id")});
    }

    /// @brief Validate an arithmetic receiver ID before converting it to a byte.
    /// @tparam ValueType Arithmetic input type other than uint8_t.
    /// @param[in] receiverId Numeric destination, including broadcast value 255.
    /// @throws field_conversion_error The ID is fractional, non-finite or outside [0, 255].
    template<Arithmetic ValueType> requires (!std::same_as<ValueType, uint8_t>)
    void setReceiverId(ValueType receiverId)
    {
      setReceiverId(detail::checkedNumber<uint8_t>(receiverId, "receiver_id"));
    }

    /// @brief Validate an arithmetic component ID before converting it to a byte.
    /// @tparam ValueType Arithmetic input type other than uint8_t.
    /// @param[in] componentId Numeric byte value; binary encoding additionally restricts it to four bits.
    /// @throws field_conversion_error The ID is fractional, non-finite or outside [0, 255].
    template<Arithmetic ValueType> requires (!std::same_as<ValueType, uint8_t>)
    void setComponentId(ValueType componentId)
    {
      setComponentId(detail::checkedNumber<uint8_t>(componentId, "component_id"));
    }

    /// Binary payload size; all fields must have values.
    /// @return Encoded XML field bytes, excluding message headers and transport envelopes.
    /// @throws field_has_no_value The message is incomplete.
    /// @throws std::exception A field cannot be binary-encoded or exceeds its count limit.
    size_t getByteSize() const;

  private:
    const FieldValue &fieldWithValue(const std::string &name) const;

    MessageDefinition def;
    std::map<std::string, FieldValue> fieldValues;
    SenderId sender_id = uint8_t{0};
    uint8_t receiver_id = 0;
    uint8_t component_id = 0;
  };
}
#endif // PPRZLINKCPP_MESSAGE_H
