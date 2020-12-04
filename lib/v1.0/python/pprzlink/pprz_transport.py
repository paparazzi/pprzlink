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

"""
Paparazzi transport encoding utilities

"""

from __future__ import absolute_import, division
import struct
from .message import PprzMessage

# use Enum from python 3.4 if available (https://www.python.org/dev/peps/pep-0435/)
# (backports as enum34 on pypi)
try:
    from enum import Enum
except ImportError:
    Enum = object

STX = 0x99

class PprzParserState(Enum):
    WaitSTX = 1
    GotSTX = 2
    GotLength = 3
    GotPayload = 4
    GotCRC1 = 5

class PprzTransport(object):
    """parser for binary Paparazzi messages"""
    def __init__(self, msg_class='telemetry'):
        self.msg_class = msg_class
        self.reset_parser()

    def reset_parser(self):
        self.state = PprzParserState.WaitSTX
        self.length = 0
        self.buf = []
        self.ck_a = 0
        self.ck_b = 0
        self.idx = 0

    def parse_byte(self, c):
        """parse new byte, return True when a new full message is available"""
        b = struct.unpack("<B", c)[0]
        if self.state == PprzParserState.WaitSTX:
            if b == STX:
                self.state = PprzParserState.GotSTX
        elif self.state == PprzParserState.GotSTX:
            self.length = b - 4
            self.buf = bytearray(self.length)
            self.ck_a = b % 256
            self.ck_b = b % 256
            self.idx = 0
            self.state = PprzParserState.GotLength
        elif self.state == PprzParserState.GotLength:
            self.buf[self.idx] = b
            self.ck_a = (self.ck_a + b) % 256
            self.ck_b = (self.ck_b + self.ck_a) % 256
            self.idx += 1
            if self.idx == self.length:
                self.state = PprzParserState.GotPayload
        elif self.state == PprzParserState.GotPayload:
            if self.ck_a == b:
                self.state = PprzParserState.GotCRC1
            else:
                self.state = PprzParserState.WaitSTX
        elif self.state == PprzParserState.GotCRC1:
            self.state = PprzParserState.WaitSTX
            if self.ck_b == b:
                """New message available"""
                return True
        else:
            self.state = PprzParserState.WaitSTX
        return False

    def get_buffer(self):
        return self.buf

    def unpack_pprz_msg(self, msg_class, data):
        """Unpack a raw PPRZ message"""
        sender_id = data[0]
        msg_id = data[1]
        msg = PprzMessage(msg_class, msg_id)
        msg.binary_to_payload(data[2:])
        return sender_id, msg

    def unpack(self):
        """Unpack the last received message"""
        return self.unpack_pprz_msg(self.msg_class, self.buf)

    def calculate_checksum(self, msg):
        ck_a = 0
        ck_b = 0
        # start char not included in checksum for pprz protocol
        for c in msg[1:]:
            # try to handle differences between python 2.x and 3.x
            if isinstance(c, str):
                c = struct.unpack("<B", c)[0]
            ck_a = (ck_a + c) % 256
            ck_b = (ck_b + ck_a) % 256
        return ck_a, ck_b

    def pack_pprz_msg(self, sender, msg):
        data = msg.payload_to_binary()
        # STX + length + sender_id + msg_id + data + ck_a + ck_b
        length = 6 + len(data)
        msg = struct.pack("<BBBB", STX, length, sender, msg.msg_id) + data
        (ck_a, ck_b) = self.calculate_checksum(msg)
        msg += struct.pack('<BB', ck_a, ck_b)
        return msg

