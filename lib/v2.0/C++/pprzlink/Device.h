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
 * @file Device.h
 * @brief Byte-stream and reconfigurable UART contracts.
 * @ingroup transports
 *
 * A transport owns its device. Writes must complete or throw; availableBytes and readAll expose only data already available to the implementation.
 */

#ifndef PPRZLINKCPP_DEVICE_H
#define PPRZLINKCPP_DEVICE_H

#include <pprzlink/BytesBuffer.h>
#include <cstdint>
#include <cstddef>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>
#include <functional>
#include <mutex>
#include <stdexcept>

namespace pprzlink {

  /// Byte stream used by a transport. Implementations define their threading contract.
  /// @ingroup transports
  class Device {
  public:
    using ReceiveCallback = std::function<void()>; ///< Input/error notification executed outside device I/O locks.
    /// @brief Release the device and any implementation-owned I/O resources.
    virtual ~Device() = default;
    /// @brief Set the observer notified when input or a terminal read error is available.
    /// @param[in] callback Observer retained until replacement; an empty callback disables notifications.
    virtual void setReceiveCallback(ReceiveCallback callback) = 0;
    /// @brief Start asynchronous reads on the device's executor, without an owned thread.
    virtual void startReception() = 0;
    /// @brief Cancel pending reads while keeping the device open.
    virtual void stopReception() = 0;
    /// @brief Return the executor used for read notifications and protocol deadlines.
    /// @return Borrowed-context executor; its execution context must outlive the device.
    virtual boost::asio::any_io_executor getExecutor() = 0;
    /// @brief Inspect already buffered input without consuming it.
    /// @return Number of bytes available to read without waiting for new input.
    /// @throws std::exception An implementation-specific I/O failure.
    virtual size_t availableBytes() = 0;

    /// Consume the bytes currently available; an empty buffer means no data yet.
    /// @return Owned bytes removed from the device's input buffer.
    /// @throws std::exception An implementation-specific I/O failure.
    virtual BytesBuffer readAll() = 0;

    /// Write the entire buffer or throw. On failure some bytes may already be sent.
    /// @param[in] data Bytes to deliver in their original order.
    /// @throws std::exception An implementation-specific I/O failure.
    virtual void writeBuffer(const BytesBuffer &data) = 0;
  };

  /// Byte stream whose UART rate and RTS/CTS can be changed during modem initialization.
  /// @ingroup transports
  class SerialDevice : public Device {
  public:
    /// Set the host baud rate and discard buffered/in-flight input from the old rate.
    /// The caller must own the dialogue and drain any stale input before sending commands.
    /// @param[in] baudrate New host UART rate, in bits per second.
    /// @throws std::exception An unsupported setting or I/O failure.
    virtual void resetBaudrate(unsigned int baudrate) = 0;

    /// @brief Query the instantaneous logical CTS input without waiting for assertion.
    /// @return True when CTS is asserted (clear to send), false when deasserted.
    /// This reads a signal, independently of whether hardware flow control is enabled.
    /// The sample does not reserve buffer space or guarantee a later write cannot block.
    /// @throws boost::system::system_error Status is unsupported or the native query fails.
    /// The default reports operation_not_supported; adapters may override this optional query.
    [[nodiscard]] virtual bool getCTS() const
    {
      throw boost::system::system_error(boost::asio::error::operation_not_supported,
                                        "Serial CTS status is not supported by this device");
    }

    /// @brief Inspect the host UART's hardware flow-control setting.
    /// @return True for RTS/CTS, false for no flow control.
    /// @throws std::exception The query fails or software flow control is active.
    bool getHardwareFlowControl() const
    {
      std::lock_guard lock(flowControlMutex);
      return readHardwareFlowControl();
    }

    /// @brief Configure RTS/CTS on a standalone UART, before handing it to XBee.
    /// @param[in] enabled True selects RTS/CTS; false disables flow control.
    /// @throws std::logic_error XBee owns this setting; use XbeeConfiguration instead.
    /// @throws std::exception The native setting cannot be applied or verified.
    void setHardwareFlowControl(bool enabled)
    {
      auto lock = lockStandaloneFlowControl();
      applyHardwareFlowControl(enabled);
      if (readHardwareFlowControl() != enabled)
        throw std::runtime_error("Host UART did not apply the requested RTS/CTS setting");
    }

  protected:
    /// @brief Serialize a standalone flow-control change and reject XBee-owned changes.
    /// @return Lock held until the native option update is complete.
    /// @throws std::logic_error A modem has claimed flow-control configuration.
    std::unique_lock<std::mutex> lockStandaloneFlowControl() const
    {
      std::unique_lock lock(flowControlMutex);
      if (xbeeOwnsFlowControl)
        throw std::logic_error("XBee owns UART flow control; use XbeeConfiguration::hardwareFlowControl and reinitialize");
      return lock;
    }

    /// @brief Apply RTS/CTS or no flow control without changing the baud rate or input buffers.
    /// @param[in] enabled Whether to select hardware flow control.
    /// @throws std::exception The native setting cannot be applied.
    virtual void applyHardwareFlowControl(bool enabled) = 0;
    /// @brief Query the native RTS/CTS setting, rejecting software flow control.
    /// @return True for RTS/CTS, false for no flow control.
    /// @throws std::exception Query failure or unsupported software flow control.
    virtual bool readHardwareFlowControl() const = 0;

  private:
    friend class XbeeModem;
    /// @brief Claim flow control for modem initialization and verify each native update.
    /// @param[in] enabled Host setting during bootstrap or after the final ATCN.
    /// @throws std::exception The native setting cannot be applied or verified.
    void configureXbeeHardwareFlowControl(bool enabled)
    {
      std::lock_guard lock(flowControlMutex);
      xbeeOwnsFlowControl = true;
      applyHardwareFlowControl(enabled);
      if (readHardwareFlowControl() != enabled)
        throw std::runtime_error("Host UART did not apply the requested RTS/CTS setting");
    }
    mutable std::mutex flowControlMutex; ///< Serializes native updates and ownership checks.
    bool xbeeOwnsFlowControl = false; ///< Once claimed, only modem initialization can change RTS/CTS.
  };
}
#endif // PPRZLINKCPP_DEVICE_H
