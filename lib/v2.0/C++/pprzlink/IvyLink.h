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
 * @file IvyLink.h
 * @brief Independent Ivy buses, scoped subscriptions and request helpers.
 * @ingroup ivy
 *
 * A link borrows its dictionary and owns its bus. Application callbacks run on the selected event loop; subscription tokens and captured state need appropriate lifetimes.
 */

#ifndef PPRZLINKCPP_IVYLINK_H
#define PPRZLINKCPP_IVYLINK_H

#include <Ivy/ivy.hpp>
#include <Ivy/ivy_thread.hpp>
#include <pprzlink/Message.h>
#include <pprzlink/MessageDictionary.h>
#include <pprzlink/Receiver.h>
#include <boost/asio/io_context.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace pprzlink {

  /// @brief Observer receiving sender text and an owned decoded message on the selected event loop.
  /// @ingroup ivy
  using messageCallback_t = std::function<void(std::string, Message)>;
  /// @brief Request handler receiving sender/request values and returning its reply message.
  /// @ingroup ivy
  using answererCallback_t = std::function<Message(std::string, Message)>;

  /** Owns an independent Ivy bus. The dictionary must outlive the link.
   * @ingroup ivy
   * Sends and subscription operations may be called from multiple threads.
   * Without an Asio context, callbacks run on the Ivy loop. With a context,
   * all SDK message callbacks, Receiver predicates/error handlers and request
   * answerers are posted there and serialized per link. Native getBus() callbacks
   * still run on Ivy. The context and dictionary must outlive the link.
   * In Asio mode, close() suppresses queued callbacks and waits for active
   * callbacks on other threads; no callback starts after it returns. Unbinding can leave already
   * posted SDK callbacks queued. Keep their captured state alive until close()
   * or until those invocations finish. Queued captures may survive until the
   * context drains or is destroyed, but cannot access a closed/destroyed link.
   * Asio callback/reply errors propagate from context.run(); native decode errors
   * remain available through getBus().take_callback_error(). No Asio thread is
   * owned; service the context to receive callbacks. Do not synchronously wait
   * for replies from its callbacks. A work guard keeps context.run() running until close().
   * Destroy the link after external callers/loops have finished, never from a callback.
   */
  class IvyLink : public Receiver {
  public:
    /** Start a bus, optionally running its event loop on an owned thread.
     * With threadedIvy=false, call run() to service this bus.
     * @param[in] dict Borrowed, unchanged schemas that must outlive this link.
     * @param[in] appName Bus application name and prefix of its ready announcement.
     * @param[in] domain Ivy bus address and port.
     * @param[in] threadedIvy Whether to own an Ivy event-loop thread.
     * @throws std::system_error Native Ivy creation, startup or thread creation fails.
     */
    IvyLink(const MessageDictionary &dict, std::string appName,
            std::string domain = "127.255.255.255:2010", bool threadedIvy = false);
    /** Dispatch SDK callbacks on context, optionally owning an Ivy I/O thread.
     * Request answerers return normally on context; their correlated reply is
     * sent there without blocking the Ivy thread. Closing discards pending replies.
     * With threadedIvy=false, service the Ivy bus separately using run().
     * @param[in] context Borrowed application event loop.
     * @param[in] dict Borrowed immutable schemas.
     * @param[in] appName Ivy application name.
     * @param[in] domain Ivy bus address and port.
     * @param[in] threadedIvy Whether to own an Ivy event-loop thread.
     * @throws std::system_error Native startup or thread creation fails.
     */
    IvyLink(boost::asio::io_context &context, const MessageDictionary &dict,
            std::string appName, std::string domain = "127.255.255.255:2010", bool threadedIvy = true);
    /// @brief Stop/join the owned loop before releasing subscriptions and the bus.
    /// External callers and non-owned loops must have finished; do not destroy from a callback.
    ~IvyLink();

    /// @brief Copying an independent bus owner is prohibited.
    IvyLink(const IvyLink&) = delete;
    /// @brief Copy assignment is prohibited.
    /// @return This operation is deleted and cannot return.
    IvyLink& operator=(const IvyLink&) = delete;
    /// @brief Moving a live bus owner is prohibited.
    IvyLink(IvyLink&&) = delete;
    /// @brief Move assignment is prohibited.
    /// @return This operation is deleted and cannot return.
    IvyLink& operator=(IvyLink&&) = delete;

    /// Keep the returned Ivy token alive; destruction unsubscribes automatically.
    /// Callbacks execute on the selected event loop, as for the legacy binding methods.
    /// Unbinding does not wait for an executing callback or cancel one already
    /// posted to Asio. Captured state must survive those invocations or close().
    /// @param[in] definition Schema copied into the binding's capture state.
    /// @param[in] callback Observer receiving the sender and decoded message on the selected event loop.
    /// @return Owned scoped subscription token; keep it alive to remain subscribed.
    /// @throws std::exception Regex generation or native Ivy binding fails.
    [[nodiscard]] ivy::Subscription subscribeMessage(const MessageDefinition &definition, messageCallback_t callback);
    /// @brief Subscribe to a named schema using scoped token ownership.
    /// @param[in] name Exact dictionary message name.
    /// @param[in] callback Observer executed on the selected event loop.
    /// @return Owned token whose destruction unsubscribes without waiting for an active callback.
    /// @throws std::exception Dictionary lookup, regex generation or native binding fails.
    [[nodiscard]] ivy::Subscription subscribeMessage(std::string_view name, messageCallback_t callback);
    /// @brief Subscribe to all dictionary messages from one literal sender ID.
    /// @param[in] sender Sender text matched literally, not as a regular expression.
    /// @param[in] callback Observer receiving sender text and the parsed message.
    /// @return Owned token whose lifetime controls this subscription.
    /// @throws std::system_error Native Ivy binding fails; callback parsing failures are recorded by Ivy.
    [[nodiscard]] ivy::Subscription subscribeSender(std::string sender, messageCallback_t callback);
    /// @brief Answer requests of one schema using a scoped subscription.
    /// @param[in] definition Request schema whose name ends with _REQ.
    /// @param[in] callback Handler returning the corresponding message without the _REQ suffix.
    /// @return Owned answerer token; keep captured state alive through active callbacks.
    /// @throws message_is_not_request The schema is not a request.
    /// @throws std::system_error Binding fails; callback/reply errors reach the selected event loop.
    [[nodiscard]] ivy::Subscription subscribeRequestAnswerer(const MessageDefinition &definition,
                                                            answererCallback_t callback);

    /// @brief Create a legacy ID-owned subscription retained by this link.
    /// @param[in] def Message schema to observe.
    /// @param[in] cb Observer executed on the selected event loop.
    /// @return Binding ID accepted by UnbindMessage().
    /// @throws std::exception Schema regex generation or native binding fails.
    long BindMessage(const MessageDefinition &def, messageCallback_t cb);

    /// Bind all dictionary messages from this literal sender ID.
    /// @param[in] ac_id Literal sender text to match.
    /// @param[in] cb Observer receiving decoded sender/message values.
    /// @return Link-owned binding ID accepted by UnbindMessage().
    /// @throws std::system_error Native binding fails.
    long BindOnSrcAc(std::string ac_id, messageCallback_t cb);
    /// Cancel a binding. An already executing callback may finish after return.
    /// @param[in] bindId ID returned by a legacy bind or request helper; unknown IDs are harmless.
    /// @throws std::system_error Native unbinding fails.
    void UnbindMessage(long bindId);
    /// @brief Publish an ordinary populated message on this bus.
    /// @param[in] msg Non-request message to serialize in standard Ivy syntax.
    /// @throws message_is_request The schema ends with _REQ; use sendRequest().
    /// @throws std::exception Field formatting or native sending fails.
    void sendMessage(const Message &msg);
    /// @brief Send a request and register a one-shot correlated reply subscription.
    /// @param[in] msg Populated _REQ message; the answer schema must exist without that suffix.
    /// @param[in] cb Observer called after unbinding the reply subscription.
    /// @return Binding ID; call UnbindMessage() to cancel an unanswered request. No timeout is imposed.
    /// @throws std::exception Invalid request/schema, binding, formatting or native sending fails.
    long sendRequest(const Message &msg, messageCallback_t cb);
    /// @brief Retain a legacy ID-owned request answerer in this link.
    /// @param[in] def Request schema whose name ends with _REQ.
    /// @param[in] cb Handler returning the corresponding answer Message.
    /// @return Binding ID accepted by UnbindMessage().
    /// @throws std::exception Invalid request schema or native binding failure.
    long registerRequestAnswerer(const MessageDefinition &def, answererCallback_t cb);

    /// Run the loop on this thread until close() or a terminal bus error.
    /// Only valid when constructed with threadedIvy=false; stop() pauses reception
    /// without ending this loop.
    /// @throws std::logic_error This link already owns a loop thread.
    /// @throws std::system_error The native loop or a recorded callback error fails.
    void run();
    /// @brief Activate Receiver::bind subscriptions on this bus's event loop.
    /// One subscription per distinct bound message; any ALL binding replaces these
    /// with one broad subscription. No bindings means no subscription. Additional
    /// filters are applied locally. Binding changes update subscriptions while active;
    /// peers learn changes asynchronously, so transitions may lose in-flight messages.
    /// Names absent from the dictionary are ignored. Malformed fields of known
    /// subscribed messages are reported through onError() as ReceiveError::Kind::Decode.
    void start() override;
    /// Pause Receiver::bind reception while preserving bindings for start().
    /// Scoped/native subscriptions, sends and bus timers remain active.
    /// Safe inside callbacks; the event loop keeps running.
    void stop() override;
    /// @brief Permanently stop this Ivy session, cancel queued Asio callbacks/replies, and request return from run().
    /// Safe inside callbacks. Destruction joins the owned thread. A closed link
    /// cannot be restarted; create a new IvyLink for another session.
    /// @throws std::system_error The native stop request fails.
    void close();
    /// Borrow the bus for native API access and take_callback_error() inspection.
    /// Do not move/destroy it or run a second loop on it.
    /// @return Bus reference valid for this link's lifetime.
    ivy::Bus& getBus() noexcept { return bus; }

  private:
    struct DispatchState;
    std::shared_ptr<DispatchState> dispatchState;
    IvyLink(boost::asio::io_context *context, const MessageDictionary &dict,
            std::string appName, std::string domain, bool threadedIvy);
    messageCallback_t dispatchCallback(messageCallback_t callback) const;
    const MessageDictionary &dictionary;
    ivy::Bus bus;
    std::mutex subscriptionsMutex;
    std::map<long, ivy::Subscription> subscriptions;
    std::atomic<long> nextBindId{1};
    /// @brief Native subscription and cancellation gate for already posted notifications.
    struct ReceiverSubscription {
      ivy::Subscription token; ///< Owned native binding.
      std::shared_ptr<bool> active; ///< Read/written only under the receiver lock.
    };
    std::map<std::optional<std::string>, ReceiverSubscription> receiverSubscriptions; ///< nullopt selects ALL.
    uint64_t receptionGeneration = 0; ///< Ignore notifications retained from an earlier reception interval.
    // Declared last: stop/join before destroying subscriptions or the bus.
    std::optional<ivy::LoopThread> loop;

    long storeSubscription(ivy::Subscription subscription);
    void storeSubscription(long id, ivy::Bus::BindResult result);
    void bindingsChanged() override;
    ReceiverSubscription bindReceiver(const std::optional<std::string> &name);
  };
}
#endif // PPRZLINKCPP_IVYLINK_H
