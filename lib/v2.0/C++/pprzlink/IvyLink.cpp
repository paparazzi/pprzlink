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
 * @file IvyLink.cpp
 * @brief Ivy bindings, request correlation and callback lifetime management.
 * @ingroup ivy
 *
 * Subscription ownership is retained behind legacy IDs or returned as scoped tokens. Request replies are one-shot and correlated by a process-wide sequence.
 */

#include <pprzlink/IvyLink.h>
#include <pprzlink/IvyMessageCodec.h>
#include <pprzlink/exceptions/pprzlink_exception.h>
#include <system_error>
#include <unistd.h>

namespace pprzlink {
  /// @brief Internal conversion of native Ivy expected-results into C++ exceptions.
  /// @ingroup internals
  namespace {
    /// @brief Unwrap a native Ivy operation or throw its error with bus context.
    /// @tparam T Native result value type, including void.
    /// @param[in] result Move-owned expected value/error.
    /// @return The successful value for non-void T; no value for void T.
    /// @throws std::system_error The native operation reports an error.
    template<class T>
    T checked(std::expected<T, std::error_code> result)
    {
      if (!result) {
        throw std::system_error(result.error(), "Ivy");
      }
      if constexpr (!std::is_void_v<T>) {
        return std::move(*result);
      }
    }

  }

  IvyLink::IvyLink(const MessageDictionary &dict, std::string appName,
                   std::string domain, bool threadedIvy)
    : Receiver(dict, Kind::Ivy), dictionary(dict), bus(checked(ivy::Bus::create(appName, appName + " ready")))
  {
    checked(bus.start(domain));
    if (threadedIvy) {
      loop.emplace(checked(ivy::LoopThread::create(bus)));
    }
  }

  IvyLink::~IvyLink()
  {
    shutdown();
    loop.reset(); // Stop and join before releasing any callback captures.
    (void)bus.stop();
  }

  void IvyLink::run()
  {
    if (loop) {
      throw std::logic_error("This IvyLink already owns an event-loop thread");
    }
    start();
    checked(bus.run());
    checked(bus.take_callback_error());
  }

  void IvyLink::stop()
  {
    auto lock = lockReceiver();
    deactivate();
    ++receptionGeneration;
    reactiveSubscription.reset();
  }

  void IvyLink::close()
  {
    auto lock = lockReceiver();
    stop();
    checked(bus.request_stop());
  }

  void IvyLink::start()
  {
    auto lock = lockReceiver();
    if (!activate()) return;
    auto notification = guarded([this, session = ++receptionGeneration](std::string_view sender, std::string_view body) {
      if (session != receptionGeneration) return;
      std::optional<Message> message;
      try {
        const MessageDefinition *definition;
        try {
          definition = &dictionary.getDefinition(std::string(body.substr(0, body.find(' '))));
        } catch (const no_such_message &) {
          // Other Ivy applications and request envelopes share this bus.
          return;
        }
        message = ivy_codec::parseMessageBody(*definition, sender, body);
      } catch (...) { reportError(std::current_exception(), ReceiveError::Kind::Decode); return; }
      try { deliver(*message, ReceiveInfo{}); } catch (...) { stop(); throw; }
    });
    try {
      reactiveSubscription.emplace(checked(bus.bind_raw(
        [notification](IvyClientPtr, std::span<const std::string_view> arguments) mutable {
          if (arguments.size() == 2) notification(arguments[0], arguments[1]);
        }, ivy::runtime_regexp("^([^ ]*) (.*)$"))));
    } catch (...) { deactivate(); throw; }
  }

  long IvyLink::storeSubscription(ivy::Subscription subscription)
  {
    const long id = nextBindId++;
    std::lock_guard lock(subscriptionsMutex);
    subscriptions.emplace(id, std::move(subscription));
    return id;
  }

  void IvyLink::storeSubscription(long id, ivy::Bus::BindResult result)
  {
    auto subscription = checked(std::move(result));
    std::lock_guard lock(subscriptionsMutex);
    subscriptions.emplace(id, std::move(subscription));
  }

  long IvyLink::BindMessage(const MessageDefinition &def, messageCallback_t cb)
  {
    return storeSubscription(subscribeMessage(def, std::move(cb)));
  }

  ivy::Subscription IvyLink::subscribeMessage(std::string_view name, messageCallback_t cb)
  {
    return subscribeMessage(dictionary.getDefinition(std::string(name)), std::move(cb));
  }

  ivy::Subscription IvyLink::subscribeMessage(const MessageDefinition &def, messageCallback_t cb)
  {
    const auto regexp = "^([^ ]*) " + ivy_codec::messageRegexp(def);
    return checked(bus.bind_raw(
      [def, cb = std::move(cb)](IvyClientPtr, std::span<const std::string_view> args) {
        if (args.empty()) {
          throw wrong_message_format("Missing sender in " + def.getName());
        }
        auto msg = ivy_codec::parseFields(def, args.front(), args.subspan(1));
        cb(ivy_codec::unquote(args.front()), std::move(msg));
      }, ivy::runtime_regexp(regexp)));
  }

  long IvyLink::BindOnSrcAc(std::string ac_id, messageCallback_t cb)
  {
    return storeSubscription(subscribeSender(std::move(ac_id), std::move(cb)));
  }

  ivy::Subscription IvyLink::subscribeSender(std::string ac_id, messageCallback_t cb)
  {
    const auto regexp = "^(" + ivy_codec::escapeRegexp(ac_id) + ") (.*)$";
    return checked(bus.bind_raw(
      [this, cb = std::move(cb)](IvyClientPtr, std::span<const std::string_view> args) {
        if (args.size() != 2) {
          throw wrong_message_format("Missing sender or message body");
        }
        const auto sender = args[0];
        const auto body = args[1];
        const auto &def = dictionary.getDefinition(std::string(body.substr(0, body.find(' '))));
        auto msg = ivy_codec::parseMessageBody(def, sender, body);
        cb(ivy_codec::unquote(sender), std::move(msg));
      }, ivy::runtime_regexp(regexp)));
  }

  void IvyLink::UnbindMessage(long bindId)
  {
    decltype(subscriptions)::node_type subscription;
    {
      std::lock_guard lock(subscriptionsMutex);
      subscription = subscriptions.extract(bindId);
    }
    // Native unbind and capture destruction can call back into this link.
    if (!subscription.empty()) {
      checked(subscription.mapped().unbind());
    }
  }

  void IvyLink::sendMessage(const Message &msg)
  {
    if (msg.getDefinition().isRequest()) {
      throw message_is_request("Message " + msg.getDefinition().getName() +
                               " is a request message. Use sendRequest instead!");
    }
    checked(bus.send(ivy_codec::serializeMessage(msg)));
  }

  /// @details Install the answer binding before sending. A process-wide sequence
  /// and PID correlate the reply, and the callback unbinds its token before
  /// invoking user code. Failure to send rolls back the installed subscription.
  long IvyLink::sendRequest(const Message &msg, messageCallback_t cb)
  {
    const auto &def = msg.getDefinition();
    if (!def.isRequest()) {
      throw message_is_not_request("Message " + def.getName() + " is not a request message");
    }
    const auto answer = dictionary.getDefinition(def.getName().substr(0, def.getName().size() - 4));
    // Unique across all links in this process, including recreated links.
    static std::atomic<unsigned long long> requestNumber{0};
    const auto requestId = std::to_string(getpid()) + "_" + std::to_string(requestNumber++);
    const auto regexp = "^" + requestId + " ([^ ]*) " + ivy_codec::messageRegexp(answer);
    const long id = nextBindId++;
    auto result = bus.bind_raw(
      [this, answer, id, cb = std::move(cb)](IvyClientPtr, std::span<const std::string_view> args) {
        if (args.empty()) {
          throw wrong_message_format("Missing request answer sender");
        }
        auto reply = ivy_codec::parseFields(answer, args.front(), args.subspan(1));
        UnbindMessage(id); // One-shot, also when the application callback throws.
        cb(ivy_codec::unquote(args.front()), std::move(reply));
      }, ivy::runtime_regexp(regexp));
    storeSubscription(id, std::move(result));
    try {
      auto request = ivy_codec::serializeMessage(msg);
      request.insert(request.find(' ') + 1, requestId + " ");
      checked(bus.send(request));
    } catch (...) {
      UnbindMessage(id);
      throw;
    }
    return id;
  }

  long IvyLink::registerRequestAnswerer(const MessageDefinition &def, answererCallback_t cb)
  {
    return storeSubscription(subscribeRequestAnswerer(def, std::move(cb)));
  }

  ivy::Subscription IvyLink::subscribeRequestAnswerer(const MessageDefinition &def, answererCallback_t cb)
  {
    if (!def.isRequest()) {
      throw message_is_not_request("Message " + def.getName() + " is not a request message");
    }
    const auto answerName = def.getName().substr(0, def.getName().size() - 4);
    const auto regexp = "^([^ ]*) ([^ ]*) " + ivy_codec::messageRegexp(def);
    return checked(bus.bind_raw(
      [this, def, answerName, cb = std::move(cb)]
      (IvyClientPtr, std::span<const std::string_view> args) {
        if (args.size() < 2) {
          throw wrong_message_format("Missing request sender or ID");
        }
        auto request = ivy_codec::parseFields(def, args[0], args.subspan(2));
        auto answer = cb(ivy_codec::unquote(args[0]), std::move(request));
        if (answer.getDefinition().getName() != answerName) {
          throw wrong_answer_to_request("Wrong answer " + answer.getDefinition().getName() +
                                        " to request " + def.getName());
        }
        checked(bus.send(std::string(args[1]) + " " + ivy_codec::serializeMessage(answer)));
      }, ivy::runtime_regexp(regexp)));
  }

}
