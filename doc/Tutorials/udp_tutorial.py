#!/usr/bin/env python

import sys

sys.path.append("/home/garciafa/Documents/PROG/paparazzi_messages/pprzlink/lib/v2.0/python/")

import threading
import time

import pprzlink.udp
import pprzlink.messages_xml_map as messages_xml_map
import pprzlink.message as message

# Some constants of the program
ping_port = 2010 # The port to which the PING are sent
pong_port = 2011 # The port to which the PONG are sent
remote_address = "127.0.0.1"
my_sender_id = 1
my_receiver_id = 2


class UDPTutorial:
    """
    Class UDPTutorial that uses udp.UdpMessagesInterface to listen to incoming messages. 
    If a PING message arrives, it will answer with a PONG message back to the sender.

    It can also send PING every 2 seconds if constructed with the parameter ping_sender 
    to True.
    """

    # Construction of the UDPTutorial object
    def __init__(self,ping_sender = False):
        self.ping_sender = ping_sender
        if ping_sender:
            # If we should send the pings, use ping_port as uplink_port (the port we send to) 
            # and pong_port as the downlink (the port we listen to)
            self.udp = pprzlink.udp.UdpMessagesInterface(
                            self.proccess_msg,          # Callback function
                            uplink_port = ping_port,    # Port we send messages to 
                            downlink_port = pong_port,  # Port used to receive messages
                            interface_id = my_sender_id # Numerical id of the interface (ac_id)
                            )
        else:
            # If we should not send the pings, use pong_port as uplink_port (the port we send to)
            # and ping_port as the downlink (the port we listen to)
            self.udp = pprzlink.udp.UdpMessagesInterface(
                            self.proccess_msg,            # Callback function
                            uplink_port = pong_port,      # Port we send messages to 
                            downlink_port = ping_port,    # Port used to receive messages
                            interface_id = my_receiver_id # Numerical id of the interface (ac_id)
                            )

    # Function used as callback when messages are received
    def proccess_msg(self,sender,address,msg,length,receiver_id=None, component_id=None):
        # If it is a PING send a PONG, else print message information
        if msg.name=="PING":
            print("Received PING from %i %s [%d Bytes]" % (sender, address, length))
            pong = message.PprzMessage('telemetry', 'PONG')
            print ("Sending back %s to %s:%d (%d)" % (pong,address[0],address[1],sender))
            self.udp.send(pong, receiver_id, address[0], receiver = sender)
        else:
            print("Received message from %i %s [%d Bytes]: %s" % (sender, address, length, msg))

    # Activity function of this object
    def run(self):
        try:
            # Start the UDP interface
            self.udp.start()

            if self.ping_sender:
                    # Create a PING message
                    ping = message.PprzMessage('datalink', 'PING')

            # Wait for a ^C
            while True:
                if self.ping_sender:
                    # Send PING message
                    print ("Sending %s to %s:%d (%d)" % (ping,remote_address,ping_port,my_receiver_id))
                    self.udp.send(
                        ping,           # The message to send
                        my_sender_id,   # Our numerical id
                        remote_address, # The IP address of the destination
                        my_receiver_id  # The id of the destination
                        )
                time.sleep(2)

        except KeyboardInterrupt:
            self.udp.stop()

if __name__ == '__main__':
    from argparse import ArgumentParser

    # Parse arguments looking for the -s switch telling us we should send the PING
    parser = ArgumentParser(description="UDP Tutorial for pprzlink")
    parser.add_argument("-s","--send_ping",dest="send",default=False, action='store_true', help="Send the PING messages")
    args = parser.parse_args()

    # Run the UDPTutorial
    UDPTutorial(args.send).run()
