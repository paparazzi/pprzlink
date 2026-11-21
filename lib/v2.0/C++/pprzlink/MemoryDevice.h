// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file MemoryDevice.h
 * @brief Event-driven in-memory byte stream for examples, loopback and simulations.
 * @ingroup transports
 */
#pragma once
#include <pprzlink/Device.h>
#include <boost/asio/io_context.hpp>
#include <memory>
#include <span>

namespace pprzlink {
  /// An asynchronous memory loopback: writes become input on the supplied context.
  /// feed() can inject complete or fragmented wire data without a socket or hardware.
  /// No thread is created. The supplied context must outlive pending notifications.
  /// @ingroup transports
  class MemoryDevice final : public Device {
  public:
    /// @brief Create an empty memory byte stream on a caller-owned event loop.
    /// @param[in] context Event loop used for readiness callbacks.
    explicit MemoryDevice(boost::asio::io_context &context);
    /// @brief Disable observers; queued notifications retain their own buffer state.
    ~MemoryDevice() override;
    MemoryDevice(const MemoryDevice &) = delete; ///< Copying a live stream is prohibited.
    MemoryDevice &operator=(const MemoryDevice &) = delete; ///< Assigning stream ownership is prohibited.
    MemoryDevice(MemoryDevice &&) = delete; ///< Moving a live stream is prohibited.
    MemoryDevice &operator=(MemoryDevice &&) = delete; ///< Move assignment is prohibited.
    /// @brief Append incoming bytes and schedule readiness if reception is active.
    /// @param[in] bytes Bytes copied into the input stream in their original order.
    void feed(std::span<const uint8_t> bytes);
    /// @brief Replace the readiness observer; it is always called outside the buffer lock.
    /// @param[in] callback Observer retained until replacement or destruction.
    void setReceiveCallback(ReceiveCallback callback) override;
    /// @brief Enable notifications, including for already buffered input; idempotent.
    void startReception() override;
    /// @brief Disable queued notifications while preserving input for a restart.
    void stopReception() override;
    /// @brief Return the caller-owned context's executor.
    /// @return Executor used to post memory readiness notifications.
    boost::asio::any_io_executor getExecutor() override;
    /// @brief Inspect buffered input without advancing the context.
    /// @return Number of bytes currently available to readAll().
    size_t availableBytes() override;
    /// @brief Drain the input stream without waiting or scheduling reads.
    /// @return Owned copy of all bytes removed from the buffer.
    BytesBuffer readAll() override;
    /// @brief Echo an entire write into the incoming memory stream.
    /// @param[in] bytes Complete bytes copied, as with feed().
    void writeBuffer(const BytesBuffer &bytes) override;
  private:
    struct State;
    std::shared_ptr<State> state; ///< Input and observer state retained by posted notifications.
  };
}
