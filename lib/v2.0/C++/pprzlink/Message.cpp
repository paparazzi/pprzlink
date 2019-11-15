/*
 * Copyright 2019 garciafa
 * This file is part of PprzLinkCPP
 *
 * PprzLinkCPP is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PprzLinkCPP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ModemTester.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/** \file Message.cpp
 *
 *
 */

#include <iostream>
#include <pprzlink/Message.h>

namespace pprzlink {

  Message::Message(const MessageDefinition &def) : def(def)
  {
  }

  size_t Message::getNbFields() const
  {
    return fieldValues.size();
  }

  const MessageDefinition &Message::getDefinition() const
  {
    return def;
  }

  template<typename StreamType>
  void addFieldToStream(StreamType &stream,const MessageField &field,std::any value, bool uint8_as_int=false)
  {
    switch (field.getType().getBaseType())
    {
      case BaseType::CHAR :
        stream << std::any_cast<char>(value);
        break;
      case BaseType::INT8:
        if (uint8_as_int)
        {
          stream << (int)std::any_cast<int8_t>(value);
        }
        else
        {
          stream << std::any_cast<int8_t>(value);
        }
        break;
      case BaseType::INT16:
        stream << std::any_cast<int16_t>(value);
        break;
      case BaseType::INT32:
        stream << std::any_cast<int32_t>(value);
        break;
      case BaseType::UINT8:
        if (uint8_as_int)
        {
          stream << (int)std::any_cast<uint8_t>(value);
        }
        else
        {
          stream << std::any_cast<uint8_t>(value);
        }
        break;
      case BaseType::UINT16:
        stream << std::any_cast<uint16_t>(value);
        break;
      case BaseType::UINT32:
        stream << std::any_cast<uint32_t>(value);
        break;
      case BaseType::FLOAT:
        stream << std::any_cast<float>(value);
        break;
      case BaseType::STRING:
          stream << std::any_cast<std::string>(value);
          break;
      case BaseType::NOT_A_TYPE:
        // THIS SHOULD NOT HAPPEN !!!!
        throw std::logic_error(" field "+ field.getName() +"as type NOT_A_TYPE !");
        break;
    }
  }

  template<typename StreamType>
  void addVectorToStream(StreamType &stream,const Message &msg,int index, bool uint8_as_int=false)
  {
    const auto& value = msg.getRawValue(index);
    switch (value.getType().getBaseType())
    {
      case BaseType::CHAR :
      {
        std::vector<char> vec;

      }
        break;
      case BaseType::INT8:
        break;
      case BaseType::INT16:
        break;
      case BaseType::INT32:
        break;
      case BaseType::UINT8:
        break;
      case BaseType::UINT16:
        break;
      case BaseType::UINT32:
        break;
      case BaseType::FLOAT:
        break;
      case BaseType::STRING:
        break;
      case BaseType::NOT_A_TYPE:
        // THIS SHOULD NOT HAPPEN !!!!
        throw std::logic_error(" field " + value.getName() + "as type NOT_A_TYPE !");
        break;
    }
  }

  std::string Message::toString() const
  {
    std::stringstream sstr;

    sstr << def.getName() << " [";
    if (def.getNbFields())
    for (int i=0;i<def.getNbFields();++i)
    {
      if (i != 0)
      {
        sstr << "; ";
      }
      auto name = def.getField(i).getName();

      sstr << name << "=";
      auto found = fieldValues.find(name);
      if (found == fieldValues.end())
      {
        sstr << "NOTSET";
      }
      else
      {
        if (found->second.getType().isArray() && found->second.getType().getBaseType()!=BaseType::CHAR)
          sstr << "{";
        auto val =found->second;
        val.setOutputInt8AsInt(true);
          sstr << val;
        if (found->second.getType().isArray() && found->second.getType().getBaseType()!=BaseType::CHAR)
          sstr << "}";
      }
    }
    sstr << "]";

    return sstr.str();
  }

  const FieldValue &Message::getRawValue(const std::string &name) const
  {
    auto found = fieldValues.find(name);
    if (found==fieldValues.end())
    {
      throw pprzlink::no_such_field("No value for field "+name);
    }
    return found->second;
  }

  const FieldValue &Message::getRawValue(int index) const
  {
    auto name = def.getField(index).getName();
    auto found = fieldValues.find(name);
    if (found==fieldValues.end())
    {
      throw pprzlink::no_such_field("No value for field "+name);
    }
    return found->second;
  }
}
