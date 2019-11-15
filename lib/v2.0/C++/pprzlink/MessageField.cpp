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

/** \file MessageField.cpp
 *
 *
 */

#include <pprzlink/MessageField.h>

namespace pprzlink {

  MessageField::MessageField(const std::string &name, const FieldType &type)
    : name(name), type(type)
  {
  }

  MessageField::MessageField(const std::string &name, const std::string &typeString)
    : name(name), type(typeString)
  {
  }

  const std::string &MessageField::getName() const
  {
    return name;
  }

  const FieldType &MessageField::getType() const
  {
    return type;
  }
}