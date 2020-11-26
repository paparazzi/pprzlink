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
PPRZLINK UDP to Python interface
"""
from __future__ import absolute_import, division, print_function

import threading
import socket
import logging

# load pprzlink messages and transport
from .message import PprzMessage
from .pprz_transport import PprzTransport

# default port
UPLINK_PORT = 4243
DOWNLINK_PORT = 4242


logger = logging.getLogger("PprzLink")


class UdpMessagesInterface(threading.Thread):
    def __init__(self, callback, verbose=False,
                 uplink_port=UPLINK_PORT, downlink_port=DOWNLINK_PORT,
                 msg_class='telemetry'):
        threading.Thread.__init__(self)
        self.callback = callback
        self.verbose = verbose
        self.msg_class = msg_class
        self.uplink_port = uplink_port
        self.downlink_port = downlink_port
        self.ac_downlink_status = {}
        self.running = True
        try:
            self.server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.server.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self.server.settimeout(2.0)
            self.server.bind(('0.0.0.0', self.downlink_port))
        except OSError:
            logger.error("Error: unable to open socket on ports '%d' (up) and '%d' (down)" % (self.uplink_port, self.downlink_port))
            exit(0)
        self.trans = PprzTransport(msg_class)

    def stop(self):
        logger.info("End thread and close UDP link")
        self.running = False
        self.server.close()

    def shutdown(self):
        self.stop()

    def __del__(self):
        try:
            self.server.close()
        except:
            pass

    def send(self, msg, sender_id, address):
        """ Send a message over a UDP link"""
        if isinstance(msg, PprzMessage):
            data = self.trans.pack_pprz_msg(sender_id, msg)
            try:
                self.server.sendto(data, (address, self.uplink_port))
            except:
                pass # TODO better error handling

    def run(self):
        """Thread running function"""
        try:
            while self.running:
                # Parse incoming data
                try:
                    (msg, address) = self.server.recvfrom(2048)
                    length = len(msg)
                    for c in msg:
                        if self.trans.parse_byte(c):
                            try:
                                (sender_id, msg) = self.trans.unpack()
                            except ValueError as e:
                                logger.warning("Ignoring unknown message, %s" % e)
                            else:
                                if self.verbose:
                                    logger.info("New incoming message '%s' from %i (%s)" % (msg.name, sender_id, address))
                                # Callback function on new message
                                self.callback(sender_id, address, msg, length)
                except socket.timeout:
                    pass

        except StopIteration:
            pass


def test():
    import time
    import argparse
    from pprzlink import messages_xml_map

    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", help="path to messages.xml file")
    parser.add_argument("-c", "--class", help="message class of incoming messages", dest='msg_class', default='telemetry')
    parser.add_argument("-a", "--address", help="destination address", dest='address', default='127.0.0.1')
    parser.add_argument("-id", "--ac_id", help="aircraft id", dest='ac_id', default=42, type=int)
    parser.add_argument("-up", "--uplink_port", help="uplink port", dest='uplink', default=UPLINK_PORT, type=int)
    parser.add_argument("-dp", "--downlink_port", help="downlink port", dest='downlink', default=DOWNLINK_PORT, type=int)
    args = parser.parse_args()
    messages_xml_map.parse_messages(args.file)
    udp_interface = UdpMessagesInterface(lambda s, a, m: print("new message from %i (%s): %s" % (s, a, m)),
                                               uplink_port=args.uplink, downlink_port=args.downlink,
                                               msg_class=args.msg_class, verbose=True)

    print("Starting UDP interface with '%s' with id '%d'" % (args.address, args.ac_id))
    address = (args.address, args.uplink)
    try:
        udp_interface.start()

        # give the thread some time to properly start
        time.sleep(0.1)

        # send some datalink messages to aicraft for test
        ac_id = args.ac_id
        print("sending ping")
        ping = PprzMessage('datalink', 'PING')
        udp_interface.send(ping, 0, address)

        print("sending get_setting")
        get_setting = PprzMessage('datalink', 'GET_SETTING')
        get_setting['index'] = 0
        get_setting['ac_id'] = ac_id
        udp_interface.send(get_setting, 0, address)

        # change setting with index 0 (usually the telemetry mode)
        print("sending setting")
        set_setting = PprzMessage('datalink', 'SETTING')
        set_setting['index'] = 0
        set_setting['ac_id'] = ac_id
        set_setting['value'] = 1
        udp_interface.send(set_setting, 0, address)

        # print("sending block")
        # block = PprzMessage('datalink', 'BLOCK')
        # block['block_id'] = 3
        # block['ac_id'] = ac_id
        # udp_interface.send(block, 0, address)

        while udp_interface.isAlive():
            udp_interface.join(1)
    except (KeyboardInterrupt, SystemExit):
        print('Shutting down...')
        udp_interface.stop()
        exit()


if __name__ == '__main__':
    test()
