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

/** \file IvyLink.cpp
 *
 *
 */


#include <pprzlink/IvyLink.h>
#include <pprzlink/exceptions/pprzlink_exception.h>
#include <ivy-c++/IvyApplication.h>
#include <iostream>
#include <regex>

namespace pprzlink {

  IvyLink::IvyLink(MessageDictionary const & dict , std::string appName, std::string domain, bool threadedIvy)
  : dictionary (dict), domain(domain), appName(appName), threaded(threadedIvy), requestNb(0)
  {
    bus = new Ivy(appName.c_str(), (appName + " ready").c_str(), this, threadedIvy);
    bus->start(domain.c_str());
  }

  IvyLink::~IvyLink()
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

  void IvyLink::OnApplicationConnected(IvyApplication *app)
  {
  }

  void IvyLink::OnApplicationDisconnected(IvyApplication *app)
  {
  }

  void IvyLink::OnApplicationCongestion(IvyApplication *app)
  {
  }

  void IvyLink::OnApplicationDecongestion(IvyApplication *app)
  {
  }

  void IvyLink::OnApplicationFifoFull(IvyApplication *app)
  {
  }

  long IvyLink::BindMessage(const MessageDefinition &def, messageCallback_t cb)
  {
    auto mcb = new MessageCallback(dictionary, cb);
    auto regexp = regexpForMessageDefinition(def);
    //std::cout << "Binding to " << regexp << std::endl;
    auto id = bus->BindMsg(regexp.c_str(), mcb);
    messagesCallbackMap[id] = mcb;
    return id;
  }

  long IvyLink::BindOnSrcAc(std::string ac_id, messageCallback_t cb)
  {
  auto mcb = new AircraftCallback(dictionary, cb);
    std::stringstream regexp;
    regexp << "^(" << ac_id << ") " << "([^ ]*)( .*)?$";
    //std::cout << "Binding to " << regexp.str() << std::endl;
    auto id = bus->BindMsg(regexp.str().c_str(), mcb);
    aircraftCallbackMap[id] = mcb;
    return id;
  }

  void IvyLink::UnbindMessage(int bindId)
  {
    auto iter = messagesCallbackMap.find(bindId);
    if (iter == messagesCallbackMap.end())
    {
      auto iter = aircraftCallbackMap.find(bindId);
      if (iter == aircraftCallbackMap.end())
      {
        throw no_such_binding("Cannot unbind (bind id invalid)");
      }
      else
      {
        bus->UnbindMsg(bindId);
        delete (iter->second);
        aircraftCallbackMap.erase(bindId);
      }
    }
    else
    {
      bus->UnbindMsg(bindId);
      delete (iter->second);
      messagesCallbackMap.erase(bindId);
    }
  }

  std::string IvyLink::regexpForMessageDefinition(const MessageDefinition &def)
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

    for (size_t i = 0; i < def.getNbFields(); ++i)
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

  void IvyLink::getMessageData(const Message& msg, std::string &ac_id, std::string &name, std::string &fields) {
    std::stringstream fieldsStream;
    if (msg.getSenderId().index()==0) // The variant holds a string
    {
      ac_id = std::get<std::string>(msg.getSenderId());
    }
    else
    {
      std::stringstream sstr;
      sstr << (int)std::get<uint8_t>(msg.getSenderId());
      ac_id = sstr.str();
    }
    const auto &def=msg.getDefinition();

    for (size_t i=0;i<def.getNbFields();++i)
    {
      if (i!=0)
        fieldsStream << " ";
      auto val= msg.getRawValue(i);
      val.setOutputInt8AsInt(true);
      fieldsStream << val;
    }

    name = def.getName();
    fields = fieldsStream.str();

  }

  void IvyLink::sendMessage(const Message& msg)
  {
    const auto &def=msg.getDefinition();
    if(def.isRequest()) {
      throw message_is_request("Message " + def.getName() + " is a request message. Use sendRequest instead!");
    }

    std::string ac_id;
    std::string name;
    std::string fields;

    getMessageData(msg, ac_id, name, fields);


    bus->SendMsg("%s %s %s",ac_id.c_str(), def.getName().c_str(), fields.c_str());

  }



  long IvyLink::sendRequest(const Message& msg, messageCallback_t cb) {
    const auto &def=msg.getDefinition();

    if(!def.isRequest()) {
      throw message_is_not_request("Message " + def.getName() + " is not a request message. Use sendMessage instead!");
    }

    // remove last 4 characters (_REQ) from request name to get the answer message name
    auto ansName = def.getName().substr(0, def.getName().size() - 4);
    auto ansDef = dictionary.getDefinition(ansName);

    std::string ac_id;
    std::string name;
    std::string fields;

    getMessageData(msg, ac_id, name, fields);

    std::stringstream requestIdStream;
    requestIdStream << getpid() << "_" << requestNb++;
    std::string requestId = requestIdStream.str();



    auto mcb = new MessageCallback(dictionary, [=](std::string ac_id,Message msg) {
      cb(std::move(ac_id), std::move(msg));

      // find bind id, then unbind answer message
      auto iter = requestBindId.left.find(requestId);
      if (iter != requestBindId.left.end())
      {
        // TODO make these two lines work ! (crash on Unbind)
        //long id = iter->second;
        //UnbindMessage(id);
        requestBindId.left.erase(requestId);

      }
    });

    std::stringstream regexpStream;
    regexpStream << requestId << " " << regexpForMessageDefinition(ansDef).erase(0,1);
    auto id = bus->BindMsg(regexpStream.str().c_str(), mcb);
    messagesCallbackMap[id] = mcb;
    requestBindId.insert({requestId, id});

    std::cout << "sendRequest requestBindId size: " << requestBindId.size() << std::endl;

    bus->SendMsg("%s %s %s %s",ac_id.c_str(), requestId.c_str(), def.getName().c_str(), fields.c_str());

    return id;
  }

  MessageCallback::MessageCallback(const MessageDictionary &dictionary, const messageCallback_t &cb) : dictionary(
    dictionary), cb(cb)
  {
  }

  void MessageCallback::OnMessage(IvyApplication *app, int argc, const char **argv)
  {
    MessageDefinition def = dictionary.getDefinition(argv[1]);
    Message msg(def);
    if (def.getNbFields() != (size_t)(argc - 2) )
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
        //std::cout << str.substr(1,str.size()-2) << std::endl;
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

  AircraftCallback::AircraftCallback(const MessageDictionary &dictionary, const messageCallback_t &cb) : dictionary(dictionary), cb(cb)
  {
  }

  void AircraftCallback::OnMessage(IvyApplication *app, int argc, const char **argv)
  {
    MessageDefinition def = dictionary.getDefinition(argv[1]);
    Message msg(def);

    if (argc==3) // If we have fields to parse
    {
      std::regex fieldRegex("([^ ]+|\"[^\"]+\")");

      std::smatch results;
      std::vector<std::string> fields;
      std::string fieldsStr(argv[2]);
      while (std::regex_search(fieldsStr, results, fieldRegex))
      {
        fields.push_back(results.str());
        fieldsStr = results.suffix();
      }

      if (def.getNbFields()!=fields.size()) // check that the number of fields is correct
      {
        throw wrong_message_format("Wrong number of fields in message " + std::string(argv[1]));
      }

      for (size_t i = 0; i < def.getNbFields(); ++i)
      {
        const auto &field = def.getField(i);

        // For char arrays and strings remove possible quotes
        if ((field.getType().getBaseType() == BaseType::STRING ||
             (field.getType().getBaseType() == BaseType::CHAR && field.getType().isArray())) && argv[i][0] == '"')
        {
          std::string &str(fields[i]);
          //std::cout << str.substr(1,str.size()-2) << std::endl;
          msg.addField(field.getName(), str.substr(1, str.size() - 2));
        }
        else
        {
          std::stringstream sstr(fields[i]);
          if (field.getType().isArray())
          {
            switch (field.getType().getBaseType())
            {
              case BaseType::NOT_A_TYPE:
                throw std::logic_error("NOT_A_TYPE for field " + field.getName() + " in message " + argv[1]);
                break;
              case BaseType::CHAR:
                throw wrong_message_format("Wrong field format for a char[] " + fields[i]);
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
                  if (c != ',')
                  {
                    throw wrong_message_format("Wrong format for array \"" + fields[i] + "\"");
                  }
                  values.push_back(val);
                }
                msg.addField(field.getName(), values); // The value will be statically cast to the right type
              }
                break;
              case BaseType::STRING:
                msg.addField(field.getName(), fields[i]);
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
                msg.addField(field.getName(), fields[i]);
                break;
            }
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
    msg.setSenderId(sender);

    cb(sender, msg);
  }
}
