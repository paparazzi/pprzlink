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
 * @file MessageDefinition.h
 * @brief Ordered message schemas and application routing metadata.
 * @ingroup messages
 *
 * A schema contains identifiers and field definitions rather than values. XML link attributes are exposed to applications without implementing routing policy.
 */

#ifndef PPRZLINKCPP_MESSAGEDEFINITION_H
#define PPRZLINKCPP_MESSAGEDEFINITION_H

#include <pprzlink/MessageField.h>
#include <tinyxml2.h>
#include <map>
#include <vector>

namespace pprzlink {

  /// A message schema: class/message IDs, name and ordered field definitions.
  /// It contains no transmitted values; Message populates a copy of this schema.
  /// @ingroup messages
  class MessageDefinition {
  public:
    /// @brief Application routing hint taken from the XML link attribute.
    enum class LinkMode {
      None, ///< No forwarding or broadcasting hint.
      Forwarded, ///< The application may forward this message to its specified aircraft.
      Broadcasted ///< The application may broadcast this message to its aircraft peers.
    };
    /// @brief Create an empty schema with zero identifiers and no fields.
    MessageDefinition();

    /// @brief Copy and validate a message element and its containing class identifier.
    /// @param[in] xml Non-null XML message element; its lifetime need not outlive the copy.
    /// @param[in] classId Message class in [0, 15].
    /// @throws bad_message_file Invalid elements, identifiers, field types, duplicate names or routing mode.
    explicit MessageDefinition(const tinyxml2::XMLElement *xml, int classId);

    /// @brief Read the message class identifier.
    /// @return Four-bit class identifier used in a binary header.
    [[nodiscard]] uint8_t getClassId() const;

    /// @brief Read the message identifier within its class.
    /// @return Message ID in [0, 255].
    [[nodiscard]] uint8_t getId() const;

    /// @brief Borrow the schema name.
    /// @return Name reference valid for this schema's lifetime.
    [[nodiscard]] const std::string &getName() const;

    /// @brief Count the ordered field definitions.
    /// @return Number of fields, whether or not a Message has populated values.
    [[nodiscard]] size_t getNbFields() const;

    /// Unknown names and out-of-range indices throw no_such_field.
    /// @param[in] index Zero-based position in XML field order.
    /// @return Borrowed field definition, valid for this schema's lifetime.
    /// @throws no_such_field The index is outside the schema.
    [[nodiscard]] const MessageField &getField(size_t index) const;

    /// @brief Look up a field by its XML name.
    /// @param[in] name Exact field name.
    /// @return Borrowed field definition, valid for this schema's lifetime.
    /// @throws no_such_field The name is not in the schema.
    [[nodiscard]] const MessageField &getField(const std::string &name) const;

    /// @brief Check whether a field is declared.
    /// @param[in] name Exact field name.
    /// @return True if the schema declares that name.
    [[nodiscard]] bool hasFieldName(const std::string &name) const;

    /// @brief Format identifiers and ordered field types for diagnostics.
    /// @return Human-readable schema description without populated values.
    [[nodiscard]] std::string toString() const;

    /// Sum of fixed field data sizes, excluding variable-length count prefixes.
    /// @return Fixed data bytes; this is not a complete payload or frame size.
    [[nodiscard]] size_t getMinimumSize() const;

    /// @brief Recognize the request naming convention.
    /// @return True when the message name ends with _REQ.
    [[nodiscard]] bool isRequest() const;

    /// @brief Inspect the application routing hint.
    /// @return Parsed link mode, defaulting to LinkMode::None.
    [[nodiscard]] LinkMode getLinkMode() const noexcept { return linkMode; }

  private:
    uint8_t classId;
    uint8_t id;
    std::string name;
    LinkMode linkMode = LinkMode::None;
    std::vector<MessageField> fields;
    std::map<std::string, size_t> fieldNameToIndex;
  };
}
#endif // PPRZLINKCPP_MESSAGEDEFINITION_H
