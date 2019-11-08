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

/** \file MessageDefinition.h
 *
 *
 */

#ifndef PPRZLINKCPP_MESSAGEDEFINITION_H
#define PPRZLINKCPP_MESSAGEDEFINITION_H

#include <vector>
#include "MessageField.h"
#include <tinyxml2.h>
#include <map>

namespace pprzlink {

  class MessageDefinition {
  public:
    MessageDefinition ();

    explicit MessageDefinition (tinyxml2::XMLElement* xml,int classId);

    [[nodiscard]] int getClassId() const;

    [[nodiscard]] int getId() const;

    [[nodiscard]] const std::string &getName() const;

    [[nodiscard]] int getNbFields() const;

    [[nodiscard]] const MessageField& getField(int index) const;

    [[nodiscard]] const MessageField& getField(const std::string &name) const;

    [[nodiscard]] bool hasFieldName(const std::string &name) const;

    [[nodiscard]] std::string toString() const;
  private:
    int classId;
    int id;
    std::string name;
    std::vector<MessageField> fields;
    std::map<std::string,size_t> fieldNameToIndex;
  };
}
#endif //PPRZLINKCPP_MESSAGEDEFINITION_H
