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
 * @file MessageField.cpp
 * @brief Fixed schema data sizes for fields.
 * @ingroup messages
 *
 * Strings and dynamic arrays report zero fixed bytes; concrete field sizes are computed by the binary codec after values are supplied.
 */

#include <pprzlink/MessageField.h>
#include "detail/UnitConversion.h"
#include <utility>

namespace pprzlink {

  MessageField::MessageField(const std::string &name, const FieldType &type)
    : name(name), type(type), size(0)
  {
    if (type.getBaseType() == BaseType::STRING) {
      size = 0;
    } else if (type.isArray()) {
      size = sizeofBaseType(type.getBaseType()) * type.getArraySize();
    } else {
      size = sizeofBaseType(type.getBaseType());
    }
  }

  MessageField::MessageField(const std::string &name, const std::string &typeString, std::string format,
                             std::string unit, std::string altUnit, std::optional<double> altUnitCoef,
                             std::string messageName)
    : MessageField(name, FieldType(typeString))
  {
    this->format = std::move(format);
    this->unit = std::move(unit);
    this->altUnit = std::move(altUnit);
    this->altUnitCoef = altUnitCoef;
    this->messageName = std::move(messageName);
    if (altUnitCoef && (!std::isfinite(*altUnitCoef) || *altUnitCoef == 0.0)) {
      throw bad_message_file("alt_unit_coef must be finite and nonzero");
    }
    auto conversion = detail::prepareUnitConversion(this->unit, this->altUnit, altUnitCoef,
                                                    this->messageName, name);
    if (type.getBaseType() != BaseType::STRING) unitConversion = std::move(conversion);
  }

  const std::string &MessageField::getSIUnit() const noexcept
  {
    static const std::string unspecified;
    return unitConversion ? unitConversion->siSymbol : unspecified;
  }

  std::string MessageField::unitContext() const
  {
    return std::format("message '{}' field '{}' (XML unit '{}')", messageName, name, unit);
  }

  double MessageField::valueToSI(double value) const
  {
    if (!unitConversion) throw field_unit_error(unitContext() + ": SI conversion is unavailable");
    const auto &conversion = *unitConversion;
    const double converted = value * conversion.multiplier + conversion.offset;
    if (std::isfinite(value) && !std::isfinite(converted)) {
      throw field_conversion_error(unitContext() + ": value cannot be represented as an SI double");
    }
    return converted;
  }

  double MessageField::valueFromSI(double value) const
  {
    if (!unitConversion) throw field_unit_error(unitContext() + ": SI conversion is unavailable");
    const auto &conversion = *unitConversion;
    const double converted = (value - conversion.offset) / conversion.multiplier;
    if (std::isfinite(value) && !std::isfinite(converted)) {
      throw field_conversion_error(unitContext() + ": SI value exceeds the XML numeric range");
    }
    return converted;
  }

  const std::string &MessageField::getName() const
  {
    return name;
  }

  const FieldType &MessageField::getType() const
  {
    return type;
  }

  size_t MessageField::getSize() const
  {
    return size;
  }
}
