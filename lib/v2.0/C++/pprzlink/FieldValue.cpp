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
 * @file FieldValue.cpp
 * @brief Text and array validation for populated fields.
 * @ingroup messages
 *
 * String values are distinct from character arrays. Binary compatibility helpers delegate to the field codec rather than duplicating wire-format rules.
 */

#include <pprzlink/FieldValue.h>
#include <pprzlink/BinaryCodec.h>

namespace pprzlink {
  void FieldValue::checkArraySize(const MessageField &field, size_t size)
  {
    const auto &type = field.getType();
    if (!type.isArray()) {
      throw std::logic_error("Cannot build scalar field " + field.getName() + " from an array");
    }
    if (type.getArraySize() != 0 && type.getArraySize() != size) {
      throw std::length_error(std::format("Field {} has {} elements, expected {}",
                                         field.getName(), size, type.getArraySize()));
    }
  }

  FieldValue::Storage FieldValue::makeText(const MessageField &field, const std::string &text)
  {
    const auto &type = field.getType();
    if (type.getBaseType() == BaseType::STRING && !type.isArray()) return text;
    if (type.getBaseType() == BaseType::CHAR && type.isArray()) {
      checkArraySize(field, text.size());
      return std::vector<char>(text.begin(), text.end());
    }
    throw std::logic_error(std::format("Cannot build field {} of type {} from text",
                                      field.getName(), type.toString()));
  }

  size_t FieldValue::addToBuffer(BytesBuffer &buffer) const
  {
    return binary::writeField(buffer, *this);
  }

  size_t FieldValue::getByteSize() const
  {
    return binary::fieldSize(*this);
  }
}
