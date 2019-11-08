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

/** \file FieldValue.h
 *
 *
 */


#ifndef PPRZLINKCPP_FIELDVALUE_H
#define PPRZLINKCPP_FIELDVALUE_H

#include <any>
#include <vector>
#include <array>
#include <stdexcept>
#include "MessageField.h"
#include <sstream>
#include <cstdint>

namespace pprzlink {
  template<typename ValueType>
  struct FieldValueBuilder {
    static std::any MakeStdAny(const MessageField &field, const ValueType &value)
    {
      auto &type = field.getType();
      auto &name = field.getName();
      switch (type.getBaseType())
      {
        case BaseType::NOT_A_TYPE:
          throw std::logic_error("Field " + name + " as type NOT_A_TYPE");
          break;
        case BaseType::CHAR:
          return std::any(static_cast<char>(value));
        case BaseType::INT8:
          return std::any(static_cast<int8_t>(value));
        case BaseType::INT16:
          return std::any(static_cast<int16_t>(value));
        case BaseType::INT32:
          return std::any(static_cast<int32_t>(value));
        case BaseType::UINT8:
          return std::any(static_cast<uint8_t>(value));
        case BaseType::UINT16:
          return std::any(static_cast<uint16_t>(value));
        case BaseType::UINT32:
          return std::any(static_cast<uint32_t>(value));
        case BaseType::FLOAT:
          return std::any(static_cast<float>(value));
        case BaseType::STRING:
          std::stringstream sstr;
          sstr << value;
          return std::any(sstr.str());
      }
      return std::any();
    }
  };

  class FieldValue {
  public:
    FieldValue() : field("","char") {}

    template<
      typename T,
      typename = typename std::enable_if<std::is_arithmetic<T>::value>::type
    >
    FieldValue(const MessageField &field, const T *array, size_t size) : field(field)
    {
      const auto &type = field.getType();
      if (type.isArray())
      {
        if (type.getArraySize() && type.getArraySize() != size)
        {
          std::stringstream sstr;
          sstr << "Wrong size in building value for " << field.getName() << ", got " << size << " / expected "
               << type.getArraySize();
          throw std::logic_error(sstr.str());
        }
        std::vector <std::any> vec;
        vec.resize(size);
        for (size_t i = 0; i < size; ++i)
        {
          vec[i] = FieldValueBuilder<T>::MakeStdAny(field, array[i]);
        }
        value = std::any(vec);
      }
      else
      {
        throw std::logic_error("Cannot build scalar field from an array");
      }
    }

    template<
      typename T,
      typename = typename std::enable_if<std::is_same<std::string, T>::value>::type
    >
    FieldValue(const MessageField &field, const T &str) : field(field)
    {
      const auto &type = field.getType();
      if (type.isArray())
      {
        // Values are comma separated in string
        std::vector <std::any> vec;
        size_t prev = 0;
        auto pos = str.find_first_of(',');
        while (prev != std::string::npos)
        {
          std::stringstream sstr(str.substr(prev, pos));
          switch (field.getType().getBaseType())
          {
            case BaseType::CHAR:
            {
              char v;
              sstr >> v;
              vec.emplace_back(v);
            }
              break;
            case BaseType::INT8:
            {
              int v; // Should not read this as a char...
              sstr >> v;
              vec.emplace_back((int8_t)v);
            }
              break;
            case BaseType::INT16:
            {
              int16_t v;
              sstr >> v;
              vec.emplace_back(v);
            }
              break;
            case BaseType::INT32:
            {
              int32_t v;
              sstr >> v;
              vec.emplace_back(v);
            }
              break;
            case BaseType::UINT8:
            {
              int v; // Should not read this as a char...
              sstr >> v;
              vec.emplace_back((uint8_t)v);
            }
              break;
            case BaseType::UINT16:
            {
              uint16_t v;
              sstr >> v;
              vec.emplace_back(v);
            }
              break;
            case BaseType::UINT32:
            {
              uint32_t v;
              sstr >> v;
              vec.emplace_back(v);
            }
              break;
            case BaseType::FLOAT:
            {
              float v;
              sstr >> v;
              vec.emplace_back(v);
            }
              break;
            case BaseType::STRING:
            {
              vec.emplace_back(sstr.str());
            }
              break;
            case BaseType::NOT_A_TYPE:
              break;
          }
          if (pos == std::string::npos)
          {
            prev = pos;
          }
          else
          {
            prev = pos + 1;
            pos = str.find_first_of(',', prev);
          }
        }

        if (type.getArraySize() && type.getArraySize() != vec.size())
        {
          std::stringstream sstr;
          sstr << "Wrong size in building value for " << field.getName() << ", got " << vec.size() << " / expected "
               << type.getArraySize();
          throw std::logic_error(sstr.str());
        }
        value = std::any(vec);
      }
      else
      {
        switch (field.getType().getBaseType())
        {
          case BaseType::INT8:
          {
            int8_t v;
            std::stringstream sstr(str);
            sstr >> v;
            value = std::any(v);
          }
            break;
          case BaseType::INT16:
          {
            int16_t v;
            std::stringstream sstr(str);
            sstr >> v;
            value = std::any(v);
          }
            break;
          case BaseType::INT32:
          {
            int32_t v;
            std::stringstream sstr(str);
            sstr >> v;
            value = std::any(v);
          }
            break;
          case BaseType::UINT8:
          {
            uint8_t v;
            std::stringstream sstr(str);
            sstr >> v;
            value = std::any(v);
          }
            break;
          case BaseType::UINT16:
          {
            uint16_t v;
            std::stringstream sstr(str);
            sstr >> v;
            value = std::any(v);
          }
            break;
          case BaseType::UINT32:
          {
            uint32_t v;
            std::stringstream sstr(str);
            sstr >> v;
            value = std::any(v);
          }
            break;
          case BaseType::FLOAT:
          {
            float v;
            std::stringstream sstr(str);
            sstr >> v;
            value = std::any(v);
          }
            break;
          case BaseType::STRING:
            value = std::any(std::string(str));
            break;
          case BaseType::CHAR:
            throw std::logic_error("Cannot build a char value from a char array");
            break;
          case BaseType::NOT_A_TYPE:
            throw std::logic_error("Field " + field.getName() + " as type NOT_A_TYPE");
            break;
        }
      }
    }

    template<
      typename T,
      typename = typename std::enable_if<std::is_same<char, T>::value>::type
    >
    FieldValue(const MessageField &field, const T *s) : FieldValue(field, std::string(s))
    {
    }

    template<
      typename Container,
      typename T = typename Container::value_type,
      typename = typename std::enable_if<
        !std::is_arithmetic<Container>::value && !std::is_same<std::string, Container>::value>::type
    >
    FieldValue(const MessageField &field, const Container &c) : field(field)
    {
      std::vector <std::any> v;
      const auto &type = field.getType();
      if (type.getArraySize() && type.getArraySize() != c.size())
      {
        std::stringstream sstr;
        sstr << "Wrong size in building value for " << field.getName() << ", got " << c.size() << " / expected "
             << type.getArraySize();
        throw std::logic_error(sstr.str());
      }
      v.reserve(c.size());
      for (auto val: c)
      {
        v.push_back(FieldValueBuilder<T>::MakeStdAny(field, val));
      }
      value = std::any(v);
    };

    template<
      typename T,
      typename = typename std::enable_if<std::is_arithmetic<T>::value>::type
    >
    FieldValue(const MessageField &field, T v) : field(field)
    {
      value = FieldValueBuilder<T>::MakeStdAny(field, v);
    }

    template<
      typename T,
      size_t Size>
    void getValue(std::array <T, Size> &c) const
    {
      auto vec = std::any_cast < std::vector < std::any >> (value);
      for (size_t i = 0; i < c.size(); ++i)
      {
        c[i] = std::any_cast<T>(vec[i]);
      }
    }

    template<
      typename Container,
      typename T = typename Container::value_type,
      typename = typename std::enable_if<
        !std::is_arithmetic<Container>::value && !std::is_same<std::string, Container>::value>::type
    >
    void getValue(Container &c) const
    {
      c.clear();
      auto vec = std::any_cast < std::vector < std::any >> (value);
      for (auto val: vec)
      {
        c.push_back(std::any_cast<T>(val));
      }
    }

    template<
      typename T,
      typename E = typename std::enable_if<std::is_arithmetic<T>::value>::type
    >
    void getValue(T &val) const
    {
      val = std::any_cast<T>(value);
    }

    template<typename T,
      typename = typename std::enable_if<std::is_same<std::string, T>::value>::type,
      typename = typename std::enable_if<!std::is_arithmetic<T>::value>::type,
      typename = typename T::value_type>
    void getValue(T &val) const
    {
      val = std::any_cast<T>(value);
    }

    [[nodiscard]] const MessageField &getField() const
    {
      return field;
    }

    [[nodiscard]] const FieldType &getType() const
    {
      return field.getType();
    }

    [[nodiscard]] const std::string &getName() const
    {
      return field.getName();
    }

    const std::any &getValue() const
    {
      return value;
    }

    inline bool isOutputInt8AsInt() const
    {
      return output_int8_as_int;
    }

    inline void setOutputInt8AsInt(bool outputInt8AsInt)
    {
      output_int8_as_int = outputInt8AsInt;
    }
  private:
    MessageField field;
    std::any value;
    bool output_int8_as_int=false;
  };
}
std::ostream& operator<<(std::ostream& o,const pprzlink::FieldValue& v);

#endif //PPRZLINKCPP_FIELDVALUE_H
