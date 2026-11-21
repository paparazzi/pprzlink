// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file Receiver.h
 * @brief Receiver-owned subscriptions and checked message/source filters.
 * @ingroup transports
 */
#pragma once

#include <pprzlink/MessageDictionary.h>
#include <pprzlink/ReceivedMessage.h>
#include <concepts>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace pprzlink {
  /// @brief Selector for all successfully decoded dictionary messages.
  /// @ingroup transports
  struct AllMessages {};
  /// @brief Bind all decoded messages without a string wildcard.
  inline constexpr AllMessages ALL{};

  /// @brief Adapt a one- or two-argument predicate to message and metadata matching.
  /// @ingroup transports
  class ReceivePredicate {
  public:
    /// @brief An empty predicate accepts every message.
    ReceivePredicate() = default;
    /// @brief Store an application predicate with optional metadata access.
    /// @tparam Function Callable returning bool from Message and optionally ReceiveInfo.
    /// @param[in] function Predicate copied or moved into this filter.
    template<class Function>
      requires (!std::same_as<std::remove_cvref_t<Function>, ReceivePredicate>) &&
        (std::is_invocable_r_v<bool, Function, const Message &, const ReceiveInfo &> ||
         std::is_invocable_r_v<bool, Function, const Message &>)
    ReceivePredicate(Function function)
    {
      callback = [function = std::move(function)](const Message &message, const ReceiveInfo &info) mutable {
        if constexpr (std::is_invocable_r_v<bool, Function, const Message &, const ReceiveInfo &>)
          return std::invoke(function, message, info);
        else return std::invoke(function, message);
      };
    }
    /// @brief Evaluate the predicate, or accept when it is empty.
    /// @param[in] message Decoded message borrowed for this invocation.
    /// @param[in] info Matching transport metadata.
    /// @return Whether the message is selected.
    bool operator()(const Message &message, const ReceiveInfo &info) const
    { return !callback || callback(message, info); }
  private:
    std::function<bool(const Message &, const ReceiveInfo &)> callback; ///< Optional application condition.
  };

  /// @brief Criteria combined with AND; absent values impose no restriction.
  /// @ingroup transports
  struct ReceiveFilter {
    std::optional<unsigned int> senderId = std::nullopt; ///< Numeric PPRZLINK sender, in 0..255.
    std::optional<std::string> sender = std::nullopt; ///< Literal textual sender, for Ivy.
    std::optional<unsigned int> receiverId = std::nullopt; ///< Binary destination, in 0..255; 255 selects broadcast.
    std::optional<std::string> className = std::nullopt; ///< Dictionary class name.
    std::optional<unsigned int> classId = std::nullopt; ///< Dictionary class ID, in 0..15.
    std::optional<unsigned int> componentId = std::nullopt; ///< Binary component ID, in 0..15.
    std::optional<UdpEndpoint> udpPeer = std::nullopt; ///< Exact UDP source address and port.
    std::optional<std::string> udpAddress = std::nullopt; ///< Numeric UDP source address, any port.
    std::optional<uint16_t> udpPort = std::nullopt; ///< UDP source port, any address.
    std::optional<uint64_t> xbeeAddress = std::nullopt; ///< Radio address, independent of the PPRZLINK sender.
    std::optional<int> minimumRssi = std::nullopt; ///< Minimum received signal in negative dBm; unavailable RSSI does not match.
    ReceivePredicate where{}; ///< Optional condition on fields and/or metadata.
  };

  /// @brief An asynchronous receive failure with its original exception.
  /// @ingroup transports
  struct ReceiveError {
    /// @brief Distinguish recoverable decoding from terminal channel/initialization failures.
    enum class Kind {
      Decode, ///< Malformed or unknown payload, consumed before notification.
      Io, ///< Terminal operating-system read failure.
      Initialization ///< Failed modem dialogue or protocol deadline.
    };
    Kind kind; ///< Failure category.
    std::exception_ptr exception; ///< Original exception, suitable for std::rethrow_exception.
    std::string message; ///< Human-readable exception description.
  };

  /// @brief Common subscription interface for event-driven message receivers.
  /// Bindings remain active until unbind() or receiver destruction. Callback arguments
  /// are borrowed for the invocation; copy a Message before retaining it or its views.
  /// @ingroup transports
  class Receiver {
  public:
    using BindingId = uint64_t; ///< Receiver-local binding identifier with no destructor side effects.
    using Callback = std::function<void(const Message &, const ReceiveInfo &)>; ///< Borrowed message/metadata observer.
    using ErrorCallback = std::function<void(const ReceiveError &)>; ///< Receive error observer.
    /// @brief Disable callbacks before releasing receiver-owned bindings.
    virtual ~Receiver();
    Receiver(const Receiver &) = delete; ///< Receiver ownership cannot be copied.
    Receiver &operator=(const Receiver &) = delete; ///< Receiver ownership cannot be assigned.
    Receiver(Receiver &&) = delete; ///< A live receiver cannot be moved.
    Receiver &operator=(Receiver &&) = delete; ///< A live receiver cannot be move-assigned.

    /// @brief Bind a named dictionary message with an optional metadata argument.
    /// @tparam Function Observer accepting const Message&, optionally followed by const ReceiveInfo&.
    /// @param[in] name Exact dictionary message name, resolved when binding.
    /// @param[in] function Observer retained by the receiver.
    /// @return ID that can optionally be retained for unbind().
    /// @throws std::exception Unknown message, invalid callback/filter or transport subscription failure.
    template<class Function>
    BindingId bind(std::string_view name, Function function)
    { return bind(name, ReceiveFilter{}, std::move(function)); }

    /// @brief Bind all decoded dictionary messages.
    /// @tparam Function Observer accepting Message and optionally ReceiveInfo.
    /// @param[in] selector ALL selector.
    /// @param[in] function Observer retained by the receiver.
    /// @return Receiver-local binding ID.
    template<class Function>
    BindingId bind(AllMessages selector, Function function)
    { return bind(selector, ReceiveFilter{}, std::move(function)); }

    /// @brief Bind a named message and additional AND-combined criteria.
    /// @tparam Function Observer accepting Message and optionally ReceiveInfo.
    /// @param[in] name Exact dictionary message name.
    /// @param[in] filter Additional checked criteria.
    /// @param[in] function Receiver-owned callback.
    /// @return Receiver-local binding ID.
    /// @throws std::exception Unknown schema, invalid/unsupported filter or transport subscription failure.
    template<class Function>
    BindingId bind(std::string_view name, ReceiveFilter filter, Function function)
    { return addBinding(std::string(name), std::move(filter), adapt(std::move(function))); }

    /// @brief Bind all messages satisfying additional criteria.
    /// @tparam Function Observer accepting Message and optionally ReceiveInfo.
    /// @param[in] selector ALL selector.
    /// @param[in] filter Checked AND-combined criteria.
    /// @param[in] function Receiver-owned callback.
    /// @return Receiver-local binding ID.
    /// @throws std::exception Invalid/unsupported filter or transport subscription failure.
    template<class Function>
    BindingId bind(AllMessages selector, ReceiveFilter filter, Function function)
    { (void)selector; return addBinding(std::nullopt, std::move(filter), adapt(std::move(function))); }

    /// @brief Remove a binding; unknown IDs are harmless, an active invocation can finish.
    /// @param[in] id Receiver-local ID previously returned by bind().
    /// @throws std::exception A transport subscription update fails; the binding is retained.
    void unbind(BindingId id);
    /// @brief Install the receive error handler; empty restores the default policy.
    /// Decode errors are skipped by default. Terminal errors propagate to the event loop.
    /// Application callback/predicate exceptions always propagate to the event loop.
    /// @param[in] callback Observer copied or moved into the receiver.
    void onError(ErrorCallback callback);
    /// @brief Start asynchronous reception; repeated starts are harmless.
    virtual void start() = 0;
    /// @brief Stop this receiver without stopping its shared event loop; preserve bindings.
    virtual void stop() = 0;
    /// @brief Inspect whether reception is active.
    /// @return True after start() until stop() or a terminal receive error.
    bool isRunning() const;

  protected:
    /// @brief Source kind used to reject unavailable metadata filters at bind time.
    enum class Kind {
      Stream, ///< Binary PPRZLINK byte stream without remote-address metadata.
      Udp, ///< Binary datagrams with an actual UDP source endpoint.
      Xbee, ///< Binary radio frames with radio source and optional RSSI.
      Ivy ///< Text bus messages with a literal sender and no binary routing header.
    };
    /// @brief Shared callback lifetime and serialization gate.
    struct State;
    std::shared_ptr<State> receiverState; ///< State retained by guarded asynchronous handlers.
    /// @brief Construct a registry borrowing immutable dictionary schemas.
    /// @param[in] dictionary Dictionary that must outlive this receiver.
    /// @param[in] kind Source kind determining available metadata.
    Receiver(const MessageDictionary &dictionary, Kind kind);
    /// @brief Distinct bound names, or only nullopt when any ALL binding exists.
    /// @return Empty for no bindings; filters remain the dispatcher's responsibility.
    std::set<std::optional<std::string>> boundMessageNames() const;
    /// @brief Update transport subscriptions after a registry change, under the receiver lock.
    /// Throwing must leave transport subscriptions unchanged; the registry change is rolled back.
    virtual void bindingsChanged() {}
    /// @brief Serialize receiver operations and callbacks, allowing callback-side stop/unbind.
    /// @return A lock on this receiver's recursive lifetime gate.
    std::unique_lock<std::recursive_mutex> lockReceiver() const;
    /// @brief Mark reception active if it was stopped.
    /// @return Whether activation changed the running state.
    bool activate();
    /// @brief Mark reception stopped before canceling source operations.
    void deactivate();
    /// @brief Permanently disable late callbacks and release application captures.
    void shutdown();
    /// @brief Invoke every matching binding in registration order.
    /// @param[in] message Decoded message borrowed throughout dispatch.
    /// @param[in] info Metadata belonging to that same message.
    void deliver(const Message &message, const ReceiveInfo &info);
    /// @brief Notify a receive failure; unhandled terminal errors are rethrown.
    /// @param[in] exception Original receive exception.
    /// @param[in] kind Recoverable decode or terminal I/O/initialization category.
    void reportError(std::exception_ptr exception, ReceiveError::Kind kind);
    /// @brief Guard a late asynchronous notification against receiver destruction/stop.
    /// @tparam Function Callable notification with arbitrary completion arguments.
    /// @param[in] callback Notification borrowing its receiver only while invoked.
    /// @return Callback serialized through shared lifetime state.
    template<class Function>
    auto guarded(Function callback) const
    {
      return [weak = std::weak_ptr(receiverState), callback = std::move(callback)](auto &&...arguments) mutable {
        invokeGuarded(weak, [&] { std::invoke(callback, std::forward<decltype(arguments)>(arguments)...); });
      };
    }

  private:
    static void invokeGuarded(const std::weak_ptr<State> &weak, const std::function<void()> &callback);
    BindingId addBinding(std::optional<std::string> name, ReceiveFilter filter, Callback callback);
    template<class Function>
    static Callback adapt(Function function)
    {
      static_assert(std::is_invocable_v<Function, const Message &, const ReceiveInfo &> ||
                    std::is_invocable_v<Function, const Message &>,
                    "bind callback must accept const Message& and optionally const ReceiveInfo&");
      if constexpr (std::is_pointer_v<Function>) {
        if (function == nullptr) throw std::invalid_argument("bind requires a nonempty callback");
      } else if constexpr (requires { function.operator bool(); }) {
        if (!function.operator bool()) throw std::invalid_argument("bind requires a nonempty callback");
      }
      return [function = std::move(function)](const Message &message, const ReceiveInfo &info) mutable {
        if constexpr (std::is_invocable_v<Function, const Message &, const ReceiveInfo &>)
          std::invoke(function, message, info);
        else std::invoke(function, message);
      };
    }
  };
}
