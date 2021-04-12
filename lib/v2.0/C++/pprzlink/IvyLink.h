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
#include <boost/bimap.hpp>

namespace pprzlink {
  class MessageCallback;
  class AircraftCallback;
  class RequestCallback;

  using messageCallback_t = std::function<void(std::string,Message)>;
  using answererCallback_t = std::function<Message(std::string,Message)>;

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
    void UnbindMessage(long bindId);

    /**
     *
     * @param ac_id
     * @param msg
     */
    void sendMessage(const Message& msg);

    long sendRequest(const Message& msg, messageCallback_t cb);

    long registerRequestAnswerer(const MessageDefinition &def, answererCallback_t cb);

  private:
    const MessageDictionary &dictionary;
    std::string domain;
    std::string appName;
    Ivy *bus;
    bool threaded;
    unsigned int requestNb;
    boost::bimap<std::string, long> requestBindId;

    void getMessageData(const Message& msg, std::string &ac_id, std::string &name, std::string &fieldStream);

    /**
     *
     * @param def
     * @return
     */
    std::string regexpForMessageDefinition(MessageDefinition const & def);

    std::string messageRegexp(MessageDefinition const & def);

    std::map<long,MessageCallback*> messagesCallbackMap;
    std::map<long,AircraftCallback*> aircraftCallbackMap;
    std::map<long,RequestCallback*> requestCallbackMap;

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


  class RequestCallback : public MessageCallback {
  public:
    /**
     *
     * @param dictionary
     * @param cb
     */
    RequestCallback(const MessageDictionary &dictionary,const messageCallback_t &cb);

    /**
     *
     * @param app
     * @param argc
     * @param argv
     */
    void OnMessage(IvyApplication *app, int argc, const char **argv) override;

    std::string& getRequestId() {return requestId;}

  private:
    std::string requestId;
  };

}
#endif //PPRZLINKCPP_IVYLINK_H
