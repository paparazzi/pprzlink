// SPDX-License-Identifier: LGPL-3.0-or-later
/** @file RequestReply.h
 * @brief Transport-independent correlated request/answer capability.
 */
#pragma once
#include <pprzlink/ReceivedMessage.h>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pprzlink {
  /** Optional capability implemented by transports with request correlation.
   * @ingroup transports
   * Registrations are owned by the implementation, not by the returned integer IDs.
   * Callback arguments are borrowed; copy them to retain them. Callbacks execute on
   * the transport's application executor. Exceptions propagate to that event loop.
   * No timeout is imposed. Cancellation suppresses queued callbacks; an invocation
   * already executing may finish. Unknown/completed IDs are harmless. Closing or
   * destroying the transport cancels requests and answerers; Receiver::stop() does not.
   */
  class RequestReply {
  public:
    using RequestId = uint64_t; ///< Local pending request identifier.
    using HandlerId = uint64_t; ///< Local persistent answerer identifier.
    using ReplyCallback = std::function<void(const Message &, const ReceiveInfo &)>; ///< Borrowed answer observer.
    using AnswerCallback = std::function<Message(const Message &, const ReceiveInfo &)>; ///< Returns the answer to send.
    /// Release the capability after the implementation has canceled its registrations.
    virtual ~RequestReply() = default;

    /// Send a request and observe its first correlated answer. Metadata is optional.
    /// Synchronous validation/registration/send failures throw and leave no pending request.
    /// @tparam Function Observer accepting a borrowed answer and optionally metadata.
    /// @param[in] request Populated request message.
    /// @param[in] callback Observer retained until completion or cancellation.
    /// @return Local request ID accepted by cancelRequest().
    template<class Function>
    RequestId sendRequest(const Message &request, Function callback)
    { return sendRequestImpl(request, adapt<void>(std::move(callback))); }

    /// Retain an answerer for a named request schema. The transport correlates and routes its returned answer.
    /// The callback may optionally accept ReceiveInfo. Invalid schemas/callbacks or registration failures throw.
    /// @tparam Function Handler returning a Message from a borrowed request and optionally metadata.
    /// @param[in] name Exact dictionary name of the request schema.
    /// @param[in] callback Persistent answerer retained by the implementation.
    /// @return Local handler ID accepted by unbindRequest().
    template<class Function>
    HandlerId bindRequest(std::string_view name, Function callback)
    { return bindRequestImpl(name, adapt<Message>(std::move(callback))); }

    /// Cancel a pending request, including an answer queued for delivery.
    /// @param[in] id Local pending request ID; unknown IDs are ignored.
    virtual void cancelRequest(RequestId id) = 0;
    /// Remove a persistent answerer, suppressing queued invocations.
    /// @param[in] id Local handler ID; unknown IDs are ignored.
    virtual void unbindRequest(HandlerId id) = 0;

  protected:
    /// Transport-specific send, correlation and one-shot completion.
    /// @param[in] request Populated request message.
    /// @param[in] callback Validated observer accepting message and metadata.
    /// @return Local request ID.
    virtual RequestId sendRequestImpl(const Message &request, ReplyCallback callback) = 0;
    /// Transport-specific request decoding and automatic correlated replies.
    /// @param[in] name Request schema name.
    /// @param[in] callback Validated answerer accepting message and metadata.
    /// @return Local handler ID.
    virtual HandlerId bindRequestImpl(std::string_view name, AnswerCallback callback) = 0;

  private:
    template<class Result, class Function>
    static std::function<Result(const Message &, const ReceiveInfo &)> adapt(Function function)
    {
      static_assert(std::is_invocable_r_v<Result, Function, const Message &, const ReceiveInfo &> ||
                    std::is_invocable_r_v<Result, Function, const Message &>,
                    "Request callback must accept const Message& and optionally const ReceiveInfo&");
      if constexpr (std::is_pointer_v<Function>) {
        if (function == nullptr) throw std::invalid_argument("Request callback is empty");
      } else if constexpr (requires { function.operator bool(); }) {
        if (!function.operator bool()) throw std::invalid_argument("Request callback is empty");
      }
      return [function = std::move(function)](const Message &message, const ReceiveInfo &info) mutable -> Result {
        auto invoke = [&]() -> decltype(auto) {
          if constexpr (std::is_invocable_r_v<Result, Function, const Message &, const ReceiveInfo &>)
            return std::invoke(function, message, info);
          else return std::invoke(function, message);
        };
        if constexpr (std::is_void_v<Result>) invoke();
        else return invoke();
      };
    }
  };
}
