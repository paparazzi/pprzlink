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
 * @brief Independent Ivy buses, Receiver bindings and request helpers.
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
#include <pprzlink/RequestReply.h>
#include <boost/asio/io_context.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace pprzlink {

  /** Owns an independent Ivy bus. The dictionary must outlive the link.
   * @ingroup ivy
   * Use the inherited Receiver::bind() and Receiver::unbind() for ordinary messages.
   * Sends and subscription operations may be called from multiple threads.
   * Without an Asio context, callbacks run on the Ivy loop. With a context,
   * Receiver callbacks/predicates/error handlers, request reply callbacks and
   * answerers are posted there and serialized per link. Native getBus() callbacks
   * still run on Ivy. The context and dictionary must outlive the link.
   * In Asio mode, close() suppresses queued callbacks and waits for active
   * callbacks on other threads; no callback starts after it returns. Canceling a request
   * suppresses queued delivery; an already executing invocation may finish. Queued captures may survive until the
   * context drains or is destroyed, but cannot access a closed/destroyed link.
   * Receiver decode failures reach onError(); application callback/reply errors propagate
   * from context.run(). Native request decode errors remain available through
   * getBus().take_callback_error(). No Asio thread is
   * owned; service the context to receive callbacks. Do not synchronously wait
   * for replies from its callbacks. A work guard keeps context.run() running until close().
   * Destroy the link after external callers/loops have finished, never from a callback.
   */
  class IvyLink : public Receiver, public RequestReply {
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

    /// Cancel a pending request, suppressing queued reply delivery. Unknown IDs are harmless.
    void cancelRequest(RequestId id) override;
    /// Remove a persistent answerer, suppressing queued invocations. Unknown IDs are harmless.
    void unbindRequest(HandlerId id) override;
    /// @brief Publish an ordinary populated message on this bus.
    /// @param[in] msg Non-request message to serialize in standard Ivy syntax.
    /// @throws message_is_request The schema ends with _REQ; use sendRequest().
    /// @throws std::exception Field formatting or native sending fails.
    void sendMessage(const Message &msg);
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
    /// Request subscriptions, native getBus() subscriptions, sends and bus timers remain active.
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
    const MessageDictionary &dictionary;
    ivy::Bus bus;
    /// Owned request registration and gate for already queued application work.
    struct RequestSubscription {
      ivy::Subscription token; ///< Native registration.
      std::shared_ptr<bool> active; ///< Protected by DispatchState::mutex.
    };
    std::map<RequestId, RequestSubscription> pendingRequests;
    std::map<HandlerId, RequestSubscription> requestHandlers;
    uint64_t nextRequestId = 1; ///< Shared local ID sequence, protected by DispatchState::mutex.
    /// @brief Native subscription and cancellation gate for already posted notifications.
    struct ReceiverSubscription {
      ivy::Subscription token; ///< Owned native binding.
      std::shared_ptr<bool> active; ///< Read/written only under the receiver lock.
    };
    std::map<std::optional<std::string>, ReceiverSubscription> receiverSubscriptions; ///< nullopt selects ALL.
    uint64_t receptionGeneration = 0; ///< Ignore notifications retained from an earlier reception interval.
    // Declared last: stop/join before destroying subscriptions or the bus.
    std::optional<ivy::LoopThread> loop;

    RequestId sendRequestImpl(const Message &request, ReplyCallback callback) override;
    HandlerId bindRequestImpl(std::string_view name, AnswerCallback callback) override;
    void bindingsChanged() override;
    ReceiverSubscription bindReceiver(const std::optional<std::string> &name);
  };
}
#endif // PPRZLINKCPP_IVYLINK_H
