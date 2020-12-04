#!/usr/bin/env python
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


from __future__ import absolute_import, print_function

import os

# if PAPARAZZI_HOME is set use $PAPARAZZI_HOME/var/messages.xml
# else assume this file is installed in var/lib/python/pprzlink
# and use messages.xml from var
# Message definition file should be installed at the same
# time as this file so it should be ok
PPRZ_HOME = os.getenv("PAPARAZZI_HOME")
if PPRZ_HOME is not None:
    default_messages_file = '%s/var/messages.xml' % PPRZ_HOME
else:
    default_messages_file = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                          "../../../../message_definitions/v1.0/messages.xml"))
# Define the pprzlink protocol version
PROTOCOL_VERSION="2.0"

message_dictionary = {}
message_dictionary_types = {}
message_dictionary_coefs = {}
message_dictionary_id_name = {}
message_dictionary_name_id = {}
message_dictionary_class_id_name = {}
message_dictionary_class_name_id = {}
message_dictionary_broadcast = {}



class MessagesNotFound(Exception):
    def __init__(self, filename):
        self.filename = filename

    def __str__(self):
        return "messages file " + repr(self.filename) + " not found"


def parse_messages(messages_file=''):
    if not messages_file:
        messages_file = default_messages_file
    if not os.path.isfile(messages_file):
        raise MessagesNotFound(messages_file)
    #print("Parsing %s" % messages_file)
    from lxml import etree
    tree = etree.parse(messages_file)
    for the_class in tree.xpath("//msg_class[@name]"):
        class_name = the_class.attrib['name']
        if 'id' in the_class.attrib:
            class_id = int(the_class.attrib['id'])
            message_dictionary_class_id_name[class_id] = class_name
            message_dictionary_class_name_id[class_name] = class_id
        elif 'ID' in the_class.attrib:
            class_id = int(the_class.attrib['ID'])
            message_dictionary_class_id_name[class_id] = class_name
            message_dictionary_class_name_id[class_name] = class_id

        if class_name not in message_dictionary:
            message_dictionary_id_name[class_name] = {}
            message_dictionary_name_id[class_name] = {}
            message_dictionary[class_name] = {}
            message_dictionary_types[class_name] = {}
            message_dictionary_coefs[class_name] = {}
        for the_message in the_class.xpath("message[@name]"):
            message_name = the_message.attrib['name']
            if 'id' in the_message.attrib:
                message_id = the_message.attrib['id']
            else:
                message_id = the_message.attrib['ID']
            if message_id[0:2] == "0x":
                message_id = int(message_id, 16)
            else:
                message_id = int(message_id)

            if 'link' in the_message.attrib:
                message_dictionary_broadcast[message_name] = the_message.attrib['link']
            else:
                message_dictionary_broadcast[message_name] = 'forwarded' # Default behavior is to send message to destination only

            message_dictionary_id_name[class_name][message_id] = message_name
            message_dictionary_name_id[class_name][message_name] = message_id

            # insert this message into our dictionary as a list with room for the fields
            message_dictionary[class_name][message_name] = []
            message_dictionary_types[class_name][message_id] = []
            message_dictionary_coefs[class_name][message_id] = []

            for the_field in the_message.xpath('field[@name]'):
                # for now, just save the field names -- in the future maybe expand this to save a struct?
                message_dictionary[class_name][message_name].append(the_field.attrib['name'])
                message_dictionary_types[class_name][message_id].append(the_field.attrib['type'])
                try:
                    message_dictionary_coefs[class_name][message_id].append(float(the_field.attrib['alt_unit_coef']))
                except KeyError:
                    # print("no such key")
                    message_dictionary_coefs[class_name][message_id].append(1.)


def _ensure_message_dictionary():
    if not message_dictionary:
        parse_messages()


def find_msg_by_name(name):
    _ensure_message_dictionary()
    for msg_class, msg_name in message_dictionary.items():
        if name in msg_name:
            #print("found msg name %s in class %s" % (name, msg_class))
            return msg_class, name

    raise ValueError("Error: msg_name %s not found." % name)


def get_msgs(msg_class):
    _ensure_message_dictionary()
    if msg_class not in message_dictionary:
        raise ValueError("Error: msg_class %s not found." % msg_class)

    return message_dictionary[msg_class]


def get_class_name(class_id):
    _ensure_message_dictionary()
    if class_id not in message_dictionary_class_id_name:
        raise ValueError("Error: class_id %d not found." % class_id)

    return message_dictionary_class_id_name[class_id]


def get_class_id(class_name):
    _ensure_message_dictionary()
    if class_name not in message_dictionary_class_name_id:
        raise ValueError("Error: class_name %s not found." % class_name)

    return message_dictionary_class_name_id[class_name]


def get_msg_name(msg_class, msg_id):
    _ensure_message_dictionary()
    if msg_class not in message_dictionary:
        raise ValueError("Error: msg_class %s not found." % msg_class)

    if msg_id not in message_dictionary_id_name[msg_class]:
        raise ValueError("Error: msg_id %d not found in msg_class %s." % (msg_id, msg_class))

    return message_dictionary_id_name[msg_class][msg_id]


def get_msg_fields(msg_class, msg_name):
    _ensure_message_dictionary()
    if msg_class not in message_dictionary:
        raise ValueError("Error: msg_class %s not found." % msg_class)

    if msg_name not in message_dictionary[msg_class]:
        raise ValueError("Error: msg_name %s not found in msg_class %s." % (msg_name, msg_class))

    return message_dictionary[msg_class][msg_name]


def get_msg_id(msg_class, msg_name):
    _ensure_message_dictionary()
    if msg_class not in message_dictionary_name_id:
        raise ValueError("Error: msg_class %s not found." % msg_class)

    if msg_name not in message_dictionary_name_id[msg_class]:
        raise ValueError("Error: msg_name %s not found in msg_class %s." % (msg_name, msg_class))

    return message_dictionary_name_id[msg_class][msg_name]


def get_msg_fieldtypes(msg_class, msg_id):
    _ensure_message_dictionary()

    if msg_class not in message_dictionary_types:
        raise ValueError("Error: msg_class %s not found." % msg_class)

    if msg_id not in message_dictionary_types[msg_class]:
        raise ValueError("Error: message with ID %d not found in msg_class %s." % (msg_id, msg_class))

    return message_dictionary_types[msg_class][msg_id]


def get_msg_fieldcoefs(msg_class, msg_id):
    _ensure_message_dictionary()

    if msg_class not in message_dictionary_coefs:
        raise ValueError("Error: msg_class %s not found." % msg_class)

    if msg_id not in message_dictionary_coefs[msg_class]:
        raise ValueError("Error: message with ID %d not found in msg_class %s." % (msg_id, msg_class))

    return message_dictionary_coefs[msg_class][msg_id]


def test():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", help="path to messages.xml file")
    parser.add_argument("-l", "--list", help="list parsed messages", action="store_true", dest="list_messages")
    parser.add_argument("-c", "--class", help="message class", dest="msg_class", default="telemetry")
    args = parser.parse_args()
    parse_messages(args.file)
    if args.list_messages:
        print("Listing %i messages in '%s' msg_class" % (len(message_dictionary[args.msg_class]), args.msg_class))
        for msg_name, msg_fields in message_dictionary[args.msg_class].iteritems():
            print(msg_name + ": " + ", ".join(msg_fields))


if __name__ == '__main__':
    test()
