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

/** \file MessageDictionnary.cpp
 *
 *
 */

#include <pprzlink/MessageDictionary.h>
#include <tinyxml2.h>
#include <iostream>
#include <pprzlink/exceptions/pprzlink_exception.h>

// TODO Implement the dictionnary as a singleton !

namespace pprzlink {

  MessageDictionary::MessageDictionary(std::string const &fileName)
  {
      tinyxml2::XMLDocument xml;
      xml.LoadFile(fileName.c_str());
      std::string rootElem(xml.RootElement()->Value());
      if(rootElem!="protocol")
      {
        throw bad_message_file("Root element is not protocol in xml messages file (found "+rootElem+").");
      }
      auto msg_class = xml.RootElement()->FirstChildElement("msg_class");
      while (msg_class!= nullptr)
      {
        auto className = msg_class->Attribute("name", nullptr);
        int classId = msg_class->IntAttribute("id", -1);
        if (className == nullptr || classId == -1)
        {
          throw bad_message_file(fileName + " msg_class as no name or id.");
        }
        classMap.left.insert(boost::bimap<int,std::string>::left_value_type(classId,className));
        auto message = msg_class->FirstChildElement("message");
        while (message!= nullptr)
        {
          auto messageName = message->Attribute("name", nullptr);
          int messageId = message->IntAttribute("id", -1);
          if (messageName == nullptr || messageId == -1)
          {
            throw bad_message_file(fileName + " in class : " + className + " message as no name or id.");
          }
          try
          {
            MessageDefinition def(message, classId);
            messagesDict[messageName]=def;
            msgNameToId.left.insert(boost::bimap<std::string, std::pair<int, int>>::left_value_type(messageName,std::make_pair(classId,messageId)));
          } catch (bad_message_file &e)
          {
            throw bad_message_file(fileName + " in class : " + className + " message " + messageName + " has a bad field.");
          }
          message = message->NextSiblingElement("message");
        }

        msg_class = msg_class->NextSiblingElement("msg_class");
      }
  }

  const MessageDefinition &MessageDictionary::getDefinition(std::string const & name) const
  {
    auto iter = messagesDict.find(name);
    if (iter == messagesDict.end())
    {
      std::stringstream sstr;
      sstr << "could not find message with name " << name << std::endl;
      throw no_such_message(sstr.str());
    }
    else
    {
      return iter->second;
    }
  }

  const MessageDefinition &MessageDictionary::getDefinition(int classId, int msgId) const
  {
    auto iter = msgNameToId.right.find(std::make_pair(classId, msgId));
    if (iter == msgNameToId.right.end())
    {
      std::stringstream sstr;
      sstr << "could not find message with id (" << classId << ":" << msgId << ")" << std::endl;
      throw no_such_message(sstr.str());
    }
    else
    {
      std::string name = iter->second;
      //std::cout << "message with id (" << classId << ":" << msgId << ") = " << name << std::endl;
      //std::cout << messagesDict.find(name)->second.toString() << std::endl;
      return messagesDict.find(name)->second;
    }
  }

  std::pair<int, int> MessageDictionary::getMessageId(std::string name) const
  {
    auto iter = msgNameToId.left.find(name);
    if (iter == msgNameToId.left.end())
    {
      throw no_such_message("No message with name " + name);
    }
    else
    {
      return iter->second;
    }
  }

  std::string MessageDictionary::getMessageName(int classId, int msgId) const
  {
    auto iter = msgNameToId.right.find(std::make_pair(classId, msgId));
    if (iter == msgNameToId.right.end())
    {
      std::stringstream sstr;
      sstr << "could not find message with id (" << classId << ":" << msgId << ")" << std::endl;
      throw no_such_message(sstr.str());
    }
    else
    {
      return iter->second;
    }
  }

  int MessageDictionary::getClassId(std::string name) const
  {
    auto iter = classMap.right.find(name);
    if (iter == classMap.right.end())
    {
      std::stringstream sstr;
      sstr << "could not find class named " << name << std::endl;
      throw no_such_class(sstr.str());
    }
    else
    {
      return iter->second;
    }
  }

  std::string MessageDictionary::getClassName(int id) const
  {
    auto iter = classMap.left.find(id);
    if (iter == classMap.left.end())
    {
      std::stringstream sstr;
      sstr << "could not find class with id " << id << std::endl;
      throw no_such_class(sstr.str());
    }
    else
    {
      return iter->second;
    }
  }

  std::vector<MessageDefinition> MessageDictionary::getMsgsForClass(std::string className) const
  {
    return getMsgsForClass(getClassId(className));
  }

  std::vector<MessageDefinition> MessageDictionary::getMsgsForClass(int classId) const
  {
    std::vector<MessageDefinition> result;
    for (auto msgPair : messagesDict)
    {
      auto &def = msgPair.second;
      if (def.getClassId()==classId)
        result.push_back(def);
    }
    return result;
  }
}

