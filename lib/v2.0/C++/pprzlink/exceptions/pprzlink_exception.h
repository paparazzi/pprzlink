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
 * @file pprzlink_exception.h
 * @brief Exception categories for schemas, values, bindings and codecs.
 * @ingroup errors
 *
 * Field type mismatches preserve the std::bad_variant_access catch contract; numeric conversion errors derive from std::out_of_range.
 */

#ifndef PPRZLINKCPP_PPRZLINK_EXCEPTION_H
#define PPRZLINKCPP_PPRZLINK_EXCEPTION_H

#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

/// @brief Declare a message-bearing exception derived from pprzlink_exception.
/// @param a Name of the exception class to declare.
#define DECLARE_PPRZLINK_EXCEPT(a) class a : public pprzlink_exception {\
public:\
/** @brief Create this exception with owned diagnostic text. @param arg Error message. */\
explicit a(const std::string &arg) :\
  pprzlink_exception(arg) {}\
};

namespace pprzlink {
  /// Preserve the existing std::bad_variant_access catch contract with useful context.
  /// @ingroup errors
  class field_type_mismatch : public std::bad_variant_access {
  public:
    /// @brief Own a diagnostic explaining the requested and actual field types.
    /// @param[in] message Error text copied/moved into this exception.
    explicit field_type_mismatch(std::string message) : message(std::move(message)) {}
    /// @brief Borrow the diagnostic string.
    /// @return Zero-terminated text valid while this exception remains alive and unchanged.
    const char *what() const noexcept override { return message.c_str(); }
  private:
    std::string message;
  };

  /// @brief Checked numeric conversion rejected a range, fraction or non-finite integer input.
  /// @ingroup errors
  /// Preserves the std::out_of_range catch contract and its message constructors.
  class field_conversion_error : public std::out_of_range {
  public:
    using std::out_of_range::out_of_range;
  };

  /// @brief Base runtime error for XML, message, codec and binding failures.
  /// @ingroup errors
  class pprzlink_exception : public std::runtime_error {
  public:
    /// @brief Create a message-bearing library runtime error.
    /// @param[in] arg Diagnostic text owned by std::runtime_error.
    pprzlink_exception(const std::string &arg) : runtime_error(arg){}
  };

  /// @brief A wire message, payload or display format is malformed.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(wrong_message_format)
  /// @brief A message name or class/message identifier pair is unknown.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(no_such_message)
  /// @brief A field name or index is absent from its schema.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(no_such_field)
  /// @brief A declared field has not been populated with a value.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(field_has_no_value)
  /// Requested SI conversion has absent, unsupported or inconsistent unit metadata.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(field_unit_error)
  /// @brief A requested XML class name or identifier is unknown.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(no_such_class)
  /// @brief A requested messages XML file cannot be found.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(messages_file_not_found)
  /// @brief XML parsing or schema validation failed.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(bad_message_file)
  /// @brief Compatibility exception for an unknown binding identifier.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(no_such_binding)
  /// @brief An ordinary send was attempted with a request schema.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(message_is_request)
  /// @brief A request helper was called with a schema not ending in _REQ.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(message_is_not_request)
  /// @brief A request handler returned a different answer schema from the required one.
  /// @ingroup errors
  DECLARE_PPRZLINK_EXCEPT(wrong_answer_to_request)
}
#endif // PPRZLINKCPP_PPRZLINK_EXCEPTION_H
