from __future__ import absolute_import, division, print_function

from ivy.std_api import *
from ivy.ivy import IvyIllegalStateError
import logging
import os
import sys
import re
import platform

from .message import PprzMessage
from . import messages_xml_map


if os.getenv('IVY_BUS') is not None:
    IVY_BUS = os.getenv('IVY_BUS')
elif platform.system() == 'Darwin':
    IVY_BUS = "224.255.255.255:2010"
else:
    IVY_BUS = ""


class IvyMessagesInterface(object):
    def __init__(self, agent_name=None, start_ivy=True, verbose=False, ivy_bus=IVY_BUS):
        if agent_name is None:
            agent_name = "IvyMessagesInterface %i" % os.getpid()
        self.verbose = verbose
        self._ivy_bus = ivy_bus
        self._running = False

        # make sure all messages are parsed before we start creating them in callbacks
        # the message parsing should really be redone...
        messages_xml_map.parse_messages()

        # bindings with associated callback functions
        self.bindings = {}

        IvyInit(agent_name, "READY")
        logging.getLogger('Ivy').setLevel(logging.WARN)
        if start_ivy:
            self.start()

    def __del__(self):
        try:
            self.shutdown()
        except Exception as e:
            print(e)

    def start(self):
        if not self._running:
            IvyStart(self._ivy_bus)
            self._running = True

    def stop(self):
        if self._running:
            self._running = False
            IvyStop()

    def unsubscribe_all(self):
        for b in self.bindings.keys():
            IvyUnBindMsg(b)
            del self.bindings[b]

    def shutdown(self):
        try:
            self.unsubscribe_all()
            self.stop()
        except IvyIllegalStateError as e:
            print(e)

    def bind_raw(self, callback, regex='(.*)'):
        """
        Bind callback to Ivy messages matching regex (without any extra parsing)

        :param callback: function called on new message with agent, message, from as params
        :param regex: regular expression for matching message
        """
        bind_id = IvyBindMsg(callback, regex)
        self.bindings[bind_id] = (callback, regex)
        return bind_id

    def unbind(self, bind_id):
        if bind_id in self.bindings:
            IvyUnBindMsg(bind_id)
            del self.bindings[bind_id]

    def subscribe(self, callback, regex='(.*)'):
        """
        Subscribe to Ivy message matching regex and call callback with ac_id and PprzMessage
        TODO: possibility to directly specify PprzMessage instead of regex

        :param callback: function called on new message with ac_id and PprzMessage as params
        :param regex: regular expression for matching message
        """
        bind_id = IvyBindMsg(lambda agent, *larg: self.parse_pprz_msg(callback, larg[0]), regex)
        self.bindings[bind_id] = (callback, regex)
        return bind_id

    def unsubscribe(self, bind_id):
        self.unbind(bind_id)

    @staticmethod
    def parse_pprz_msg(callback, ivy_msg):
        """
        Parse an Ivy message into a PprzMessage.
        Basically parts/args in string are separated by space, but char array can also contain a space:
        ``|f,o,o, ,b,a,r|`` in old format or ``"foo bar"`` in new format

        :param callback: function to call with ac_id and parsed PprzMessage as params
        :param ivy_msg: Ivy message string to parse into PprzMessage
        """
        # first split on array delimiters
        l = re.split('([|\"][^|\"]*[|\"])', ivy_msg)
        # strip spaces and filter out emtpy strings
        l = [str.strip(s) for s in l if str.strip(s) is not '']
        data = []
        for s in l:
            # split non-array strings further up
            if '|' not in s and '"' not in s:
                data += s.split(' ')
            else:
                data.append(s)
        # ignore ivy message with less than 3 elements
        if len(data) < 3:
            return

        # normal format is "sender_name msg_name msg_payload..."
        # advanced format has requests and answers (with request_id as 'pid_index')
        # request: "sender_name request_id msg_name_REQ msg_payload..."
        # answer:  "request_id sender_name msg_name msg_payload..."

        # check for request_id in first or second string (-> advanced format with msg_name in third string)
        advanced = False
        if re.search("[0-9]+_[0-9]+", data[0]) or re.search("[0-9]+_[0-9]+", data[1]):
            advanced = True
        msg_name = data[2] if advanced else data[1]
        # check which message class it is
        msg_class, msg_name = messages_xml_map.find_msg_by_name(msg_name)
        if msg_class is None:
            print("Ignoring unknown message " + ivy_msg)
            return
        # pass non-telemetry messages with ac_id 0
        if msg_class == "telemetry":
            try:
                sdata = data[0]
                if(sdata[0:6] == 'replay'):
                    ac_id = int(sdata[6:])
                else:
                    ac_id = int(sdata)
            except ValueError:
                print("ignoring message " + ivy_msg)
                sys.stdout.flush()
        else:
            ac_id = 0
        payload = data[3:] if advanced else data[2:]
        values = list(filter(None, payload))
        msg = PprzMessage(msg_class, msg_name)
        msg.set_values(values)
        # finally call the callback, passing the aircraft id and parsed message
        callback(ac_id, msg)

    def send_raw_datalink(self, msg):
        """
        Send a PprzMessage of datalink msg_class embedded in RAW_DATALINK message

        :param msg: PprzMessage
        :returns: Number of clients the message sent to, None if msg was invalid
        """
        if not isinstance(msg, PprzMessage):
            print("Can only send PprzMessage")
            return None
        if "datalink" not in msg.msg_class:
            print("Message to embed in RAW_DATALINK needs to be of 'datalink' class")
            return None
        raw = PprzMessage("ground", "RAW_DATALINK")
        raw['ac_id'] = msg['ac_id']
        raw['message'] = msg.to_csv()
        return self.send(raw)

    def send(self, msg, ac_id=None):
        """
        Send a message

        :param msg: PprzMessage or simple string
        :param ac_id: Needed if sending a PprzMessage of telemetry msg_class
        :returns: Number of clients the message sent to, None if msg was invalid
        """
        if not self._running:
            print("Ivy server not running!")
            return
        if isinstance(msg, PprzMessage):
            if "telemetry" in msg.msg_class:
                if ac_id is None:
                    print("ac_id needed to send telemetry message.")
                    return None
                else:
                    return IvySendMsg("%d %s %s" % (ac_id, msg.name, msg.payload_to_ivy_string()))
            else:
                return IvySendMsg("%s %s %s" % (msg.msg_class, msg.name, msg.payload_to_ivy_string()))
        else:
            return IvySendMsg(msg)
