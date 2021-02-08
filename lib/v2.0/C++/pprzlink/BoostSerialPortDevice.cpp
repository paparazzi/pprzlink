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

/** \file BoostSerialPortDevice.cpp
 *
 *
 */


#include "BoostSerialPortDevice.h"
#include <boost/asio/read.hpp>
#include <iostream>
#include <functional>

namespace pprzlink {

  BoostSerialPortDevice::BoostSerialPortDevice(boost::asio::io_service &ioService, std::string serialPortName)
    : ioService(ioService), serialPort(ioService, serialPortName),availBytes(0)
  {}

  size_t BoostSerialPortDevice::availableBytes()
  {
    return availBytes;
  }

  BytesBuffer BoostSerialPortDevice::readAll()
  {
    auto tmp = availBytes;
    availBytes=0;
    serialPort.cancel();
    startReception();
    return BytesBuffer(buffer.begin(),buffer.begin()+tmp);
  }

  void BoostSerialPortDevice::writeBuffer(const BytesBuffer &data)
  {
    serialPort.write_some(boost::asio::buffer(data));
  }

  const BoostSerialPortDevice::Baudrate &BoostSerialPortDevice::getBaudrate() const
  {
    return baudrate;
  }

  void BoostSerialPortDevice::setBaudrate(const BoostSerialPortDevice::Baudrate &baudrate)
  {
    BoostSerialPortDevice::baudrate = baudrate;
    serialPort.set_option(baudrate);
  }

  const BoostSerialPortDevice::DataBits &BoostSerialPortDevice::getDataBits() const
  {
    return dataBits;
  }

  void BoostSerialPortDevice::setDataBits(const BoostSerialPortDevice::DataBits &dataBits)
  {
    BoostSerialPortDevice::dataBits = dataBits;
    serialPort.set_option(dataBits);
  }

  const BoostSerialPortDevice::Parity &BoostSerialPortDevice::getParity() const
  {
    return parity;
  }

  void BoostSerialPortDevice::setParity(const BoostSerialPortDevice::Parity &parity)
  {
    BoostSerialPortDevice::parity = parity;
    serialPort.set_option(parity);
  }

  const BoostSerialPortDevice::StopBits &BoostSerialPortDevice::getStopBits() const
  {
    return stopBits;
  }

  void BoostSerialPortDevice::setStopBits(const BoostSerialPortDevice::StopBits &stopBits)
  {
    BoostSerialPortDevice::stopBits = stopBits;
    serialPort.set_option(stopBits);
  }

  const BoostSerialPortDevice::Flowcontrol &BoostSerialPortDevice::getFlowcontrol() const
  {
    return flowcontrol;
  }

  void BoostSerialPortDevice::setFlowcontrol(const BoostSerialPortDevice::Flowcontrol &flowcontrol)
  {
    BoostSerialPortDevice::flowcontrol = flowcontrol;
    serialPort.set_option(flowcontrol);
  }

  void
  BoostSerialPortDevice::dataReceptionHandler(const boost::system::error_code &error, std::size_t bytes_transferred)
  {
    if (!error)
    {
      availBytes += bytes_transferred;

      // Continue waiting for data
      startReception();
    }
    else
    {
      // If the error is anything else than a cancelation of the operation throw the corresponding system_error
      if (error.value() != boost::system::errc::errc_t::operation_canceled)
      {
        throw boost::system::system_error(error);
      }
    }
  }

  void BoostSerialPortDevice::startReception()
  {
    using std::placeholders::_1;
    using std::placeholders::_2;
    serialPort.async_read_some(boost::asio::buffer(buffer.begin()+availBytes, BOOSTSERIAL_BUFFER_SIZE - availBytes), std::bind(&BoostSerialPortDevice::dataReceptionHandler, this, _1, _2));
  }
}