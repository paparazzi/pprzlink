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

/** \file IvyPprzLink.h
 *
 *
 */


#ifndef PPRZLINKCPP_IVYPPRZLINK_H
#define PPRZLINKCPP_IVYPPRZLINK_H

#include <string>
#include "MessageDictionary.h"
#include "../ivy-c++/Ivy.h"
#include "Message.h"

namespace pprzlink {
  class MessageCallback;

  using messageCallback_t = std::function<void(std::string,Message)>;
  class IvyPprzLink : public IvyApplicationCallback {
  public:
    IvyPprzLink(MessageDictionary const & dict ,std::string appName,std::string domain = "127.255.255.255:2010",  bool threadedIvy = false);

    ~IvyPprzLink();

    long BindMessage(MessageDefinition const & def, messageCallback_t cb);
    void UnbindMessage(int bindId);

    void sendMessage(const std::string& ac_id,const Message& msg);

  private:
    const MessageDictionary &dictionary;
    std::string domain;
    std::string appName;
    Ivy *bus;
    bool threaded;

    std::string regexpForMessageDefinition(MessageDefinition const & def);

    std::map<long,MessageCallback*> callbackMap;

    void OnApplicationConnected(IvyApplication *app) override;
    void OnApplicationDisconnected(IvyApplication *app) override;
    void OnApplicationCongestion(IvyApplication *app) override;
    void OnApplicationDecongestion(IvyApplication *app) override;
    void OnApplicationFifoFull(IvyApplication *app) override;
  };

  class MessageCallback : public IvyMessageCallback {
  public:
    MessageCallback(const MessageDictionary &dictionary,const messageCallback_t &cb);

    void OnMessage(IvyApplication *app, int argc, const char **argv) override;

  private:
    const MessageDictionary &dictionary;
    messageCallback_t cb;
  };
}
#endif //PPRZLINKCPP_IVYPPRZLINK_H
