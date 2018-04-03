#!/usr/bin/env python

import sys

sys.path.append("/home/garciafa/Documents/PROG/paparazzi_messages/pprzlink/lib/v2.0/python/")

import pprzlink.serial
import pprzlink.messages_xml_map as messages_xml_map
import pprzlink.message as message
import time

import argparse

class SerialTutorial:
    """
    Class SerialTutorial that uses pprzlink.serial.SerialMessagesInterface to send
    PING messages on a serial device and monitors incoming messages.
    It respond to PING messages with PONG messages.
    """

    # Construction of the SerialTutorial object
    def __init__(self,args):
        self.serial_interface = pprzlink.serial.SerialMessagesInterface(
                            callback = self.process_incoming_message,   # callback function
                            device = args.dev,                          # serial device
                            baudrate = args.baud,                       # baudrate
                            interface_id = args.ac_id,                     # id of the aircraft
                            )
        self.ac_id = args.ac_id 
        self.baudrate = args.baud 

    # Main loop of the tutorial
    def run(self):
        print("Starting serial interface on %s at %i baud" % (args.dev, self.baudrate))
        
        try:
            self.serial_interface.start()

            # give the thread some time to properly start
            time.sleep(0.1)


            while self.serial_interface.isAlive():
                self.serial_interface.join(1)

                # create a ping message
                ping = message.PprzMessage('datalink', 'PING')

                # send a ping message to ourselves
                print("Sending ping")
                self.serial_interface.send(ping, self.ac_id,self.ac_id)

        except (KeyboardInterrupt, SystemExit):
            print('Shutting down...')
            self.serial_interface.stop()
            exit()

    # Callback function that process incoming messages
    def process_incoming_message (self, source, pprz_message):
        print("Received message from %i: %s" % (source, pprz_message))
        if pprz_message.name == "PING":
            print ("Sending back PONG")
            pong = message.PprzMessage('telemetry', 'PONG')
            self.serial_interface.send(pong, self.ac_id,source)


if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--device", help="device name", dest='dev', default='/dev/ttyUSB0')
    parser.add_argument("-b", "--baudrate", help="baudrate", dest='baud', default=115200, type=int)
    parser.add_argument("-id", "--ac_id", help="aircraft id (receiver)", dest='ac_id', default=42, type=int)
    args = parser.parse_args()


    serialTutorial = SerialTutorial(args)

    serialTutorial.run()


