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

/** \file PprzTransport.cpp
 *
 *
 */


#include <iostream>
#include <memory>
#include <iomanip>
#include "PprzTransport.h"

namespace pprzlink {

  PprzTransport::PprzTransport(Device *device, const MessageDictionary &dictionary) : Transport(device, dictionary), transportBuffer(), currentMessage(nullptr)
  {
    transportBuffer.reserve(256); // This is enough for all pprz message (up to version 2.0) and should avoid mallocs
  }

  bool PprzTransport::hasMessage()
  {
    if (!currentMessage)
      decodeMessage();
    return (bool)currentMessage;
  }

  std::unique_ptr<Message> PprzTransport::getMessage()
  {
    if (!currentMessage)
      decodeMessage();

    return std::move(currentMessage);
  }

  size_t PprzTransport::sendMessage(Message const &msg)
  {
    BytesBuffer buffer;
    buffer.push_back(PPRZ_STX);
    buffer.push_back(msg.getDefinition().getMinimumSize()); // Length to be computed at the end
    if (msg.getSenderId().index()==0)
    {
      uint8_t ac_id;
      std::stringstream sstr(std::get<std::string>(msg.getSenderId()));
      sstr >> ac_id;
      buffer.push_back(ac_id);
    }
    else
    {
      buffer.push_back(std::get<uint8_t>(msg.getSenderId()));
    }
    buffer.push_back(msg.getReceiverId());
    uint8_t class_component_id = (msg.getClassId() & 0x0Fu) | ((msg.getComponentId() & 0x0Fu)<<4u);
    buffer.push_back(class_component_id);

    buffer.push_back(msg.getDefinition().getId());

    size_t fieldSize=0;
    // Add each field to the buffer
    for (size_t fieldIndex=0; fieldIndex < msg.getDefinition().getNbFields(); ++fieldIndex)
    {
      fieldSize+= msg.addFieldToBuffer(fieldIndex, buffer);
    }
    buffer[1]=8+fieldSize; // 6 header bytes + 2 checksum bytes + fieldSize

    uint8_t chk_A=0;
    uint8_t chk_B=0;

    for (size_t i=1; i<buffer.size();++i)
    {
      chk_A+=buffer[i];
      chk_B+=chk_A;
    }
    buffer.push_back(chk_A);
    buffer.push_back(chk_B);
    device->writeBuffer(buffer);

    return buffer.size();
  }

  bool PprzTransport::decodeMessage()
  {
    // Read all available bytes from device
    auto newBytes = device->readAll();
    transportBuffer.insert(transportBuffer.end(),newBytes.begin(),newBytes.end());

    // Look for PPRZ_STX at begining of message and discard anything that comes before
    while (transportBuffer.size()>0 && transportBuffer[0]!=PPRZ_STX)
    {
      transportBuffer.erase(transportBuffer.begin());
    }

    // Do we have the length of the message ?
    if (transportBuffer.size() > 2)
    {
      const uint8_t length = transportBuffer[1];
      // Do we have enough data for this message ?
      if (transportBuffer.size() >= length)
      {
        const uint8_t source = transportBuffer[2];
        const uint8_t destination = transportBuffer[3];
        const uint8_t class_component = transportBuffer[4] ;
        const uint8_t class_id = (class_component & 0x0Fu);
        const uint8_t component_id = (class_component & 0xF0u) >> 4u;
        const uint8_t message_id = transportBuffer[5];
        const uint8_t checksum_A = transportBuffer[length-2];
        const uint8_t checksum_B = transportBuffer[length-1];

        uint8_t chk_A=0;
        uint8_t chk_B=0;
        for (int i=1; i< length-2; ++i)
        {
          chk_A+=transportBuffer[i];
          chk_B+=chk_A;
        }

        if (chk_A!=checksum_A || chk_B!=checksum_B)
        {
          std::cerr << "Wrong checksum in message !\n";
          std::cerr << (int)chk_A << " !=" << (int)checksum_A << "\n";
          std::cerr << (int)chk_B << " != " << (int)checksum_B << "\n";
          // Remove STX so as to prevent reread on this message
          transportBuffer.erase(transportBuffer.begin());
          // Try again with the rest of the buffer
          return decodeMessage();
        }

        /*
        std::cout << "Message : \n ";
        std::cout << "\tsource " << (int)source << "\n";
        std::cout << "\tdestination " << (int)destination << "\n";
        std::cout << "\tclass " << (int)class_id << "\n";
        std::cout << "\tcomponent " << (int)component_id << "\n";
        std::cout << "\tmessage " << (int)message_id << "\n";
        std::cout << "\tpayload length " << (int)length-8 << "\n";
        std::cout << "\tminimum size " << dictionary.getDefinition(class_id,message_id).getMinimumSize() << "\n";
        std::cout << "\t\t=> " << dictionary.getDefinition(class_id,message_id).getName() << std::endl;
        */
        currentMessage = std::make_unique<Message>(dictionary.getDefinition(class_id,message_id));
        size_t offset=6; // Skip the header

        currentMessage->setSenderId(source);
        currentMessage->setReceiverId(destination);
        currentMessage->setComponentId(component_id);

        for (size_t fieldIndex=0; fieldIndex < currentMessage->getDefinition().getNbFields(); ++fieldIndex)
        {
          currentMessage->addFieldFromBuffer(fieldIndex,transportBuffer,offset);
        }
        transportBuffer.erase(transportBuffer.begin(),transportBuffer.begin()+length);
        return true;
      }
    }

    return false;
  }

}

/*
 PPRZ-message: ABCxxxxxxxDE
    A PPRZ_STX (0x99)
    B LENGTH (A->E)
    C PPRZ_DATA
      0 SOURCE (~sender_ID)
      1 DESTINATION (can be a broadcast ID)
      2 CLASS/COMPONENT
        bits 0-3: 16 class ID available
        bits 4-7: 16 component ID available
      3 MSG_ID
      4 MSG_PAYLOAD
      . DATA (messages.xml)
    D PPRZ_CHECKSUM_A (sum[B->C])
    E PPRZ_CHECKSUM_B (sum[ck_a])
 */

