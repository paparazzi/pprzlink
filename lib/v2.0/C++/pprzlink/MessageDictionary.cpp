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
 * @file MessageDictionary.cpp
 * @brief XML dictionary loading and bidirectional identifier lookup.
 * @ingroup messages
 *
 * Duplicate class or message identifiers are rejected. Validation errors retain file, class, message and field context where available.
 */

#include <pprzlink/MessageDictionary.h>
#include "detail/XmlReader.h"

namespace pprzlink {
  MessageDictionary::MessageDictionary(const std::string &fileName)
  {
    tinyxml2::XMLDocument xml;
    const auto status = xml.LoadFile(fileName.c_str());
    if (status == tinyxml2::XML_ERROR_FILE_NOT_FOUND) {
      throw messages_file_not_found("Cannot open message file " + fileName);
    }
    if (status != tinyxml2::XML_SUCCESS) {
      throw bad_message_file(std::format("{}: {}", fileName, xml.ErrorStr()));
    }
    loadXml(xml.RootElement(), fileName);
  }

  MessageDictionary::MessageDictionary(tinyxml2::XMLElement *root)
  {
    loadXml(root, "XML");
  }

  void MessageDictionary::loadXml(tinyxml2::XMLElement *root, const std::string &fileName)
  {
    try {
      // Flight logs wrap the protocol in a configuration element.
      if (root && std::string_view(root->Name()) == "configuration") {
        root = root->FirstChildElement("protocol");
      }
      const auto &protocol = detail::xml::element(root, "protocol");
      for (auto group = protocol.FirstChildElement("msg_class"); group;
           group = group->NextSiblingElement("msg_class")) {
        const auto className = detail::xml::attribute(*group, "name", "NAME");
        try {
          const auto classId = detail::xml::id(*group, 15);
          if (!classMap.insert({classId, className}).second) {
            throw bad_message_file("Duplicate class name or ID");
          }
          for (auto message = group->FirstChildElement("message"); message;
               message = message->NextSiblingElement("message")) {
            MessageDefinition definition(message, classId);
            const auto name = definition.getName();
            const auto id = std::make_pair(classId, definition.getId());
            if (!msgNameToId.insert({name, id}).second) {
              throw bad_message_file("Duplicate message name or ID: " + name);
            }
            messagesDict.emplace(name, std::move(definition));
          }
        } catch (const bad_message_file &error) {
          throw bad_message_file(std::format("class '{}': {}", className, error.what()));
        }
      }
    } catch (const bad_message_file &error) {
      throw bad_message_file(fileName + ": " + error.what());
    }
  }

  const MessageDefinition &MessageDictionary::getDefinition(const std::string &name) const
  {
    const auto found = messagesDict.find(name);
    if (found == messagesDict.end()) throw no_such_message("No message named " + name);
    return found->second;
  }

  const MessageDefinition &MessageDictionary::getDefinition(int classId, int msgId) const
  {
    return getDefinition(getMessageName(classId, msgId));
  }

  std::pair<int, int> MessageDictionary::getMessageId(std::string name) const
  {
    const auto found = msgNameToId.left.find(name);
    if (found == msgNameToId.left.end()) throw no_such_message("No message named " + name);
    return found->second;
  }

  std::string MessageDictionary::getMessageName(int classId, int msgId) const
  {
    const auto found = msgNameToId.right.find(std::make_pair(classId, msgId));
    if (found == msgNameToId.right.end()) {
      throw no_such_message(std::format("No message with ID ({}:{})", classId, msgId));
    }
    return found->second;
  }

  int MessageDictionary::getClassId(std::string name) const
  {
    const auto found = classMap.right.find(name);
    if (found == classMap.right.end()) throw no_such_class("No class named " + name);
    return found->second;
  }

  std::string MessageDictionary::getClassName(int id) const
  {
    const auto found = classMap.left.find(id);
    if (found == classMap.left.end()) throw no_such_class(std::format("No class with ID {}", id));
    return found->second;
  }

  std::vector<MessageDefinition> MessageDictionary::getMsgsForClass(std::string className) const
  {
    return getMsgsForClass(getClassId(className));
  }

  std::vector<MessageDefinition> MessageDictionary::getMsgsForClass(int classId) const
  {
    std::vector<MessageDefinition> result;
    for (const auto &[name, definition] : messagesDict) {
      if (definition.getClassId() == classId) result.push_back(definition);
    }
    return result;
  }
}
