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
 * @file TextCodec.h
 * @brief Ivy field text and human-readable diagnostic formatting.
 * @ingroup codecs
 *
 * Formatting preserves the caller stream flags and the stored value; eight-bit integers are printed numerically.
 */

#ifndef PPRZLINKCPP_TEXTCODEC_H
#define PPRZLINKCPP_TEXTCODEC_H

#include <pprzlink/FieldValue.h>

namespace pprzlink {
  /// Ivy payload: comma-separated arrays, quoted char arrays and empty/spaced strings.
  /// Numbers use locale-independent to_chars formatting; finite floats use their
  /// shortest round-trippable representation, independently of the stream precision.
  /// @ingroup codecs
  /// @param[in,out] stream Destination whose flags, locale and precision are preserved.
  /// @param[in] value Read-only XML-selected variant to format.
  /// @return The destination stream after appending the field text.
  std::ostream &writeIvyField(std::ostream &stream, const FieldValue::Storage &value);

  /// Diagnostic output adds braces around numeric/string arrays.
  /// Both functions preserve the caller's stream flags and the stored value.
  /// @ingroup codecs
  /// @param[in,out] stream Destination whose flags, locale and precision are preserved.
  /// @param[in] value Read-only XML-selected variant to format.
  /// @return The destination stream after appending diagnostic text.
  std::ostream &writeDebugField(std::ostream &stream, const FieldValue::Storage &value);

  /// Convenience overloads for standalone fields; formatting needs only their value.
  /// @ingroup codecs
  /// @param[in,out] stream Destination with preserved formatting state.
  /// @param[in] field Populated field to render in Ivy syntax.
  /// @return The destination stream after appending the field.
  std::ostream &writeIvyField(std::ostream &stream, const FieldValue &field);
  /// @brief Append a populated field in diagnostic syntax.
  /// @ingroup codecs
  /// @param[in,out] stream Destination with preserved formatting state.
  /// @param[in] field Populated field to render.
  /// @return The destination stream after appending the field.
  std::ostream &writeDebugField(std::ostream &stream, const FieldValue &field);
}
#endif // PPRZLINKCPP_TEXTCODEC_H
