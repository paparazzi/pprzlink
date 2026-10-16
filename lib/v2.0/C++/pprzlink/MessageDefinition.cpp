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
 * @file MessageDefinition.cpp
 * @brief Validated XML message schemas and field lookup.
 * @ingroup messages
 *
 * Parsing checks identifiers, duplicate field names and routing modes. Definitions preserve XML field order for binary encoding.
 */

#include <pprzlink/MessageDefinition.h>
#include <pprzlink/exceptions/pprzlink_exception.h>
#include "detail/XmlReader.h"
#include <format>
#include <charconv>
#include <cmath>

namespace pprzlink {
  MessageDefinition::MessageDefinition() : classId(0), id(0) {}

  MessageDefinition::MessageDefinition(const tinyxml2::XMLElement *xml, int classId)
    : classId(0), id(0)
  {
    if (classId < 0 || classId > 15) throw bad_message_file("Class ID must be in [0, 15]");
    this->classId = static_cast<uint8_t>(classId);
    const auto &message = detail::xml::element(xml, "message");
    name = detail::xml::attribute(message, "name", "NAME");
    try {
      id = static_cast<uint8_t>(detail::xml::id(message, 255));
      const char *link = message.Attribute("link");
      if (!link) link = message.Attribute("LINK");
      if (link) {
        if (std::string_view(link) == "forwarded") linkMode = LinkMode::Forwarded;
        else if (std::string_view(link) == "broadcasted") linkMode = LinkMode::Broadcasted;
        else throw bad_message_file("Unknown link mode: " + std::string(link));
      }
      for (auto field = message.FirstChildElement("field"); field;
           field = field->NextSiblingElement("field")) {
        const auto fieldName = detail::xml::attribute(*field, "name", "NAME");
        try {
          const auto type = detail::xml::attribute(*field, "type", "TYPE");
          if (!fieldNameToIndex.emplace(fieldName, fields.size()).second) {
            throw bad_message_file("Duplicate field name");
          }
          const auto optionalAttribute = [&](const char *lowercase, const char *uppercase) {
            // Keep format's original lowercase-first precedence when both forms exist.
            const char *value = field->Attribute(lowercase);
            if (!value) value = field->Attribute(uppercase);
            return value;
          };
          const auto textAttribute = [&](const char *lowercase, const char *uppercase) {
            const auto value = optionalAttribute(lowercase, uppercase);
            return std::string(value ? value : "");
          };
          std::optional<double> coefficient;
          if (const auto attribute = optionalAttribute("alt_unit_coef", "ALT_UNIT_COEF")) {
            const std::string_view text(attribute);
            double value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() ||
                !std::isfinite(value) || value == 0.0) {
              throw bad_message_file("Invalid alt_unit_coef: " + std::string(text));
            }
            coefficient = value;
          }
          fields.emplace_back(fieldName, type, textAttribute("format", "FORMAT"),
                              textAttribute("unit", "UNIT"), textAttribute("alt_unit", "ALT_UNIT"),
                              coefficient, name);
        } catch (const bad_message_file &error) {
          throw bad_message_file(std::format("field '{}': {}", fieldName, error.what()));
        }
      }
    } catch (const bad_message_file &error) {
      throw bad_message_file(std::format("message '{}': {}", name, error.what()));
    }
  }

  uint8_t MessageDefinition::getClassId() const
  {
    return classId;
  }

  uint8_t MessageDefinition::getId() const
  {
    return id;
  }

  const std::string &MessageDefinition::getName() const
  {
    return name;
  }

  const MessageField &MessageDefinition::getField(size_t index) const
  {
    if (index >= fields.size()) {
      throw no_such_field(std::format("No field at index {} in message {}", index, name));
    }
    return fields[index];
  }

  const MessageField &MessageDefinition::getField(const std::string &name) const
  {
    const auto found = fieldNameToIndex.find(name);
    if (found == fieldNameToIndex.end()) {
      throw no_such_field(std::format("No field {} in message {}", name, getName()));
    }
    return fields[found->second];
  }

  size_t MessageDefinition::getNbFields() const
  {
    return fields.size();
  }

  bool MessageDefinition::hasFieldName(const std::string &name) const
  {
    return fieldNameToIndex.contains(name);
  }

  std::string MessageDefinition::toString() const
  {
    auto text = std::format("{}({}) in class {}\n", name, id, classId);
    for (const auto &field : fields) {
      text += std::format("\t{} : {}\n", field.getName(), field.getType().toString());
    }
    return text;
  }

  size_t MessageDefinition::getMinimumSize() const
  {
    size_t size = 0;
    for (const auto &field : fields) {
      size += field.getSize();
    }
    return size;
  }

  bool MessageDefinition::isRequest() const
  {
    return name.ends_with("_REQ");
  }
}
