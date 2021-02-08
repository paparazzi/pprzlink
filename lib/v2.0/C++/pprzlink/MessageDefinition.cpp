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

/** \file MessageDefinition.cpp
 *
 *
 */

#include <iostream>
#include <sstream>
#include <pprzlink/MessageDefinition.h>
#include <pprzlink/exceptions/pprzlink_exception.h>

namespace pprzlink {
  MessageDefinition::MessageDefinition()
    : classId(0), id(0)
  {
  }

  MessageDefinition::MessageDefinition(tinyxml2::XMLElement *xml, int classId)
    : classId(classId), id(-1)
  {
    name = xml->Attribute("name");
    id = xml->IntAttribute("id");
    auto field = xml->FirstChildElement("field");
    while (field != nullptr)
    {
      auto fieldName = field->Attribute("name", nullptr);
      auto fieldTypeStr = field->Attribute("type", nullptr);
      if (fieldName == nullptr || fieldTypeStr == nullptr)
      {
        throw bad_message_file("Bad field");
      }
      MessageField msgField(fieldName, fieldTypeStr);

      fieldNameToIndex[fieldName] = fields.size();
      fields.push_back(msgField);

      field = field->NextSiblingElement("field");
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

  const MessageField &MessageDefinition::getField(int index) const
  {
    return fields[index];
  }

  const MessageField &MessageDefinition::getField(const std::string &name) const
  {
    auto found =fieldNameToIndex.find(name);
    if (found==fieldNameToIndex.end())
      throw no_such_field("No field "+name+" in message "+getName());
    return fields[found->second];
  }

  size_t MessageDefinition::getNbFields() const
  {
    return fields.size();
  }

  bool MessageDefinition::hasFieldName(const std::string &name) const
  {
    return fieldNameToIndex.find(name)!=fieldNameToIndex.end();
  }

  std::string MessageDefinition::toString() const
  {

    std::stringstream sstr;
    sstr << getName() << "("<< (int)getId() << ") in class " << (int)getClassId() << std::endl;
    for (size_t i=0;i<getNbFields();++i)
    {
      sstr << "\t" << getField(i).getName() << " : " << getField(i).getType().toString() << std::endl;
    }

    return sstr.str();
  }

  size_t MessageDefinition::getMinimumSize() const
  {
    int size=0;
    for (auto field: fields)
    {
        size+=field.getSize();
    }
    return size;
  }

  bool MessageDefinition::isRequest() const
  {
      std::string req = "_REQ";
      if (name.length() >= req.length()) {
          return (0 == name.compare (name.length() - req.length(), req.length(), req));
      } else {
          return false;
      }
  }
}
