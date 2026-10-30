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
 * @file BoostSerialPortDevice.h
 * @brief Asio-backed serial byte stream with shared asynchronous state.
 * @ingroup transports
 *
 * The caller owns and services the io_context. Reads and configuration are synchronized internally; transports still require serialized access.
 */

#ifndef PPRZLINKCPP_BOOSTSERIALPORTDEVICE_H
#define PPRZLINKCPP_BOOSTSERIALPORTDEVICE_H

#include "Device.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/serial_port.hpp>
#include <memory>
#include <string>

namespace pprzlink {
  /** Serial byte stream with asynchronous reception and complete synchronous writes.
   * @ingroup transports
   * Run the supplied io_context to service reception; it must outlive this device.
   * I/O and option calls are serialized internally, including completion handlers.
   * Finish external calls before destruction. Pending handlers retain their own state.
   */
  class BoostSerialPortDevice : public SerialDevice {
  public:
    using Baudrate = boost::asio::serial_port_base::baud_rate; ///< UART rate in bits per second.
    using Parity = boost::asio::serial_port_base::parity; ///< None, odd or even parity.
    using StopBits = boost::asio::serial_port_base::stop_bits; ///< UART stop-bit configuration.
    using DataBits = boost::asio::serial_port_base::character_size; ///< Bits in each UART character.
    using Flowcontrol = boost::asio::serial_port_base::flow_control; ///< None, software or hardware flow control.

    /// @brief Open an owned serial port using a caller-owned Asio context.
    /// @param[in] context Context that must be serviced and outlive this device.
    /// @param[in] serialPortName Native device name, including a pseudo-terminal when supported.
    /// @throws boost::system::system_error Opening or initializing the serial port fails.
    BoostSerialPortDevice(boost::asio::io_context &context, std::string serialPortName);
    /// @brief Cancel reception and close the port; queued handlers retain their own state.
    /// External callers must have finished before destruction.
    ~BoostSerialPortDevice() override;
    /// @brief Copying the serial owner is prohibited.
    BoostSerialPortDevice(const BoostSerialPortDevice&) = delete;
    /// @brief Copy assignment is prohibited.
    /// @return This operation is deleted and cannot return.
    BoostSerialPortDevice& operator=(const BoostSerialPortDevice&) = delete;
    /// @brief Moving the serial owner is prohibited.
    BoostSerialPortDevice(BoostSerialPortDevice&&) = delete;
    /// @brief Move assignment is prohibited.
    /// @return This operation is deleted and cannot return.
    BoostSerialPortDevice& operator=(BoostSerialPortDevice&&) = delete;

    /// @brief Inspect completed serial input without consuming it.
    /// @return Buffered bytes; this call does not progress the Asio event loop.
    /// @throws boost::system::system_error A receive error, after buffered input is exhausted.
    size_t availableBytes() override;
    /// Drain received bytes and start/resume reception (also works with polling transports).
    /// A receive error is reported after any already buffered bytes have been drained.
    /// @return Owned buffered bytes, or an empty buffer before a read completion.
    /// @throws boost::system::system_error A deferred receive error after draining input.
    BytesBuffer readAll() override;
    /// @brief Synchronously write the entire buffer under the device lock.
    /// @param[in] data Raw bytes to send; a failure may follow a partial external write.
    /// @throws boost::system::system_error Writing the port fails.
    void writeBuffer(const BytesBuffer &data) override;
    /// @brief Cancel pending reads, discard old-rate input and restart reception at a new rate.
    /// @param[in] baudrate Positive host UART rate, in bits per second.
    /// @throws std::invalid_argument The requested rate is zero.
    /// @throws boost::system::system_error A pending receive error or port configuration failure.
    void resetBaudrate(unsigned int baudrate) override;

    /// @brief Read the logical CTS input using the native serial driver.
    /// @return True for asserted CTS (clear to send), false for deasserted CTS.
    /// POSIX uses TIOCMGET/TIOCM_CTS. The query is serialized with other port I/O
    /// and does not wait for CTS to change or modify flow-control configuration.
    /// @throws boost::system::system_error Native query failure (including unsupported
    /// modem lines on PTYs/adapters), or operation_not_supported on other platforms.
    [[nodiscard]] bool getCTS() const override;

    /// @brief Query the current UART speed.
    /// @return The port's baud-rate option.
    /// @throws boost::system::system_error Querying the option fails.
    [[nodiscard]] Baudrate getBaudrate() const;
    /// @brief Change the UART speed without the old-input reset of resetBaudrate().
    /// @param[in] value Asio baud-rate option.
    /// @throws boost::system::system_error Applying the setting fails.
    void setBaudrate(const Baudrate &value);
    /// @brief Query the number of bits per character.
    /// @return The current character-size option.
    /// @throws boost::system::system_error Querying the option fails.
    [[nodiscard]] DataBits getDataBits() const;
    /// @brief Configure the number of bits per character.
    /// @param[in] value Asio character-size option.
    /// @throws boost::system::system_error Applying the setting fails.
    void setDataBits(const DataBits &value);
    /// @brief Query UART parity.
    /// @return The current parity option.
    /// @throws boost::system::system_error Querying the option fails.
    [[nodiscard]] Parity getParity() const;
    /// @brief Configure UART parity.
    /// @param[in] value Asio parity option.
    /// @throws boost::system::system_error Applying the setting fails.
    void setParity(const Parity &value);
    /// @brief Query the UART stop-bit configuration.
    /// @return The current stop-bit option.
    /// @throws boost::system::system_error Querying the option fails.
    [[nodiscard]] StopBits getStopBits() const;
    /// @brief Configure the UART stop bits.
    /// @param[in] value Asio stop-bit option.
    /// @throws boost::system::system_error Applying the setting fails.
    void setStopBits(const StopBits &value);
    /// @brief Query UART flow control.
    /// @return The current flow-control option.
    /// @throws boost::system::system_error Querying the option fails.
    [[nodiscard]] Flowcontrol getFlowcontrol() const;
    /// @brief Configure UART flow control.
    /// @param[in] value Asio flow-control option.
    /// @throws std::logic_error XBee owns flow control; reinitialize with XbeeConfiguration instead.
    /// @throws boost::system::system_error Applying the setting fails.
    void setFlowcontrol(const Flowcontrol &value);

    /// Idempotent. A stopped reception can be restarted even before cancellation completes.
    /// @throws boost::system::system_error A receive error is already latched.
    void startReception() override;
    /// Cancel reception, keeping the port open and the buffered bytes available.
    /// @throws boost::system::system_error Cancellation fails.
    void stopReception() override;
    /// @brief Replace the readiness observer; it runs after releasing the device mutex.
    /// @param[in] callback Observer notified of received bytes or deferred read errors.
    void setReceiveCallback(ReceiveCallback callback) override;
    /// @brief Return the serial port's caller-owned event-loop executor.
    /// @return Executor serviced by the supplied io_context.
    boost::asio::any_io_executor getExecutor() override;

  private:
    void applyHardwareFlowControl(bool enabled) override;
    bool readHardwareFlowControl() const override;
    struct State;
    std::shared_ptr<State> state;
  };
}
#endif // PPRZLINKCPP_BOOSTSERIALPORTDEVICE_H
