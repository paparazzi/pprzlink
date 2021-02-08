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

/** \file BoostSerialPortDevice.h
 *
 *
 */


#ifndef PPRZLINKCPP_BOOSTSERIALPORTDEVICE_H
#define PPRZLINKCPP_BOOSTSERIALPORTDEVICE_H

#include "Device.h"
#include <boost/asio/io_service.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/asio/streambuf.hpp>

#define BOOSTSERIAL_BUFFER_SIZE (1024)

namespace pprzlink {
  class BoostSerialPortDevice : public Device {
  public:
    using Baudrate = boost::asio::serial_port_base::baud_rate;
    using Parity = boost::asio::serial_port_base::parity;
    using StopBits = boost::asio::serial_port_base::stop_bits;
    using DataBits = boost::asio::serial_port_base::character_size;
    using Flowcontrol = boost::asio::serial_port_base::flow_control;

    BoostSerialPortDevice(boost::asio::io_service &ioService, std::string serialPortName);

    size_t availableBytes() override;

    BytesBuffer readAll() override;

    void writeBuffer(BytesBuffer const &data) override;

    [[nodiscard]] const Baudrate &getBaudrate() const;

    void setBaudrate(const Baudrate &baudrate);

    [[nodiscard]] const DataBits &getDataBits() const;

    void setDataBits(const DataBits &dataBits);

    [[nodiscard]] const Parity &getParity() const;

    void setParity(const Parity &parity);

    [[nodiscard]] const StopBits &getStopBits() const;

    void setStopBits(const StopBits &stopBits);

    [[nodiscard]] const Flowcontrol &getFlowcontrol() const;

    void setFlowcontrol(const Flowcontrol &flowcontrol);

    void startReception();

    void dataReceptionHandler(const boost::system::error_code& error, std::size_t bytes_transferred);

  protected:
    boost::asio::io_service &ioService;
    boost::asio::serial_port serialPort;
    Baudrate baudrate;
    DataBits dataBits;
    Parity parity;
    StopBits stopBits;
    Flowcontrol flowcontrol;
    std::array<uint8_t,BOOSTSERIAL_BUFFER_SIZE> buffer;
    size_t availBytes;
  };
}
#endif //PPRZLINKCPP_BOOSTSERIALPORTDEVICE_H
