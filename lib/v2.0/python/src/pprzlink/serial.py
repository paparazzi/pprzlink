#
# This file is part of PPRZLINK.
# 
# PPRZLINK is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# PPRZLINK is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with PPRZLINK.  If not, see <https://www.gnu.org/licenses/>.
#

from __future__ import absolute_import, division, print_function

import threading
import serial
import logging
import time
import struct
from enum import Enum
from typing import List, Tuple, Dict, Any

from pprzlink.message import PprzMessage
from pprzlink.pprz_transport import PprzTransport


logger = logging.getLogger("PprzLink")


class SerialMessagesInterface(threading.Thread):
    def __init__(self, callback, verbose=False, device='/dev/ttyUSB0', baudrate=115200,
                 msg_class='telemetry', interface_id=0):
        threading.Thread.__init__(self)
        self.callback = callback
        self.verbose = verbose
        self.msg_class = msg_class
        self.id = interface_id
        self.running = True
        try:
            self.ser = serial.Serial(device, baudrate, timeout=1.0)
        except serial.SerialException:
            logger.error("Error: unable to open serial port '%s'" % device)
            exit(0)
        self.trans = PprzTransport(msg_class)

    def stop(self):
        logger.info("End thread and close serial link")
        self.running = False
        self.ser.close()

    def shutdown(self):
        self.stop()

    def __del__(self):
        try:
            self.ser.close()
        except:
            pass

    def send(self, msg, sender_id=None, receiver_id=0, component_id=0):
        """ Send a message over a serial link"""
        if sender_id is None:
            sender_id = self.id
        if isinstance(msg, PprzMessage):
            data = self.trans.pack_pprz_msg(sender_id, msg, receiver_id, component_id)
            self.ser.write(data)
            self.ser.flush()

    def run(self):
        """Thread running function"""
        try:
            while self.running:
                # Parse incoming data
                c = self.ser.read(1)
                if len(c) == 1:
                    if self.trans.parse_byte(c):
                        try:
                            (sender_id, receiver_id, component_id, msg) = self.trans.unpack()
                        except ValueError as e:
                            logger.warning("Ignoring unknown message, %s" % e)
                        else:
                            if self.verbose:  # See the note on the same line in v1.0
                                logger.info("New incoming message '%s' from %i (%i) to %i" % (msg.name, sender_id, component_id, receiver_id))
                            # Callback function on new message
                            if self.id == receiver_id:
                                self.callback(sender_id, msg)

        except StopIteration:
            pass


class XbeeMessagesInterface(threading.Thread):
    XBEE_API_START = 0x7E
    GROUND_STATION_ADDR = 0x0100
    DEFAULT_TIMEOUT = 0.1

    class RxState(Enum):
        WAIT_START = 0
        GET_LEN = 1
        GET_FRAME_DATA = 2

    class ATStatus(Enum):
        Ok = 0
        Error = 1
        InvalidCmd = 2
        InvalidParam = 3

    class APITypes(Enum):
        MODEM_STATUS = 0x8A
        AT_CMD = 0x08
        AT_CMD_QU = 0x09
        AT_CMD_RES = 0x88
        REMOTE_AT_REQ = 0x17
        REMOTE_AT_RES = 0x97
        TX_64 = 0x00
        TX_16 = 0x01
        TX_STATUS = 0x89
        RX_64 = 0x80
        RX_16 = 0x81
        RX_IO_64 = 0x82
        RX_IO_16 = 0x83

    @staticmethod
    def from16(nb: int):
        return nb.to_bytes(2, 'big')

    @staticmethod
    def to16(data: bytes):
        return int.from_bytes(data, 'big')

    def __init__(self, callback, id, port, baudrate=57600, verbose=False):
        threading.Thread.__init__(self)
        self.id = id
        if id == 0:
            self.id = self.GROUND_STATION_ADDR
        self.ser = serial.Serial(port, baudrate)
        self.rx_state = XbeeMessagesInterface.RxState.WAIT_START
        self.bytes_needed = 1
        self.running = True
        self._frame_id = 0
        self.responses = {}     # type: Dict[int, Any]
        self.verbose = verbose
        self.callback = callback
        self.buffer = bytearray()

    def start(self) -> None:
        threading.Thread.start(self)
        self.at_cmd(b'MY', self.from16(self.id), resp=False)

    def stop(self):
        if self.running:
            self.running = False
            time.sleep(0.1)
            self.ser.close()

    def __del__(self):
        self.stop()

    @staticmethod
    def checksum(frame):
        return 0xFF - (sum(frame) & 0xFF)

    @property
    def frame_id(self):
        self._frame_id = (self._frame_id + 1) % 0xFF
        return self._frame_id

    def send(self, msg: PprzMessage, sender_id=None, receiver_id: int = 0, component_id=0, ack=False):
        """
        msg: PprzMessage
        destination: integer. 0 is the ground station, 0xFF is the broadcast address
            Can also be > 0XFF (<0xFFFF) (but not for drones). receiver id will be set to 0 (like ground station).
        """
        if not isinstance(msg, PprzMessage):
            raise TypeError("not a PprzMessage")
        if not (isinstance(receiver_id, int) and receiver_id < 0xFFFF):
            raise TypeError("destination invalid")
        if sender_id is None:
            msg_sender = self.id
        else:
            msg_sender = sender_id
        if self.id == self.GROUND_STATION_ADDR:
            msg_sender = 0
        xbee_dest = receiver_id
        if receiver_id == 0xFF:     # broadcast : xbee use 0xFFFF
            xbee_dest = 0xFFFF
        elif receiver_id == 0:    # ground station
            xbee_dest = self.GROUND_STATION_ADDR
        elif receiver_id > 0xFF:
            receiver_id = 0         # for non-drone destinations, assume its ground
        class_comp = msg.class_id | (component_id << 4)

        header = struct.pack("<BBBB", msg_sender, receiver_id, class_comp, msg.msg_id)
        payload = header + msg.payload_to_binary()
        self.send_data(xbee_dest, payload, ack=ack)

    def rx_16_cb(self, source, rssi, options, data: bytes):
        sender = data[0]
        receiver = data[1]
        class_id = data[2] & 0x0F
        component_id = data[2] >> 4
        msg_id = data[3]
        msg_payload = data[4:]
        if (receiver == self.id or                                  # message addressed to me
           receiver == 0xFF or                                      # broadcast message
           (receiver == 0 and self.id == self.GROUND_STATION_ADDR)):     # message to ground station
            msg = PprzMessage(class_id, msg_id)
            msg.binary_to_payload(msg_payload)
            self.callback(sender, msg)

    def handle_rcv(self, frame):
        frame_type = frame[0]
        if frame_type == self.APITypes.MODEM_STATUS.value:
            pass    # TODO
        elif int(frame_type) == self.APITypes.AT_CMD_RES.value:
            frame_id = frame[1]
            at_cmd = frame[2:4]
            status = self.ATStatus(frame[4])
            data = frame[5:]
            self.responses[frame_id] = (status, data)
        elif frame_type == self.APITypes.RX_16.value:
            source_addr = self.to16(frame[1:3])
            rssi = frame[3]
            options = frame[4]
            data = frame[5:]
            self.rx_16_cb(source_addr, rssi, options, data)
        elif frame_type == self.APITypes.TX_STATUS.value:
            frame_id = frame[1]
            status = frame[2]
            self.responses[frame_id] = status
        else:
            print("unknown frame type:", hex(frame_type))

    def send_data(self, dest, data, ack=False, options=0):
        if ack:
            frame_id = self.frame_id
        else:
            frame_id = 0
        frame = struct.pack(">BBHB", self.APITypes.TX_16.value, frame_id, dest, options) + data
        self.transmit_frame(frame)

    def get_response_timeout(self, frame_id, timeout=DEFAULT_TIMEOUT) -> Any:
        if frame_id == 0:
            return None
        start_time = time.time()
        while frame_id not in self.responses:
            if time.time() - start_time > timeout:
                raise TimeoutError()
            time.sleep(0.01)
        return self.responses.pop(frame_id)

    def at_cmd(self, at, value=None, resp=True):
        if resp:
            frame_id = self.frame_id
        else:
            frame_id = 0
        cmd = struct.pack("<BB", 0x08, frame_id) + at
        if value is not None:
            cmd += value
        self.transmit_frame(cmd)
        return self.get_response_timeout(frame_id)

    def transmit_frame(self, frame):
        data = struct.pack(">BH", self.XBEE_API_START, len(frame)) + frame + struct.pack("<B", self.checksum(frame))
        self.ser.write(data)
        # self.ser.flush()

    def run(self):
        while self.running:
            self.buffer.extend(self.ser.read())
            if len(self.buffer) < self.bytes_needed:
                continue
            data = self.buffer[0:self.bytes_needed]
            self.buffer = self.buffer[self.bytes_needed:]
            if self.rx_state == self.RxState.WAIT_START:
                if data[0] == self.XBEE_API_START:
                    self.rx_state = self.RxState.GET_LEN
                    self.bytes_needed = 2
            elif self.rx_state == self.RxState.GET_LEN:
                self.bytes_needed = ((data[0] << 8) + data[1]) + 1  # Add 1 byte for the checksum
                self.rx_state = self.RxState.GET_FRAME_DATA
            elif self.rx_state == self.RxState.GET_FRAME_DATA:
                frame = data[:-1]
                chk = data[-1]
                if self.checksum(frame) == chk:
                    self.handle_rcv(frame)
                else:
                    print("invalid chk: {}  {}".format(sum(frame), chk))
                self.rx_state = self.RxState.WAIT_START
                self.bytes_needed = 1
        print("stopped")


def test():
    '''
    run test program as a module to avoid namespace conflicts with serial module:
    
    python -p pprzlink.serial

    pprzlink should be installed in a python standard path or included to your PYTHONPATH
    '''
    import time
    import argparse
    from pprzlink import messages_xml_map

    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", help="path to messages.xml file")
    parser.add_argument("-c", "--class", help="message class", dest='msg_class', default='telemetry')
    parser.add_argument("-d", "--device", help="device name", dest='dev', default='/dev/ttyUSB0')
    parser.add_argument("-b", "--baudrate", help="baudrate", dest='baud', default=115200, type=int)
    parser.add_argument("-id", "--ac_id", help="aircraft id (receiver)", dest='ac_id', default=42, type=int)
    parser.add_argument("--interface_id", help="interface id (sender)", dest='id', default=0, type=int)
    parser.add_argument("-t", "--transport", help="pprz or xbee", dest='transport', default="serial")
    args = parser.parse_args()
    messages_xml_map.parse_messages(args.file)
    if args.transport == "pprz":
        interface = SerialMessagesInterface(lambda s, m: print("new message from %i: %s" % (s, m)), device=args.dev,
                                               baudrate=args.baud, msg_class=args.msg_class, interface_id=args.id, verbose=True)
        print("Starting serial interface on %s at %i baud" % (args.dev, args.baud))
    elif args.transport == "xbee":
        interface = XbeeMessagesInterface(lambda s, m: print("new message from %i: %s" % (s, m)), args.id, args.dev, args.baud, verbose=True)
        print("Starting xbee interface on %s at %i baud" % (args.dev, args.baud))
    else:
        print("No such transport: ", args.transport)
        exit(1)

    try:
        interface.start()

        # give the thread some time to properly start
        time.sleep(0.1)

        # send some datalink messages to aicraft for test
        ac_id = args.ac_id
        print("sending ping")
        ping = PprzMessage('datalink', 'PING')
        interface.send(ping, 0)

        get_setting = PprzMessage('datalink', 'GET_SETTING')
        get_setting['index'] = 0
        get_setting['ac_id'] = ac_id
        interface.send(get_setting, 0)

        # change setting with index 0 (usually the telemetry mode)
        set_setting = PprzMessage('datalink', 'SETTING')
        set_setting['index'] = 12
        set_setting['ac_id'] = ac_id
        set_setting['value'] = 1
        interface.send(set_setting, 0)

        # block = PprzMessage('datalink', 'BLOCK')
        # block['block_id'] = 3
        # block['ac_id'] = ac_id
        # serial_interface.send(block, 0)

        while interface.is_alive():
            interface.join(1)
    except (KeyboardInterrupt, SystemExit):
        print('Shutting down...')
        interface.stop()
        exit()


if __name__ == '__main__':
    test()
