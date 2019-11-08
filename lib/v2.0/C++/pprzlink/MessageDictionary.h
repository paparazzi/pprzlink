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

/** \file MessageDictionnary.h
 *
 *
 */

#ifndef PPRZLINKCPP_MESSAGEDICTIONARY_H
#define PPRZLINKCPP_MESSAGEDICTIONARY_H

#include <map>
#include <boost/bimap.hpp>
#include "MessageDefinition.h"

namespace pprzlink {
  class MessageDictionary {
  public:
    explicit MessageDictionary(std::string const &fileName);

    const MessageDefinition &getDefinition(std::string const &name) const;

    const MessageDefinition &getDefinition(int classId, int msgId) const;

    std::pair<int,int> getMessageId(std::string name) const;
    std::string getMessageName(int classId, int msgId) const;

    int getClassId(std::string name) const;
    std::string getClassName(int id) const;

  private:
    std::map<std::string, MessageDefinition> messagesDict;
    boost::bimap<std::string, std::pair<int, int>> msgNameToId;
    boost::bimap<int,std::string> classMap;
  };
}
#endif //PPRZLINKCPP_MESSAGEDICTIONARY_H
