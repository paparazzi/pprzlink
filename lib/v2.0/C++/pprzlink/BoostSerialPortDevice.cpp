/*
 * Copyright 2020 garciafa
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
 * @file BoostSerialPortDevice.cpp
 * @brief Serial reception, cancellation and baud-rate changes.
 * @ingroup transports
 *
 * Completion handlers retain State rather than the wrapper. Input generations discard callbacks from a previous baud rate and allow cancellation-safe restart.
 */

#include "BoostSerialPortDevice.h"
#include <boost/asio/write.hpp>
#include <boost/asio/post.hpp>
#include <array>
#include <cerrno>
#include <mutex>
#include <stdexcept>
#include <utility>
#if !defined(BOOST_ASIO_WINDOWS) && !defined(__CYGWIN__)
#include <sys/ioctl.h>
#endif

namespace pprzlink {
  /// @brief Shared serial state retained by pending handlers after wrapper destruction.
  /// @ingroup internals
  /// All option, buffer and completion operations use mutex. At most one read
  /// is pending; inputGeneration distinguishes obsolete baud-rate completions.
  struct BoostSerialPortDevice::State : std::enable_shared_from_this<State> {
    /// @brief Open a serial port without retaining the wrapper object.
    /// @param[in] context Caller-owned Asio context that must outlive this state.
    /// @param[in] name Native serial/PTY device name.
    /// @throws boost::system::system_error Opening/configuring the native port fails.
    State(boost::asio::io_context &context, const std::string &name) : port(context, name) {}

    std::mutex mutex; ///< Serializes options, input state and completion callbacks.
    boost::asio::serial_port port; ///< Owned port borrowing the caller's context.
    std::array<uint8_t, 1024> readBuffer; ///< Storage retained until a pending read completes.
    BytesBuffer received; ///< Completed input not yet drained by readAll().
    ReceiveCallback callback; ///< Readiness observer invoked outside mutex.
    boost::system::error_code receiveError; ///< Deferred terminal receive failure.
    bool receiving = false; ///< Whether a completion should schedule another read.
    bool readPending = false; ///< Prevents overlapping reads using the same buffer.
    uint64_t inputGeneration = 0; ///< Epoch used to reject old-rate/cancelled input.

    /// @brief Report a previously latched receive failure.
    /// @throws boost::system::system_error The stored receive error is nonzero.
    void checkReceiveError() const
    {
      if (receiveError) throw boost::system::system_error(receiveError, "Serial reception");
    }

    /// @brief Schedule at most one read, retaining shared state in its callback.
    /// @pre The caller holds mutex and the borrowed context is alive.
    void startRead()
    {
      if (!receiving || readPending || receiveError) return;
      port.async_read_some(boost::asio::buffer(readBuffer),
        [self = shared_from_this(), generation = inputGeneration](const boost::system::error_code &error, size_t size) {
          self->finishRead(error, size, generation);
        });
      readPending = true;
    }

    /// @brief Commit a current-generation completion and schedule the next eligible read.
    /// @param[in] error Completion result; cancellation is not latched as a receive failure.
    /// @param[in] size Bytes completed into readBuffer.
    /// @param[in] generation Input epoch captured when the operation was started.
    void finishRead(const boost::system::error_code &error, size_t size, uint64_t generation)
    {
      ReceiveCallback observer;
      {
        std::lock_guard lock(mutex);
        readPending = false;
        if (generation == inputGeneration) {
          received.insert(received.end(), readBuffer.begin(), readBuffer.begin() + size);
          if (error && error != boost::asio::error::operation_aborted) {
            receiveError = error;
            receiving = false;
          }
          if (receiving || receiveError) observer = callback;
        }
        startRead(); // Includes a restart requested while cancellation was pending.
      }
      if (observer) {
        observer();
        // Surface a terminal error after delivering bytes completed in that same read.
        if (size && error && error != boost::asio::error::operation_aborted) observer();
      }
    }

    /// @brief Read one native serial option while holding the state lock.
    /// @tparam Option Default-constructible Asio serial option type.
    /// @return Current port option.
    /// @throws boost::system::system_error The native option query fails.
    template<class Option>
    Option getOption()
    {
      std::lock_guard lock(mutex);
      Option value;
      port.get_option(value);
      return value;
    }

    /// @brief Set one native serial option while holding the state lock.
    /// @tparam Option Asio serial option type.
    /// @param[in] value New option value.
    /// @throws boost::system::system_error The native option update fails.
    template<class Option>
    void setOption(const Option &value)
    {
      std::lock_guard lock(mutex);
      port.set_option(value);
    }
  };

  BoostSerialPortDevice::BoostSerialPortDevice(boost::asio::io_context &context, std::string name)
    : state(std::make_shared<State>(context, name)) {}

  BoostSerialPortDevice::~BoostSerialPortDevice()
  {
    std::lock_guard lock(state->mutex);
    state->receiving = false;
    boost::system::error_code ignored;
    state->port.close(ignored);
    // Canceled handlers own State, never this device. They can safely finish later.
  }

  size_t BoostSerialPortDevice::availableBytes()
  {
    std::lock_guard lock(state->mutex);
    if (state->received.empty()) state->checkReceiveError();
    return state->received.size();
  }

  BytesBuffer BoostSerialPortDevice::readAll()
  {
    std::lock_guard lock(state->mutex);
    if (state->received.empty()) state->checkReceiveError();
    state->receiving = !state->receiveError;
    state->startRead();
    return std::exchange(state->received, {});
  }

  void BoostSerialPortDevice::writeBuffer(const BytesBuffer &data)
  {
    std::lock_guard lock(state->mutex);
    boost::asio::write(state->port, boost::asio::buffer(data));
  }

  void BoostSerialPortDevice::resetBaudrate(unsigned int baudrate)
  {
    if (baudrate == 0) throw std::invalid_argument("Serial baud rate must be positive");
    std::lock_guard lock(state->mutex);
    state->checkReceiveError();
    state->port.cancel();
    ++state->inputGeneration; // Canceled completions must not reinsert old-rate bytes.
    state->received.clear();
    state->port.set_option(Baudrate(baudrate));
    state->receiving = true;
    state->startRead(); // Waits for a pending cancellation before reusing readBuffer.
  }

  void BoostSerialPortDevice::startReception()
  {
    std::lock_guard lock(state->mutex);
    state->checkReceiveError();
    state->receiving = true;
    state->startRead();
    if (!state->received.empty() && state->callback) {
      boost::asio::post(state->port.get_executor(), [state = state] {
        ReceiveCallback observer;
        { std::lock_guard lock(state->mutex); if (state->receiving) observer = state->callback; }
        if (observer) observer();
      });
    }
  }

  void BoostSerialPortDevice::stopReception()
  {
    std::lock_guard lock(state->mutex);
    state->receiving = false;
    state->port.cancel();
  }

  void BoostSerialPortDevice::setReceiveCallback(ReceiveCallback callback)
  {
    std::lock_guard lock(state->mutex);
    state->callback = std::move(callback);
  }

  boost::asio::any_io_executor BoostSerialPortDevice::getExecutor()
  {
    return state->port.get_executor();
  }

  BoostSerialPortDevice::Baudrate BoostSerialPortDevice::getBaudrate() const
  {
    return state->getOption<Baudrate>();
  }

  void BoostSerialPortDevice::setBaudrate(const Baudrate &value)
  {
    state->setOption(value);
  }

  BoostSerialPortDevice::DataBits BoostSerialPortDevice::getDataBits() const
  {
    return state->getOption<DataBits>();
  }

  void BoostSerialPortDevice::setDataBits(const DataBits &value)
  {
    state->setOption(value);
  }

  BoostSerialPortDevice::Parity BoostSerialPortDevice::getParity() const
  {
    return state->getOption<Parity>();
  }

  void BoostSerialPortDevice::setParity(const Parity &value)
  {
    state->setOption(value);
  }

  BoostSerialPortDevice::StopBits BoostSerialPortDevice::getStopBits() const
  {
    return state->getOption<StopBits>();
  }

  void BoostSerialPortDevice::setStopBits(const StopBits &value)
  {
    state->setOption(value);
  }

  BoostSerialPortDevice::Flowcontrol BoostSerialPortDevice::getFlowcontrol() const
  {
    return state->getOption<Flowcontrol>();
  }

  bool BoostSerialPortDevice::getCTS() const
  {
    std::lock_guard lock(state->mutex);
#if !defined(BOOST_ASIO_WINDOWS) && !defined(__CYGWIN__) && defined(TIOCMGET) && defined(TIOCM_CTS)
    int bits = 0;
    if (::ioctl(state->port.native_handle(), TIOCMGET, &bits) < 0) {
      const boost::system::error_code error(errno, boost::system::system_category());
      throw boost::system::system_error(error, "Serial CTS status (TIOCMGET)");
    }
    return (bits & TIOCM_CTS) != 0;
#else
    throw boost::system::system_error(boost::asio::error::operation_not_supported,
                                      "Serial CTS status is not supported on this platform");
#endif
  }

  void BoostSerialPortDevice::setFlowcontrol(const Flowcontrol &value)
  {
    auto lock = lockStandaloneFlowControl();
    state->setOption(value);
  }

  void BoostSerialPortDevice::applyHardwareFlowControl(bool enabled)
  {
    state->setOption(Flowcontrol(enabled ? Flowcontrol::hardware : Flowcontrol::none));
  }

  bool BoostSerialPortDevice::readHardwareFlowControl() const
  {
    const auto value = getFlowcontrol().value();
    if (value == Flowcontrol::software)
      throw std::logic_error("XBee requires RTS/CTS or no flow control, not XON/XOFF");
    return value == Flowcontrol::hardware;
  }

}
