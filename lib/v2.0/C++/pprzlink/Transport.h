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
 * @file Transport.h
 * @brief Exclusive device ownership and message transport abstraction.
 * @ingroup transports
 *
 * Calls on a transport must be serialized by the application. I/O failures can occur after partial external writes, even though the API never reports partial success.
 */

#ifndef PPRZLINKCPP_TRANSPORT_H
#define PPRZLINKCPP_TRANSPORT_H

#include "Message.h"
#include "Device.h"
#include "MessageDictionary.h"
#include "TransportStatistics.h"
#include "ReceivedMessage.h"
#include "Receiver.h"
#include <memory>
#include <stdexcept>
#include <utility>

namespace pprzlink {
  /// Owns its device exclusively; the borrowed dictionary must outlive the transport.
  /// Calls on the same transport must be serialized by the application.
  /// @ingroup transports
  class Transport : public Receiver {
  public:
    /// @brief Take exclusive ownership of a byte stream and borrow its schemas.
    /// @param[in] device Non-null device whose ownership moves into this transport.
    /// @param[in] dictionary Schemas that must outlive this transport.
    /// @param[in] kind Source metadata family used to validate receive filters.
    /// @throws std::invalid_argument The device pointer is null.
    explicit Transport(std::unique_ptr<Device> device, const MessageDictionary &dictionary,
                       Kind kind = Kind::Stream)
      : Receiver(dictionary, kind), device(std::move(device)), dictionary(dictionary)
    {
      if (!this->device) throw std::invalid_argument("Transport requires a device");
    }

    /// @brief Destroy the owned device and release transport state.
    ~Transport() override
    {
      shutdown();
      device->setReceiveCallback({});
      try { device->stopReception(); } catch (...) {}
    }
    /// @brief Copying an exclusively owned transport is prohibited.
    Transport(const Transport&) = delete;
    /// @brief Copy assignment is prohibited.
    /// @return This operation is deleted and cannot return.
    Transport& operator=(const Transport&) = delete;
    /// @brief Moving a live transport is prohibited.
    Transport(Transport&&) = delete;
    /// @brief Move assignment is prohibited.
    /// @return This operation is deleted and cannot return.
    Transport& operator=(Transport&&) = delete;

    /// @brief Subscribe to device readiness and start asynchronous input.
    /// @throws std::exception Device startup or an unhandled terminal error fails.
    void start() override
    {
      auto lock = lockReceiver();
      if (!activate()) return;
      device->setReceiveCallback(guarded([this] {
        try { receiveAvailable(); }
        catch (...) { stop(); throw; }
      }));
      try { device->startReception(); receptionStarted(); }
      catch (...) { stop(); throw; }
    }
    /// @brief Stop device reception without stopping the shared event loop.
    void stop() override
    {
      auto lock = lockReceiver();
      deactivate();
      device->setReceiveCallback({});
      device->stopReception();
    }

    /// Write a complete message and return the number of bytes sent, or throw.
    /// @param[in] msg Fully populated message compatible with the selected transport.
    /// @return Total encoded frame bytes written, not acknowledgement of remote delivery.
    /// @throws std::exception Validation, encoding or I/O failure.
    virtual size_t sendMessage(const Message &msg) = 0;

    /// @brief Borrow the current cumulative reception counters.
    /// @return Statistics reference, updated by later receives and valid until transport destruction.
    [[nodiscard]] const TransportStatistics &getStatistics() const noexcept { return statistics; }
    /// Size of the last successfully decoded message's complete transport frame.
    /// @return Frame bytes, or zero before any successful receive.
    [[nodiscard]] size_t getLastReceivedFrameSize() const noexcept { return lastReceivedFrameSize; }

    /// Borrow the device until transport destruction; never delete it or transfer ownership.
    /// @return Reference to the exclusively owned device.
    [[nodiscard]] Device& getDevice() noexcept { return *device; }
    /// @brief Borrow the device without permitting mutation through this reference.
    /// @return Device reference valid until transport destruction.
    [[nodiscard]] const Device& getDevice() const noexcept { return *device; }

  protected:
    /// @brief Consume available device input, reporting channel failures through onError().
    /// @return Bytes received; empty after a terminal failure has stopped reception.
    BytesBuffer receiveBytes()
    {
      try { return device->readAll(); }
      catch (...) { auto error = std::current_exception(); stop(); reportError(error, ReceiveError::Kind::Io); return {}; }
    }
    /// @brief Decode and distribute input following a device readiness notification.
    virtual void receiveAvailable() = 0;
    /// @brief Hook for protocol deadline setup after asynchronous reads have started.
    virtual void receptionStarted() {}
    std::unique_ptr<Device> device; ///< Exclusively owned byte stream.
    const MessageDictionary &dictionary; ///< Borrowed XML schemas.
    TransportStatistics statistics; ///< Receive counters maintained by the concrete transport.
    size_t lastReceivedFrameSize = 0; ///< Last successful frame length, including its envelope.
  };
}
#endif // PPRZLINKCPP_TRANSPORT_H
