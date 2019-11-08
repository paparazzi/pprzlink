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

/** \file FieldValue.cpp
 *
 *
 */
#include "FieldValue.h"

std::ostream& operator<<(std::ostream& o,const pprzlink::FieldValue& v)
{
  const auto& type=v.getType();
  const auto& name=v.getName();
  if (type.isArray())
  {
    switch (type.getBaseType())
    {
      case pprzlink::BaseType::NOT_A_TYPE:
        throw std::logic_error("NOT_A_TYPE for field "+name);
        break;
      case pprzlink::BaseType::CHAR:
      {
        std::vector<char> vec;
        v.getValue(vec);
        for (size_t i=0;i<vec.size();++i)
        {
          if (i!=0)
            o << ",";
          o << vec[i];
        }
      }
        break;
      case pprzlink::BaseType::INT8:
      {
        std::vector<int8_t> vec;
        v.getValue(vec);
        for (size_t i=0;i<vec.size();++i)
        {
          if (i!=0)
            o << ",";
          if (v.isOutputInt8AsInt())
            o << (int)vec[i];
          else
            o << vec[i];
        }
      }
        break;
      case pprzlink::BaseType::INT16:
      {
        std::vector<int16_t> vec;
        v.getValue(vec);
        for (size_t i=0;i<vec.size();++i)
        {
          if (i!=0)
            o << ",";
          o << vec[i];
        }
      }
        break;
      case pprzlink::BaseType::INT32:
      {
        std::vector<int32_t> vec;
        v.getValue(vec);
        for (size_t i=0;i<vec.size();++i)
        {
          if (i!=0)
            o << ",";
          o << vec[i];
        }
      }
        break;
      case pprzlink::BaseType::UINT8:
      {
        std::vector<uint8_t> vec;
        v.getValue(vec);
        for (size_t i=0;i<vec.size();++i)
        {
          if (i!=0)
            o << ",";
          if (v.isOutputInt8AsInt())
            o << (int)vec[i];
          else
            o << vec[i];
        }
      }
        break;
      case pprzlink::BaseType::UINT16:
      {
        std::vector<uint16_t> vec;
        v.getValue(vec);
        for (size_t i=0;i<vec.size();++i)
        {
          if (i!=0)
            o << ",";
          o << vec[i];
        }
      }
        break;
      case pprzlink::BaseType::UINT32:
      {
        std::vector<uint32_t> vec;
        v.getValue(vec);
        for (size_t i=0;i<vec.size();++i)
        {
          if (i!=0)
            o << ",";
          o << vec[i];
        }
      }
        break;
      case pprzlink::BaseType::FLOAT:
      {
        std::vector<float> vec;
        v.getValue(vec);
        for (size_t i=0;i<vec.size();++i)
        {
          if (i!=0)
            o << ",";
          o << vec[i];
        }
      }
        break;
      case pprzlink::BaseType::STRING:
      {
        std::vector<std::string> vec;
        v.getValue(vec);
        for (size_t i=0;i<vec.size();++i)
        {
          if (i!=0)
            o << ",";
          o << vec[i];
        }
      }
        break;
    }
  }
  else
  {
    switch (type.getBaseType())
    {
      case pprzlink::BaseType::NOT_A_TYPE:
        throw std::logic_error("NOT_A_TYPE for field "+name);
        break;
      case pprzlink::BaseType::CHAR:
      {
        char val;
        v.getValue(val);
        o << val;
      }
        break;
      case pprzlink::BaseType::INT8:
      {
        int8_t val;
        v.getValue(val);
        if (v.isOutputInt8AsInt())
        {
          o << (int)val;
        }
        else
        {
          o << val;
        }
      }
        break;
      case pprzlink::BaseType::INT16:
      {
        int16_t val;
        v.getValue(val);
          o << val;
      }
        break;
      case pprzlink::BaseType::INT32:
      {
        int32_t val;
        v.getValue(val);
        o << val;
      }
        break;
      case pprzlink::BaseType::UINT8:
      {
        uint8_t val;
        v.getValue(val);
        if (v.isOutputInt8AsInt())
        {
          o << (int)val;
        }
        else
        {
          o << val;
        }
      }
        break;
      case pprzlink::BaseType::UINT16:
      {
        uint16_t val;
        v.getValue(val);
        o << val;
      }
        break;
      case pprzlink::BaseType::UINT32:
      {
        uint32_t val;
        v.getValue(val);
        o << val;
      }
        break;
      case pprzlink::BaseType::FLOAT:
      {
        float val;
        v.getValue(val);
        o << val;
      }
        break;
      case pprzlink::BaseType::STRING:
      {
        std::string val;
        v.getValue(val);
        o << val;
      }
        break;
    }
  }

  return o;
}



