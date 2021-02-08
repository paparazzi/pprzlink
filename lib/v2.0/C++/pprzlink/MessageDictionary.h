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

/** \file MessageDictionnary.h
 *
 *
 */

#ifndef PPRZLINKCPP_MESSAGEDICTIONARY_H
#define PPRZLINKCPP_MESSAGEDICTIONARY_H

#include <map>
#include <boost/bimap.hpp>
#include <pprzlink/MessageDefinition.h>

namespace pprzlink {
  class MessageDictionary {
  public:
    explicit MessageDictionary(std::string const &fileName);

    [[nodiscard]] const MessageDefinition &getDefinition(std::string const &name) const;

    [[nodiscard]] const MessageDefinition &getDefinition(int classId, int msgId) const;

    [[nodiscard]] std::pair<int,int> getMessageId(std::string name) const;
    [[nodiscard]] std::string getMessageName(int classId, int msgId) const;

    [[nodiscard]] int getClassId(std::string name) const;
    [[nodiscard]] std::string getClassName(int id) const;

    [[nodiscard]] std::vector<MessageDefinition> getMsgsForClass(std::string className) const;
    [[nodiscard]] std::vector<MessageDefinition> getMsgsForClass(int classId) const;

  private:
    std::map<std::string, MessageDefinition> messagesDict;
    boost::bimap<std::string, std::pair<int, int>> msgNameToId;
    boost::bimap<int,std::string> classMap;
  };
}
#endif //PPRZLINKCPP_MESSAGEDICTIONARY_H
