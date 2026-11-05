// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file PosixFileDevice.h
 * @brief Asynchronous file and FIFO byte-stream adapter.
 * @ingroup transports
 *
 * The owned descriptor is nonblocking and is opened for reading and writing without UART configuration.
 */

#pragma once
#include <pprzlink/Device.h>
#include <boost/asio/io_context.hpp>
#include <memory>
#include <string>

namespace pprzlink {
  /// Own an O_RDWR file/FIFO descriptor without applying serial-port settings.
  /// Run the supplied Asio context. EOF and errors follow already buffered input.
  /// @ingroup transports
  class PosixFileDevice final : public Device {
  public:
    /// @brief Open an owned O_RDWR, O_NONBLOCK, O_CLOEXEC descriptor.
    /// @param[in] context Execution context that must outlive the device.
    /// @param[in] path File, FIFO or pseudo-terminal path; no serial options are applied.
    /// @throws std::system_error Opening the descriptor fails.
    /// @throws boost::system::system_error Assigning the descriptor to Asio fails.
    PosixFileDevice(boost::asio::io_context &context, const std::string &path);
    /// @brief Close the owned descriptor.
    ~PosixFileDevice() override;
    /// @brief Copying an exclusively owned descriptor is prohibited.
    PosixFileDevice(const PosixFileDevice &) = delete;
    /// @brief Copy assignment is prohibited.
    /// @return This operation is deleted and cannot return.
    PosixFileDevice &operator=(const PosixFileDevice &) = delete;
    /// @brief Inspect completed descriptor input without consuming it.
    /// @return Number of bytes ready in the internal input buffer.
    /// @throws boost::system::system_error EOF or a read failure, after buffered data is exhausted.
    size_t availableBytes() override;
    /// @brief Drain input completed by asynchronous descriptor reads.
    /// @return Owned currently available input; empty when no new input is ready.
    /// @throws boost::system::system_error Deferred EOF or read failure after buffered data is exhausted.
    BytesBuffer readAll() override;
    /// @brief Complete a descriptor write, waiting up to one second per blocked segment.
    /// @param[in] data Bytes delivered in order; a failure may follow a partial external write.
    /// @throws std::system_error Write or readiness polling fails.
    /// @throws std::runtime_error Readiness times out or the descriptor cannot complete a write.
    void writeBuffer(const BytesBuffer &data) override;

    /// @brief Set the observer invoked outside descriptor locks when input/errors arrive.
    /// @param[in] callback Readiness observer retained by this device.
    void setReceiveCallback(ReceiveCallback callback) override;
    /// @brief Arm asynchronous descriptor reads on the supplied event loop.
    /// @throws boost::system::system_error Deferred EOF or read failure after buffered data is exhausted.
    void startReception() override;
    /// @brief Cancel reads without closing the descriptor.
    void stopReception() override;
    /// @brief Return the descriptor's event-loop executor.
    /// @return Executor borrowing the supplied io_context.
    boost::asio::any_io_executor getExecutor() override;

  private:
    struct State;
    std::shared_ptr<State> state; ///< Descriptor and buffers retained until canceled reads complete.
  };
}
