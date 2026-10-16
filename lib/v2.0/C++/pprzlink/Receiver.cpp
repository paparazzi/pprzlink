// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file Receiver.cpp
 * @brief Lifetime-safe ordered callback distribution and source filter validation.
 * @ingroup transports
 */
#include "Receiver.h"
#include <boost/asio/ip/address.hpp>
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace pprzlink {
  /// @brief Shared registry and gate retaining callback state through cancellation.
  /// @ingroup internals
  struct Receiver::State {
    /// @brief One receiver-owned binding, kept alive while a dispatch snapshot uses it.
    struct Binding {
      BindingId id; ///< Receiver-local cancellation identifier.
      std::optional<std::pair<int, int>> messageId; ///< Resolved class/message selector, absent for ALL.
      ReceiveFilter filter; ///< Validated additional selection criteria.
      Callback callback; ///< Application observer invoked after matching.
      bool active = true; ///< Cleared on unbind even inside an active dispatch snapshot.
    };
    /// @brief Borrow schemas and remember the source's metadata capabilities.
    /// @param[in] dictionary Immutable schemas that outlive the receiver.
    /// @param[in] kind Source metadata family.
    State(const MessageDictionary &dictionary, Kind kind) : dictionary(dictionary), kind(kind) {}
    const MessageDictionary &dictionary; ///< Borrowed immutable schema catalogue.
    Kind kind; ///< Supported source filter family.
    mutable std::recursive_mutex mutex; ///< Serializes callbacks and allows callback-side control operations.
    bool alive = true; ///< False before any derived decoder state is released.
    bool running = false; ///< Whether event delivery is enabled.
    BindingId nextId = 1; ///< Next receiver-local binding identifier.
    std::vector<std::shared_ptr<Binding>> bindings; ///< Active bindings in registration order.
    ErrorCallback errorCallback; ///< Optional receive failure observer.
  };

  Receiver::Receiver(const MessageDictionary &dictionary, Kind kind)
    : receiverState(std::make_shared<State>(dictionary, kind)) {}
  Receiver::~Receiver() { shutdown(); }
  std::unique_lock<std::recursive_mutex> Receiver::lockReceiver() const
  { return std::unique_lock(receiverState->mutex); }
  bool Receiver::activate()
  {
    auto lock = lockReceiver();
    if (!receiverState->alive || receiverState->running) return false;
    receiverState->running = true;
    return true;
  }
  void Receiver::deactivate() { auto lock = lockReceiver(); receiverState->running = false; }
  bool Receiver::isRunning() const { auto lock = lockReceiver(); return receiverState->running; }
  void Receiver::shutdown()
  {
    auto lock = lockReceiver();
    receiverState->alive = receiverState->running = false;
    for (auto &binding : receiverState->bindings) binding->active = false;
    receiverState->bindings.clear();
    receiverState->errorCallback = {};
  }
  void Receiver::invokeGuarded(const std::weak_ptr<State> &weak, const std::function<void()> &callback)
  {
    const auto state = weak.lock();
    if (!state) return;
    std::lock_guard lock(state->mutex);
    if (state->alive && state->running) callback();
  }
  Receiver::BindingId Receiver::addBinding(std::optional<std::string> name, ReceiveFilter filter, Callback callback)
  {
    auto lock = lockReceiver();
    const auto requireRange = [](const std::optional<unsigned int> &value, unsigned int maximum) {
      if (value && *value > maximum) throw std::invalid_argument("Receive filter identifier out of range");
    };
    requireRange(filter.senderId, 255); requireRange(filter.receiverId, 255);
    requireRange(filter.classId, 15); requireRange(filter.componentId, 15);
    if (filter.className) {
      const auto id = static_cast<unsigned int>(receiverState->dictionary.getClassId(*filter.className));
      if (filter.classId && *filter.classId != id) throw std::invalid_argument("Conflicting class filters");
      filter.classId = id;
    }
    if ((filter.udpPeer || filter.udpAddress || filter.udpPort) && receiverState->kind != Kind::Udp)
      throw std::invalid_argument("UDP filters require a UDP receiver");
    if ((filter.xbeeAddress || filter.minimumRssi) && receiverState->kind != Kind::Xbee)
      throw std::invalid_argument("Radio filters require an XBee receiver");
    if ((filter.receiverId || filter.componentId) && receiverState->kind == Kind::Ivy)
      throw std::invalid_argument("Ivy does not transmit binary receiver/component IDs");
    if (filter.sender && receiverState->kind != Kind::Ivy)
      throw std::invalid_argument("Text sender filters require an Ivy receiver");
    if (filter.minimumRssi && (*filter.minimumRssi > 0 || *filter.minimumRssi < -255))
      throw std::invalid_argument("RSSI filter must be in -255..0 dBm");
    if (filter.udpAddress) *filter.udpAddress = boost::asio::ip::make_address(*filter.udpAddress).to_string();
    if (filter.udpPeer) filter.udpPeer->address = boost::asio::ip::make_address(filter.udpPeer->address).to_string();
    if (filter.udpPeer && ((filter.udpAddress && *filter.udpAddress != filter.udpPeer->address) ||
                          (filter.udpPort && *filter.udpPort != filter.udpPeer->port)))
      throw std::invalid_argument("Conflicting UDP source filters");
    auto binding = std::make_shared<State::Binding>();
    binding->id = receiverState->nextId++;
    if (name) binding->messageId = receiverState->dictionary.getMessageId(*name);
    binding->filter = std::move(filter);
    binding->callback = std::move(callback);
    receiverState->bindings.push_back(binding);
    return binding->id;
  }
  void Receiver::unbind(BindingId id)
  {
    auto lock = lockReceiver();
    std::erase_if(receiverState->bindings, [id](const auto &binding) {
      if (binding->id != id) return false;
      binding->active = false;
      return true;
    });
  }
  void Receiver::onError(ErrorCallback callback)
  { auto lock = lockReceiver(); receiverState->errorCallback = std::move(callback); }
  void Receiver::reportError(std::exception_ptr exception, ReceiveError::Kind kind)
  {
    auto lock = lockReceiver();
    if (!receiverState->errorCallback && kind == ReceiveError::Kind::Decode) return;
    std::string description = "Unknown receive failure";
    try { std::rethrow_exception(exception); }
    catch (const std::exception &error) { description = error.what(); }
    catch (...) {}
    auto callback = receiverState->errorCallback;
    if (callback) callback(ReceiveError{kind, exception, std::move(description)});
    else if (kind != ReceiveError::Kind::Decode) std::rethrow_exception(exception);
  }
  void Receiver::deliver(const Message &message, const ReceiveInfo &info)
  {
    auto lock = lockReceiver();
    const auto bindings = receiverState->bindings; // Newly added bindings begin with the next message.
    const auto messageId = std::pair<int, int>{message.getClassId(), message.getDefinition().getId()};
    for (const auto &binding : bindings) {
      if (!receiverState->running || !binding->active) continue;
      const auto &filter = binding->filter;
      if (binding->messageId && *binding->messageId != messageId) continue;
      if (filter.classId && *filter.classId != message.getClassId()) continue;
      if (filter.receiverId && *filter.receiverId != message.getReceiverId()) continue;
      if (filter.componentId && *filter.componentId != message.getComponentId()) continue;
      const auto &sender = message.getSenderId();
      if (filter.sender && (!std::holds_alternative<std::string>(sender) ||
                            std::get<std::string>(sender) != *filter.sender)) continue;
      if (filter.senderId) {
        unsigned int id = 256;
        if (const auto numeric = std::get_if<uint8_t>(&sender)) id = *numeric;
        else {
          const auto &text = std::get<std::string>(sender);
          const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), id);
          if (error != std::errc{} || end != text.data() + text.size()) continue;
        }
        if (id != *filter.senderId) continue;
      }
      if (filter.udpPeer && (!info.udpPeer || *info.udpPeer != *filter.udpPeer)) continue;
      if (filter.udpAddress && (!info.udpPeer || info.udpPeer->address != *filter.udpAddress)) continue;
      if (filter.udpPort && (!info.udpPeer || info.udpPeer->port != *filter.udpPort)) continue;
      if (filter.xbeeAddress && (!info.xbee || info.xbee->sourceAddress != *filter.xbeeAddress)) continue;
      if (filter.minimumRssi && (!info.xbee || !info.xbee->hasRssi ||
                                -static_cast<int>(info.xbee->rssi) < *filter.minimumRssi)) continue;
      if (!filter.where(message, info)) continue;
      // Predicates may stop reception or remove their own binding.
      if (!receiverState->running || !binding->active) continue;
      binding->callback(message, info);
    }
  }
}
