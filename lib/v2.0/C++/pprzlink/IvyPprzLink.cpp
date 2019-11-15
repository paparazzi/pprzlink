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

/** \file IvyPprzLink.cpp
 *
 *
 */


#include <pprzlink/IvyPprzLink.h>
#include <pprzlink/exceptions/pprzlink_exception.h>
#include <ivy-c++/IvyApplication.h>
#include <iostream>

namespace pprzlink {

  IvyPprzLink::IvyPprzLink(MessageDictionary const & dict , std::string appName, std::string domain, bool threadedIvy)
  : dictionary (dict), domain(domain), appName(appName), threaded(threadedIvy)
  {
    bus = new Ivy(appName.c_str(), (appName + " ready").c_str(), this, threadedIvy);
    bus->start(domain.c_str());
  }

  IvyPprzLink::~IvyPprzLink()
  {
    bus->stop();
    if (threaded)
    {
      auto thr = bus->getThread();
      thr->detach();
      delete (bus);
    }
    else
    {
      delete (bus);
    }
  }

  void IvyPprzLink::OnApplicationConnected(IvyApplication *app)
  {
  }

  void IvyPprzLink::OnApplicationDisconnected(IvyApplication *app)
  {
  }

  void IvyPprzLink::OnApplicationCongestion(IvyApplication *app)
  {
  }

  void IvyPprzLink::OnApplicationDecongestion(IvyApplication *app)
  {
  }

  void IvyPprzLink::OnApplicationFifoFull(IvyApplication *app)
  {
  }

  long IvyPprzLink::BindMessage(const MessageDefinition &def, messageCallback_t cb)
  {
    auto mcb = new MessageCallback(dictionary, cb);
    auto regexp = regexpForMessageDefinition(def);
    //std::cout << "Binding to " << regexp << std::endl;
    auto id = bus->BindMsg(regexp.c_str(), mcb);
    callbackMap[id] = mcb;
    return id;
  }

  void IvyPprzLink::UnbindMessage(int bindId)
  {
    auto iter = callbackMap.find(bindId);
    if (iter == callbackMap.end())
    {
      throw no_such_binding("Cannot unbind (bind id invalid)");
    }
    bus->UnbindMsg(bindId);
    delete (iter->second);
    callbackMap.erase(bindId);
  }

  std::string IvyPprzLink::regexpForMessageDefinition(const MessageDefinition &def)
  {
    static const std::map<BaseType, std::string> typeRegex = {
      {BaseType::CHAR,   "."},
      {BaseType::INT8,   "-?\\d+"},
      {BaseType::INT16,  "-?\\d+"},
      {BaseType::INT32,  "-?\\d+"},
      {BaseType::UINT8,  "\\d+"},
      {BaseType::UINT16, "\\d+"},
      {BaseType::UINT32, "\\d+"},
      {BaseType::FLOAT,  "-?\\d+(?:\\.)?(?:\\d)*"},
      {BaseType::STRING, "(?:\"[^\"]+\"|[^ ]+)"}
    };
    // ac_id MSG_NAME msgField*
    std::stringstream sstr;
    sstr << "^([^ ]*) (" << def.getName() << ")";

    for (int i = 0; i < def.getNbFields(); ++i)
    {
      //std::cout << def.getField(i).getName() << " : " << def.getField(i).getType().toString() << std::endl;
      auto iter = typeRegex.find(def.getField(i).getType().getBaseType());
      if (iter == typeRegex.end())
      {
        throw wrong_message_format(
          "IvyregexpForMessageDefinition found NOT_A_TYPE in message " + def.getField(i).getName());
      }
      std::string baseRegex = iter->second;

      if (def.getField(i).getType().isArray())
      {
        if (def.getField(i).getType().getBaseType()==BaseType::CHAR)
        {
          sstr << " (\"[^\"]*\")";
        }
        else
        {
          if (def.getField(i).getType().getArraySize() == 0)
          {
            // Dynamic array
            // s => s,s,s,s => (s(?:,s)*)
            sstr << " (" << baseRegex << "(?:," << baseRegex << ")*)";
          }
          else
          {
            // Static array
            // s => s,s,s => (s(?:,s){SIZE-1})
            sstr << " (" << baseRegex << "(?:," << baseRegex << "){" << def.getField(i).getType().getArraySize() - 1
                 << "})";
          }
        }
      }
      else
      {
          sstr << " (" << baseRegex << ")";
      }
    }
    sstr << "$";

    return sstr.str();
  }

  void IvyPprzLink::sendMessage(const std::string& ac_id, const Message& msg)
  {
    std::stringstream fieldsStream;

    const auto &def=msg.getDefinition();

    for (int i=0;i<def.getNbFields();++i)
    {
      if (i!=0)
        fieldsStream << " ";
      auto val= msg.getRawValue(i);
      val.setOutputInt8AsInt(true);
      fieldsStream << val;
    }
    bus->SendMsg("%s %s %s",ac_id.c_str(), def.getName().c_str(), fieldsStream.str().c_str());
  }

  MessageCallback::MessageCallback(const MessageDictionary &dictionary, const messageCallback_t &cb) : dictionary(
    dictionary), cb(cb)
  {
  }

  void MessageCallback::OnMessage(IvyApplication *app, int argc, const char **argv)
  {
    MessageDefinition def = dictionary.getDefinition(argv[1]);
    Message msg(def);
    if (def.getNbFields() != argc - 2)
    {
      std::stringstream sstr;
      sstr << argv[1] << " message with wrong number of fields (expected " << def.getNbFields() << " / got " << argc - 2
           << ")";
      throw wrong_message_format(sstr.str());
    }
    for (int i = 2; i < argc; ++i)
    {
      const auto& field = def.getField(i - 2);
      // Need deserializing string to build FieldValue

      // For char arrays and strings remove possible quotes
      if ((field.getType().getBaseType()==BaseType::STRING || (field.getType().getBaseType()==BaseType::CHAR && field.getType().isArray())) && argv[i][0]=='"')
      {
        std::string str(argv[i]);
        std::cout << str.substr(1,str.size()-2) << std::endl;
        msg.addField(field.getName(),str.substr(1,str.size()-2));
      }
      else
      {
        std::stringstream sstr(argv[i]);
        if (field.getType().isArray())
        {
          switch (field.getType().getBaseType())
          {
            case BaseType::NOT_A_TYPE:
              throw std::logic_error("NOT_A_TYPE for field " + field.getName() + " in message " + argv[1]);
              break;
            case BaseType::CHAR:
              throw wrong_message_format("Wrong field format for a char[] "+std::string(argv[i]));
              break;
            case BaseType::INT8:
            case BaseType::INT16:
            case BaseType::INT32:
            case BaseType::UINT8:
            case BaseType::UINT16:
            case BaseType::UINT32:
            case BaseType::FLOAT:
            {
              // Parse all numbers as a double
              std::vector<double> values;
              while (!sstr.eof())
              {
                double val;
                char c;
                sstr >> val >> c;
                if (c!=',')
                {
                  throw wrong_message_format("Wrong format for array "+std::string(argv[i]));
                }
                values.push_back(val);
              }
              msg.addField(field.getName(), values); // The value will be statically cast to the right type
            }
              break;
            case BaseType::STRING:
              msg.addField(field.getName(), argv[i]);
              break;
          }
        }
        else
        {
          switch (field.getType().getBaseType())
          {
            case BaseType::NOT_A_TYPE:
              throw std::logic_error("NOT_A_TYPE for field " + field.getName() + " in message " + argv[1]);
              break;
            case BaseType::CHAR:
            {
              char val;
              sstr >> val;
              msg.addField(field.getName(), val);
            }
              break;
            case BaseType::INT8:
            case BaseType::INT16:
            case BaseType::INT32:
            case BaseType::UINT8:
            case BaseType::UINT16:
            case BaseType::UINT32:
            case BaseType::FLOAT:
            {
              // Parse all numbers as a double
              double val;
              sstr >> val;
              msg.addField(field.getName(), val); // The value will be statically cast to the right type
            }
              break;
            case BaseType::STRING:
              msg.addField(field.getName(), argv[i]);
              break;
          }
        }
      }
    }
    std::string sender(argv[0]);
    if (argv[0][0]=='"')
    {
      // Remove quotes from string if needed
      sender=sender.substr(1,sender.size()-2);
    }

    cb(sender, msg);
  }
}