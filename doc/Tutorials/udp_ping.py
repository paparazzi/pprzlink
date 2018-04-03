#!/usr/bin/env python

import sys

sys.path.append("/home/garciafa/Documents/PROG/paparazzi_messages/pprzlink/lib/v2.0/python/")

import threading
import time

import pprzlink.udp
import pprzlink.messages_xml_map as messages_xml_map
import pprzlink.message as message

outgoing_port = 2010
incoming_port = 2011
remote_address = "127.0.0.1"
my_id = 1
other_id = 2

def proccess_msg(sender,address,msg,length,receiver_id=None, component_id=None):
    print("Got message from %i %s [%d Bytes]: %s" % (sender, address, length, msg))
    

udp = pprzlink.udp.UdpMessagesInterface(callback = proccess_msg, uplink_port = outgoing_port, downlink_port = incoming_port, interface_id = my_id)


try:
    udp.start()

    ping = message.PprzMessage('datalink', 'PING')

    while True:
        print ("Sending %s to %s:%d (%d)" % (ping,remote_address,outgoing_port,other_id))
        udp.send(ping, my_id, remote_address, other_id)
        time.sleep(2)

except KeyboardInterrupt:
	udp.stop()



