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
 * @file MessageDictionary.h
 * @brief Owned XML schemas indexed by names and numeric identifiers.
 * @ingroup messages
 *
 * References borrow dictionary storage. Both ordinary protocol XML and protocol elements wrapped by a flight-log configuration are supported.
 */

#ifndef PPRZLINKCPP_MESSAGEDICTIONARY_H
#define PPRZLINKCPP_MESSAGEDICTIONARY_H

#include <map>
#include <tinyxml2.h>
#include <boost/bimap.hpp>
#include <pprzlink/MessageDefinition.h>

namespace pprzlink {
  /// Load and own XML message definitions, indexed by name and class/message IDs.
  /// Definition references remain valid while this dictionary is alive and unchanged.
  /// @ingroup messages
  class MessageDictionary {
  public:
    /// @brief Load and validate XML message definitions from a file.
    /// @param[in] fileName Protocol XML or a flight-log configuration containing a protocol element.
    /// @throws messages_file_not_found The file cannot be found.
    /// @throws bad_message_file XML parsing or schema validation fails.
    MessageDictionary(const std::string &fileName);
    /// @brief Copy schemas from an existing XML tree.
    /// @param[in] root Non-null protocol element or configuration element wrapping a protocol.
    /// @throws bad_message_file The element or its schemas are invalid.
    MessageDictionary(tinyxml2::XMLElement *root);

    /// @brief Borrow a message schema by name.
    /// @param[in] name Exact message name, unique across all classes.
    /// @return Reference valid while the dictionary remains alive and unchanged.
    /// @throws no_such_message The message is unknown.
    [[nodiscard]] const MessageDefinition &getDefinition(const std::string &name) const;

    /// @brief Borrow a schema by class/message identifiers.
    /// @param[in] classId XML message-class identifier.
    /// @param[in] msgId XML message identifier within that class.
    /// @return Reference valid while the dictionary remains alive and unchanged.
    /// @throws no_such_message The identifier pair is unknown.
    [[nodiscard]] const MessageDefinition &getDefinition(int classId, int msgId) const;

    /// @brief Resolve a message name to its numeric identifiers.
    /// @param[in] name Exact message name.
    /// @return Pair of class ID and message ID.
    /// @throws no_such_message The name is unknown.
    [[nodiscard]] std::pair<int, int> getMessageId(std::string name) const;
    /// @brief Resolve a numeric identifier pair to a message name.
    /// @param[in] classId XML class ID.
    /// @param[in] msgId XML message ID.
    /// @return A copy of the message name.
    /// @throws no_such_message The identifier pair is unknown.
    [[nodiscard]] std::string getMessageName(int classId, int msgId) const;

    /// @brief Resolve a class name.
    /// @param[in] name Exact XML class name.
    /// @return Class identifier in [0, 15].
    /// @throws no_such_class The class name is unknown.
    [[nodiscard]] int getClassId(std::string name) const;
    /// @brief Resolve a class identifier.
    /// @param[in] id XML class ID.
    /// @return A copy of its name.
    /// @throws no_such_class The class ID is unknown.
    [[nodiscard]] std::string getClassName(int id) const;

    /// @brief Copy the message schemas belonging to a named class.
    /// @param[in] className Exact XML class name.
    /// @return Definitions ordered by message name.
    /// @throws no_such_class The class name is unknown.
    [[nodiscard]] std::vector<MessageDefinition> getMsgsForClass(std::string className) const;
    /// @brief Copy the message schemas matching a numeric class identifier.
    /// @param[in] classId Class to filter; an unknown ID produces an empty result.
    /// @return Definitions ordered by message name.
    [[nodiscard]] std::vector<MessageDefinition> getMsgsForClass(int classId) const;

  private:
    void loadXml(tinyxml2::XMLElement *root, const std::string &fileName);
    std::map<std::string, MessageDefinition> messagesDict;
    boost::bimap<std::string, std::pair<int, int>> msgNameToId;
    boost::bimap<int, std::string> classMap;
  };
}
#endif // PPRZLINKCPP_MESSAGEDICTIONARY_H
