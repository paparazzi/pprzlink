/**
 * @file XmlReader.h
 * @brief XML element, required-attribute and identifier validation.
 * @ingroup internals
 *
 * Legacy uppercase attribute names and lowercase names are accepted. Errors identify the element and source line where available.
 */

#pragma once

#include <pprzlink/exceptions/pprzlink_exception.h>
#include <tinyxml2.h>
#include <charconv>
#include <format>
#include <string_view>

namespace pprzlink::detail::xml {
  /// @brief Validate a required element's existence and exact lowercase tag name.
  /// @param[in] value Element pointer to inspect; ownership remains with its XML document.
  /// @param[in] expected Required tag spelling.
  /// @return Borrowed element reference.
  /// @throws bad_message_file The pointer is null or the tag differs.
  inline const tinyxml2::XMLElement &element(const tinyxml2::XMLElement *value,
                                            std::string_view expected)
  {
    if (!value || value->Name() != expected) {
      throw bad_message_file(std::format("Expected <{}> element", expected));
    }
    return *value;
  }

  /// @brief Read a required, nonempty attribute, accepting legacy uppercase names.
  /// @param[in] element Source element providing name/line diagnostic context.
  /// @param[in] lowercase Lowercase attribute spelling.
  /// @param[in] uppercase Legacy spelling, checked before the lowercase alternative.
  /// @return An owned attribute string.
  /// @throws bad_message_file Both spellings are absent or the selected value is empty.
  inline std::string attribute(const tinyxml2::XMLElement &element,
                               const char *lowercase, const char *uppercase)
  {
    const char *value = element.Attribute(uppercase);
    if (!value) value = element.Attribute(lowercase);
    if (!value || !*value) {
      throw bad_message_file(std::format("<{}> at line {}: missing '{}' attribute",
                                        element.Name(), element.GetLineNum(), lowercase));
    }
    return value;
  }

  /// @brief Parse a complete, nonnegative decimal XML identifier.
  /// @param[in] element Element containing id or ID.
  /// @param[in] maximum Inclusive maximum permitted by this identifier's wire width.
  /// @return Validated identifier in [0, maximum].
  /// @throws bad_message_file The attribute is missing, malformed or out of range.
  inline int id(const tinyxml2::XMLElement &element, int maximum)
  {
    const auto text = attribute(element, "id", "ID");
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < 0 || value > maximum) {
      throw bad_message_file(std::format("<{}> at line {}: id '{}' must be in [0, {}]",
                                        element.Name(), element.GetLineNum(), text, maximum));
    }
    return value;
  }
}
