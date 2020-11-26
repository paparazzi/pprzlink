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

from __future__ import absolute_import, division

from ivy.std_api import *
from ivy.ivy import IvyIllegalStateError
import logging
import os
import re
import platform

from pprzlink.message import PprzMessage
from pprzlink import messages_xml_map
from pprzlink.request_uid import RequestUIDFactory


if os.getenv('IVY_BUS') is not None:
    IVY_BUS = os.getenv('IVY_BUS')
elif platform.system() == 'Darwin':
    IVY_BUS = "224.255.255.255:2010"
else:
    IVY_BUS = ""

logger = logging.getLogger("PprzLink")


class IvyMessagesInterface(object):
    """
    This class is the interface between the paparazzi messages and the Ivy bus.
    """
    def __init__(self, agent_name=None, start_ivy=True, verbose=False, ivy_bus=IVY_BUS):
        if agent_name is None:
            agent_name = "IvyMessagesInterface %i" % os.getpid()
        self.agent_name = agent_name
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
            logger.error(e)

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
        self.bindings = {}

    def shutdown(self):
        try:
            self.unsubscribe_all()
            self.stop()
        except IvyIllegalStateError as e:
            logger.error(e)

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

    def subscribe(self, callback, regex_or_msg='(.*)'):
        """
        Subscribe to Ivy message matching regex and call callback with ac_id and PprzMessage

        :param callback: function called on new message with ac_id and PprzMessage as params
        :param regex_or_msg: regular expression for matching message or a PprzMessage object to subscribe to
        """
        if not isinstance(regex_or_msg,PprzMessage):
            regex = regex_or_msg
        else:
            regex = '^([^ ]* +%s( .*|$))' % (regex_or_msg.name)

        def _parse_and_call_callback(agent, *larg):
            params = self.parse_pprz_msg(larg[0])
            if not params:
                return
            ac_id, _, msg = params
            callback(ac_id, msg)

        return self.bind_raw(
            callback=_parse_and_call_callback,
            regex=regex
        )

    def subscribe_request_answerer(self, callback, request_name):
        """
        Subscribe to advanced request messages.

        :param callback: Should return the answer as a PprzMessage
        :param request_name: Request message name to listen to (without `_REQ` suffix)
        :type callback: Callable[[int, PprzMessage], PprzMessage]
        :type request_name: str
        :return: binding id
        """
        regex = r'^(\S*\s+\S*\s+%s_REQ.*)' % request_name
        def _callback_wrapper(_, *larg):
            params = self.parse_pprz_msg(larg[0])
            if not params:
                return
            ac_id, request_id, msg = params
            try:
                msg = callback(ac_id, msg)
            except Exception:
                logger.error('Error while answering a request message')
                import traceback
                traceback.print_exc()
            else:
                self.send(" ".join((
                    request_id, msg.msg_class, request_name, msg.payload_to_ivy_string()
                )))
        return self.bind_raw(
            callback=_callback_wrapper,
            regex=regex
        )

    def unsubscribe(self, bind_id):
        self.unbind(bind_id)

    @staticmethod
    def parse_pprz_msg(ivy_msg):
        """
        Parse an Ivy message into a PprzMessage.

        :param ivy_msg: Ivy message string to parse into PprzMessage
        :return ac_id, request_id, msg: The parameters to be passed to callback
        """
        # normal format is "sender_name msg_name msg_payload..."
        # advanced format has requests and answers (with request_id as 'pid_index')
        # request: "sender_name request_id msg_name_REQ msg_payload..."
        # answer:  "request_id sender_name msg_name msg_payload..."

        data = re.search("(\S+) +(\S+) +(.*)", ivy_msg)
        # check for request_id in first or second string (-> advanced format with msg_name in third string)
        if data is None:
            return
        if re.search("[0-9]+_[0-9]+", data.group(1)) or re.search("[0-9]+_[0-9]+", data.group(2)):
            if re.search("[0-9]+_[0-9]+", data.group(1)):
                sender_name = data.group(2)
                request_id = data.group(1)
            else:
                sender_name = data.group(1)
                request_id = data.group(2)
            # this is an advanced type, split again
            data = re.search("(\S+)+( .*|$)", data.group(3))
            msg_name = data.group(1)
            payload = data.group(2)
        else:
            # this was a normal message
            sender_name = data.group(1)
            msg_name = data.group(2)
            payload = data.group(3)
            request_id = None
        # check which message class it is
        try:
            msg_class, msg_name = messages_xml_map.find_msg_by_name(msg_name)
        except ValueError:
            logger.error("Ignoring unknown message " + ivy_msg)
            return

        msg = PprzMessage(msg_class, msg_name)
        msg.ivy_string_to_payload(payload)
        # pass non-telemetry messages with ac_id 0 or ac_id attrib value
        if msg_class == "telemetry":
            try:
                if(sender_name[0:6] == 'replay'):
                    ac_id = int(sender_name[6:])
                else:
                    ac_id = int(sender_name)
            except ValueError:
                logger.warning("ignoring message " + ivy_msg)
                return None
        else:
            if 'ac_id' in msg.fieldnames:
                ac_id_idx = msg.fieldnames.index('ac_id')
                ac_id = msg.fieldvalues[ac_id_idx]
            else:
                ac_id = 0
        # finally call the callback, passing the aircraft id, request id (might be None) and parsed message
        return ac_id, request_id, msg

    def send_raw_datalink(self, msg):
        """
        Send a PprzMessage of datalink msg_class embedded in RAW_DATALINK message

        :param msg: PprzMessage
        :returns: Number of clients the message was sent to
        :raises: ValueError: if msg was invalid
        :raises: RuntimeError: if the server is not running
        """
        if not isinstance(msg, PprzMessage):
            raise ValueError("Expected msg to be PprzMessage, got " + type(msg) + " instead.")

        if "datalink" not in msg.msg_class:
            raise ValueError("Message to embed in RAW_DATALINK needs to be of 'datalink' class.")

        raw = PprzMessage("ground", "RAW_DATALINK")
        raw['ac_id'] = msg['ac_id']
        raw['message'] = msg.to_csv()
        return self.send(raw)

    def send(self, msg, sender_id=None, receiver_id=None, component_id=None):
        """
        Send a message

        :param msg: PprzMessage or simple string
        :param sender_id: Needed if sending a PprzMessage of telemetry msg_class, otherwise
                          message class might be used instead
        :returns: Number of clients the message was sent to
        :raises: ValueError: if msg was invalid or `sender_id` not provided for telemetry messages
        :raises: RuntimeError: if the server is not running
        """
        if not self._running:
            raise RuntimeError("Ivy server not running!")

        if isinstance(msg, PprzMessage):
            if "telemetry" in msg.msg_class:
                if sender_id is None:
                    raise ValueError("ac_id needed to send telemetry message.")
                else:
                    return IvySendMsg("%d %s %s" % (sender_id, msg.name, msg.payload_to_ivy_string()))
            else:
                if sender_id is None:
                    return IvySendMsg("%s %s %s" % (msg.msg_class, msg.name, msg.payload_to_ivy_string()))
                else:
                    return IvySendMsg("%s %s %s" % (str(sender_id), msg.name, msg.payload_to_ivy_string()))
        else:
            return IvySendMsg(msg)

    def send_request(self, class_name, request_name, callback, **request_extra_data):
        """
        Send a data request message and passes the result directly to the callback method.

        :return: Number of clients this message was sent to.
        :rtype: int
        :param class_name: Message class, the same as :ref:`PprzMessage.__init__`
        :param request_name: Request name (without the _REQ suffix)
        :param callback: Callback function that accepts two parameters: 1. aircraft id as int 2. The response message
        :param request_extra_data: Payload that will be sent with the request if any
        :type class_name: str
        :type request_name: str
        :type callback: Callable[[str, PprzMessage], Any]
        :type request_extra_data: Dict[str, Any]
        :raises: ValueError: if msg was invalid or `sender_id` not provided for telemetry messages
        :raises: RuntimeError: if the server is not running
        """
        new_id = RequestUIDFactory.generate_uid()
        regex = r"^((\S*\s*)?%s %s %s( .*|$))" % (new_id, class_name, request_name)

        def data_request_callback(ac_id, msg):
            try:
                callback(ac_id, msg)
            except Exception as e:
                raise e
            finally:
                self.unsubscribe(binding_id)

        binding_id = self.subscribe(data_request_callback, regex)
        request_message = PprzMessage(class_name, "%s_REQ" % request_name)
        for k, v in request_extra_data.items():
            request_message.set_value_by_name(k, v)

        data_request_message = ' '.join((
            self.agent_name, new_id, request_message.name, request_message.payload_to_ivy_string()
        ))
        return self.send(data_request_message)
