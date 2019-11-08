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

/** \file Message.h
 *
 *
 */

#ifndef PPRZLINKCPP_MESSAGE_H
#define PPRZLINKCPP_MESSAGE_H

#include <map>
#include <cassert>
#include "MessageDefinition.h"
#include <pprzlink/exceptions/pprzlink_exception.h>
#include "FieldValue.h"

namespace pprzlink {
  class Message {
  public:
    explicit Message(const MessageDefinition &def);

    template<typename ValueType>
    void addField(const std::string &name, ValueType value)
    {
      auto field = def.getField(name); // Will throw no_such_field if non existing field name
      fieldValues[name] = FieldValue(field,value);
    }

    template<typename ValueType>
    void getField(const std::string &name, ValueType &value)
    {
      auto field = def.getField(name); // Will throw no_such_field if non existing field name
      if (fieldValues.find(name) == fieldValues.end())
      {
        throw field_has_no_value("In message " + def.getName() + " field " + name + " has not value !");
      }
      fieldValues[name].getValue(value);
    }

    template<typename ValueType>
    void getField(int index, ValueType &value)
    {
      auto name = def.getField(index).getName();
      if (fieldValues.find(name) == fieldValues.end())
      {
        throw field_has_no_value("In message " + def.getName() + " field " + name + " has not value !");
      }
      fieldValues[name].getValue(value);
    }

    const FieldValue& getRawValue(int index) const;
    const FieldValue& getRawValue(const std::string& name) const;

    [[nodiscard]] size_t getNbFields() const;

    [[nodiscard]] const MessageDefinition &getDefinition() const;

    [[nodiscard]] std::string toString() const;

  private:
    MessageDefinition def;
    std::map<std::string, FieldValue> fieldValues;
  };

}
#endif //PPRZLINKCPP_MESSAGE_H
