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

/** \file IvyLink.h
 *
 *
 */


#ifndef PPRZLINKCPP_IVYLINK_H
#define PPRZLINKCPP_IVYLINK_H

#include <string>
#include <pprzlink/MessageDictionary.h>
#include <ivy-c++/Ivy.h>
#include <pprzlink/Message.h>

namespace pprzlink {
  class MessageCallback;
  class AircraftCallback;

  using messageCallback_t = std::function<void(std::string,Message)>;

  /**
   *
   */
  class IvyLink : public IvyApplicationCallback {
  public:
    /**
     *
     * @param dict
     * @param appName
     * @param domain
     * @param threadedIvy
     */
    IvyLink(MessageDictionary const & dict , std::string appName, std::string domain = "127.255.255.255:2010", bool threadedIvy = false);

    /**
     *
     */
    ~IvyLink();

    /**
     *
     * @param def
     * @param cb
     * @return
     */
    long BindMessage(MessageDefinition const & def, messageCallback_t cb);

    /**
     * Bind on all messages coming from given ac_id
     * @param ac_id
     * @param cb
     * @return
     */
    long BindOnSrcAc(std::string ac_id, messageCallback_t cb);

    /**
     *
     * @param bindId
     */
    void UnbindMessage(int bindId);

    /**
     *
     * @param ac_id
     * @param msg
     */
    void sendMessage(const Message& msg);

  private:
    const MessageDictionary &dictionary;
    std::string domain;
    std::string appName;
    Ivy *bus;
    bool threaded;

    /**
     *
     * @param def
     * @return
     */
    std::string regexpForMessageDefinition(MessageDefinition const & def);

    std::map<long,MessageCallback*> messagesCallbackMap;
    std::map<long,AircraftCallback*> aircraftCallbackMap;

    /**
     *
     * @param app
     */
    void OnApplicationConnected(IvyApplication *app) override;

    /**
     *
     * @param app
     */
    void OnApplicationDisconnected(IvyApplication *app) override;

    /**
     *
     * @param app
     */
    void OnApplicationCongestion(IvyApplication *app) override;

    /**
     *
     * @param app
     */
    void OnApplicationDecongestion(IvyApplication *app) override;

    /**
     *
     * @param app
     */
    void OnApplicationFifoFull(IvyApplication *app) override;
  };

  /**
   *
   */
  class MessageCallback : public IvyMessageCallback {
  public:
    /**
     *
     * @param dictionary
     * @param cb
     */
    MessageCallback(const MessageDictionary &dictionary,const messageCallback_t &cb);

    /**
     *
     * @param app
     * @param argc
     * @param argv
     */
    void OnMessage(IvyApplication *app, int argc, const char **argv) override;

  private:
    const MessageDictionary &dictionary;
    messageCallback_t cb;
  };

  /**
 *
 */
  class AircraftCallback : public IvyMessageCallback {
  public:
    /**
     *
     * @param dictionary
     * @param cb
     */
    AircraftCallback(const MessageDictionary &dictionary,const messageCallback_t &cb);

    /**
     *
     * @param app
     * @param argc
     * @param argv
     */
    void OnMessage(IvyApplication *app, int argc, const char **argv) override;

  private:
    const MessageDictionary &dictionary;
    messageCallback_t cb;
  };
}
#endif //PPRZLINKCPP_IVYLINK_H
