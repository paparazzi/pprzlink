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
 * @file Message.cpp
 * @brief Field access, message diagnostics and binary payload sizing.
 * @ingroup messages
 *
 * Unknown fields and unset values remain distinct errors. Binary reads store a complete decoded field before committing the caller offset.
 */

#include <pprzlink/Message.h>
#include <pprzlink/BinaryCodec.h>
#include <pprzlink/TextCodec.h>

namespace pprzlink {
  Message::Message() = default;
  Message::Message(const MessageDefinition &definition) : def(definition) {}

  size_t Message::getNbValues() const { return fieldValues.size(); }
  const MessageDefinition &Message::getDefinition() const { return def; }

  const Message &Message::setFieldSI(const std::string &name, double value)
  {
    const auto &field = def.getField(name);
    if (field.getType().isArray()) throw field_type_mismatch(
      std::format("message '{}' field '{}': SI scalar supplied to XML array {}",
                  def.getName(), name, field.getType().toString()));
    auto converted = detail::visitBaseType(field.getType().getBaseType(), [&]<class Target>() -> FieldValue {
      if constexpr (Arithmetic<Target>) return FieldValue(field, field.fromSI<Target>(value));
      else throw field_type_mismatch(std::format("message '{}' field '{}': SI conversion requires a numeric field",
                                                 def.getName(), name));
    });
    fieldValues.insert_or_assign(name, std::move(converted));
    return *this;
  }

  const Message &Message::setFieldSI(const std::string &name, std::span<const double> values)
  {
    const auto &field = def.getField(name);
    if (!field.getType().isArray()) throw field_type_mismatch(
      std::format("message '{}' field '{}': SI array supplied to XML scalar {}",
                  def.getName(), name, field.getType().toString()));
    if (field.getType().getBaseType() == BaseType::STRING) throw field_type_mismatch(
      std::format("message '{}' field '{}': SI conversion requires numeric elements", def.getName(), name));
    // Check metadata for empty arrays too: no element may otherwise request a conversion.
    if (!field.canConvertSI()) throw field_unit_error(std::format(
      "message '{}' field '{}' (XML unit '{}'): SI conversion is unavailable", def.getName(), name, field.getUnit()));
    auto converted = detail::visitBaseType(field.getType().getBaseType(), [&]<class Target>() -> FieldValue {
      if constexpr (Arithmetic<Target>) {
        std::vector<Target> native;
        native.reserve(values.size());
        for (const double value : values) native.push_back(field.fromSI<Target>(value));
        return FieldValue(field, native);
      } else throw field_type_mismatch(std::format("message '{}' field '{}': SI conversion requires numeric elements",
                                                   def.getName(), name));
    });
    fieldValues.insert_or_assign(name, std::move(converted));
    return *this;
  }

  double Message::getFieldSI(const std::string &name) const
  {
    const auto &value = fieldWithValue(name);
    const auto &field = value.getField();
    if (field.getType().isArray()) throw field_type_mismatch(
      std::format("message '{}' field '{}': XML array requires getFieldArraySI()", def.getName(), name));
    return std::visit([&]<class Stored>(const Stored &raw) -> double {
      if constexpr (Arithmetic<Stored>) return field.toSI(raw);
      else throw field_type_mismatch(std::format("message '{}' field '{}': SI conversion requires a numeric field",
                                                 def.getName(), name));
    }, value.getValue());
  }

  std::vector<double> Message::getFieldArraySI(const std::string &name) const
  {
    const auto &value = fieldWithValue(name);
    const auto &field = value.getField();
    if (!field.getType().isArray()) throw field_type_mismatch(
      std::format("message '{}' field '{}': XML scalar requires getFieldSI()", def.getName(), name));
    if (field.getType().getBaseType() == BaseType::STRING) throw field_type_mismatch(
      std::format("message '{}' field '{}': SI conversion requires numeric elements", def.getName(), name));
    if (!field.canConvertSI()) throw field_unit_error(std::format(
      "message '{}' field '{}' (XML unit '{}'): SI conversion is unavailable", def.getName(), name, field.getUnit()));
    return std::visit([&]<class Stored>(const Stored &raw) -> std::vector<double> {
      if constexpr (requires { typename Stored::value_type; }) {
        if constexpr (Arithmetic<typename Stored::value_type>) {
          std::vector<double> result;
          result.reserve(raw.size());
          for (const auto &element : raw) result.push_back(field.toSI(element));
          return result;
        }
      }
      throw field_type_mismatch(std::format("message '{}' field '{}': SI conversion requires numeric elements",
                                            def.getName(), name));
    }, value.getValue());
  }

  void Message::getFieldSI(const std::string &name, std::vector<double> &values) const
  {
    values = getFieldArraySI(name);
  }

  std::string Message::toString() const
  {
    std::ostringstream stream;
    stream << def.getName() << " [";
    for (size_t i = 0; i < def.getNbFields(); ++i) {
      if (i != 0) stream << "; ";
      const auto &name = def.getField(i).getName();
      stream << name << '=';
      const auto found = fieldValues.find(name);
      if (found == fieldValues.end()) stream << "NOTSET";
      else writeDebugField(stream, found->second);
    }
    stream << ']';
    return stream.str();
  }

  const FieldValue &Message::fieldWithValue(const std::string &name) const
  {
    (void)def.getField(name); // Distinguish unknown fields from unset fields.
    const auto found = fieldValues.find(name);
    if (found == fieldValues.end()) {
      throw field_has_no_value(std::format("In message {} field {} has no value", def.getName(), name));
    }
    return found->second;
  }

  const FieldValue &Message::getRawValue(const std::string &name) const
  {
    return fieldWithValue(name);
  }

  const FieldValue &Message::getRawValue(size_t index) const
  {
    return getRawValue(def.getField(index).getName());
  }

  const Message::SenderId &Message::getSenderId() const { return sender_id; }
  uint8_t Message::getReceiverId() const { return receiver_id; }
  uint8_t Message::getComponentId() const { return component_id; }
  uint8_t Message::getClassId() const { return def.getClassId(); }
  void Message::setSenderId(const SenderId &senderId) { sender_id = senderId; }
  void Message::setReceiverId(uint8_t receiverId) { receiver_id = receiverId; }
  void Message::setComponentId(uint8_t componentId) { component_id = componentId; }

  size_t Message::addFieldToBuffer(size_t index, BytesBuffer &buffer) const
  {
    return binary::writeField(buffer, fieldWithValue(def.getField(index).getName()));
  }

  void Message::addFieldFromBuffer(size_t index, const BytesBuffer &buffer, size_t &offset)
  {
    addFieldFromBuffer(index, std::span<const uint8_t>(buffer), offset);
  }

  void Message::addFieldFromBuffer(size_t index, std::span<const uint8_t> buffer, size_t &offset)
  {
    const auto &field = def.getField(index);
    auto cursor = offset;
    auto value = binary::readField(field, buffer, cursor);
    fieldValues.insert_or_assign(field.getName(), std::move(value));
    offset = cursor;
  }

  size_t Message::getByteSize() const
  {
    if (fieldValues.size() != def.getNbFields()) {
      throw field_has_no_value("Cannot get size of an incomplete message");
    }
    size_t size = 0;
    for (const auto &[name, value] : fieldValues) size += binary::fieldSize(value);
    return size;
  }
}
