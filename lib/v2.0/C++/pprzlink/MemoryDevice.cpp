// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file MemoryDevice.cpp
 * @brief Coalesced asynchronous memory notifications with restart-safe lifetime.
 * @ingroup transports
 */
#include "MemoryDevice.h"
#include <boost/asio/post.hpp>
#include <mutex>
#include <utility>

namespace pprzlink {
  /// @brief Shared stream state retained until queued readiness notifications finish.
  /// @ingroup internals
  struct MemoryDevice::State : std::enable_shared_from_this<State> {
    /// @brief Borrow the executor of a caller-owned context.
    /// @param[in] context Event loop that outlives this state.
    explicit State(boost::asio::io_context &context) : executor(context.get_executor()) {}
    boost::asio::any_io_executor executor; ///< Immutable executor for notifications.
    std::mutex mutex; ///< Protects input, observer and notification generations.
    BytesBuffer input; ///< Buffered incoming bytes, including looped-back writes.
    ReceiveCallback callback; ///< Readiness observer invoked after releasing mutex.
    bool receiving = false; ///< Whether input should generate readiness events.
    bool scheduled = false; ///< Prevents redundant notifications for one input batch.
    bool alive = true; ///< False before the wrapper releases observer captures.
    uint64_t generation = 0; ///< Invalidates notifications from an earlier stop/start session.

    /// @brief Post one eligible readiness notification while the caller holds mutex.
    void schedule()
    {
      if (!alive || !receiving || scheduled || input.empty() || !callback) return;
      scheduled = true;
      boost::asio::post(executor, [self = shared_from_this(), epoch = generation] {
        ReceiveCallback observer;
        {
          std::lock_guard lock(self->mutex);
          self->scheduled = false;
          if (self->alive && self->receiving && epoch == self->generation && !self->input.empty())
            observer = self->callback;
          else self->schedule(); // Re-arm once after a stop/start invalidated the old event.
        }
        if (observer) observer();
      });
    }
  };

  MemoryDevice::MemoryDevice(boost::asio::io_context &context) : state(std::make_shared<State>(context)) {}
  MemoryDevice::~MemoryDevice()
  {
    std::lock_guard lock(state->mutex);
    state->alive = state->receiving = false;
    state->callback = {};
  }
  void MemoryDevice::feed(std::span<const uint8_t> bytes)
  {
    std::lock_guard lock(state->mutex);
    state->input.insert(state->input.end(), bytes.begin(), bytes.end());
    state->schedule();
  }
  void MemoryDevice::writeBuffer(const BytesBuffer &bytes) { feed(bytes); }
  void MemoryDevice::setReceiveCallback(ReceiveCallback callback)
  { std::lock_guard lock(state->mutex); state->callback = std::move(callback); state->schedule(); }
  void MemoryDevice::startReception()
  {
    std::lock_guard lock(state->mutex);
    if (!state->receiving) { state->receiving = true; ++state->generation; }
    state->schedule();
  }
  void MemoryDevice::stopReception()
  { std::lock_guard lock(state->mutex); state->receiving = false; ++state->generation; }
  boost::asio::any_io_executor MemoryDevice::getExecutor() { return state->executor; }
  size_t MemoryDevice::availableBytes() { std::lock_guard lock(state->mutex); return state->input.size(); }
  BytesBuffer MemoryDevice::readAll()
  { std::lock_guard lock(state->mutex); return std::exchange(state->input, {}); }
}
