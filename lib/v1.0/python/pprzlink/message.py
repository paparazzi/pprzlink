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
Paparazzi message representation

"""

from __future__ import division, print_function
import sys
import json
import struct
import re
from . import messages_xml_map


class PprzMessageError(Exception):
    def __init__(self, message, inner_exception=None):
        self.message = message
        self.inner_exception = inner_exception
        self.exception_info = sys.exc_info()

    def __str__(self):
        return self.message


class PprzMessage(object):
    """base Paparazzi message class"""

    def __init__(self, class_name, msg):
        self._class_name = class_name
        if isinstance(msg, int):
            self._id = msg
            self._name = messages_xml_map.get_msg_name(class_name, msg)
        else:
            self._name = msg
            self._id = messages_xml_map.get_msg_id(class_name, msg)
        self._fieldnames = messages_xml_map.get_msg_fields(class_name, self._name)
        self._fieldtypes = messages_xml_map.get_msg_fieldtypes(class_name, self._id)
        self._fieldcoefs = messages_xml_map.get_msg_fieldcoefs(class_name, self._id)
        self._fieldvalues = []
        # set empty values according to type
        for t in self._fieldtypes:
            if t == "char[]":
                self._fieldvalues.append('')
            elif '[' in t:
                self._fieldvalues.append([0])
            else:
                self._fieldvalues.append(0)
        if messages_xml_map.message_dictionary_broadcast[self._name]=='forwarded':
            self.broadcasted = False
        else:
            self.broadcasted = True

    @property
    def name(self):
        """Get the message name."""
        return self._name

    @property
    def msg_id(self):
        """Get the message id."""
        return self._id

    @property
    def msg_class(self):
        """Get the message class."""
        return self._class_name

    @property
    def fieldnames(self):
        """Get list of field names."""
        return self._fieldnames

    @property
    def fieldvalues(self):
        """Get list of field values."""
        return self._fieldvalues

    @property
    def fieldtypes(self):
        """Get list of field types."""
        return self._fieldtypes

    @property
    def fieldcoefs(self):
        """Get list of field coefs."""
        return self._fieldcoefs

    def fieldbintypes(self, t):
        """Get type and length for binary format"""
        data_types = {
            'float': ['f', 4],
            'double': ['d', 8],
            'uint8': ['B', 1],
            'uint16': ['H', 2],
            'uint32': ['L', 4],
            'int8': ['b', 1],
            'int16': ['h', 2],
            'int32': ['l', 4],
            'char': ['c', 1]
        }
        base_type = t.split('[')[0]
        return data_types[base_type]

    def get_field(self, idx):
        """Get field value by index."""
        return self._fieldvalues[idx]

    def __getattr__(self, attr):
        # Try to dynamically return the field value for the given name
        for idx, f in enumerate(self.fieldnames):
            if f == attr:
                return self.fieldvalues[idx]
        raise AttributeError("No such attribute %s" % attr)

    def __getitem__(self, key):
        # Try to dynamically return the field value for the given name
        for idx, f in enumerate(self.fieldnames):
            if f == key:
                return self.fieldvalues[idx]
        raise AttributeError("Msg %s has no field of name %s" % (self.name, key))

    def __setitem__(self, key, value):
        self.set_value_by_name(key, value)

    def set_values(self, values):
        # print("msg %s: %s" % (self.name, ", ".join(self.fieldnames)))
        if len(values) == len(self.fieldnames):
            # for idx in range(len(values)):
            self._fieldvalues = values
        else:
            raise PprzMessageError("Error: Msg %s has %d fields, tried to set %i values" %
                                   (self.name, len(self.fieldnames), len(values)))

    def set_value_by_name(self, name, value):
        # Try to set a value from its name
        for idx, f in enumerate(self.fieldnames):
            if f == name:
                self._fieldvalues[idx] = value
                return
        raise AttributeError("Msg %s has no field of name %s" % (self.name, name))

    def __str__(self):
        ret = '%s.%s {' % (self.msg_class, self.name)
        for idx, f in enumerate(self.fieldnames):
            ret += '%s : %s, ' % (f, self.fieldvalues[idx])
        ret = ret[0:-2] + '}'
        return ret

    def to_dict(self, payload_only=False):
        d = {}
        if not payload_only:
            d['msgname'] = self.name
            d['msgclass'] = self.msg_class
        for idx, f in enumerate(self.fieldnames):
            d[f] = self.fieldvalues[idx]
        return d

    def to_json(self, payload_only=False):
        return json.dumps(self.to_dict(payload_only))

    def to_csv(self, payload_only=False):
        """ return message as CSV string for use with RAW_DATALINK
        msg_name;field1;field2;
        """
        return str(self.name) + ';' + self.payload_to_ivy_string(sep=';')

    def payload_to_ivy_string(self, sep=' '):
        fields = []
        for idx, t in enumerate(self.fieldtypes):
            if "char[" in t:
                str_value =''
                for c in self.fieldvalues[idx]:
                    str_value += c
                fields.append('"' + str_value + '"')
            elif '[' in t:
                fields.append(','.join([str(x) for x in self.fieldvalues[idx]]))
            else:
                fields.append(str(self.fieldvalues[idx]))
            ivy_str = sep.join(fields)
        return ivy_str

    def ivy_string_to_payload(self, data):
        """
        parse Ivy data string to PPRZ values
        header and message name should have been removed

        Basically parts/args in string are separated by space, but char array can also contain a space:
        ``|f,o,o, ,b,a,r|`` in old format or ``"foo bar"`` in new format

        """
        # first split on array delimiters
        # then slip on spaces and remove empty stings
        values = []
        for el in re.split('([|\"][^|\"]*[|\"])', data):
            if '|' not in el and '"' not in el:
                # split non-array strings further up
                for e in [d for d in el.split(' ') if d != '']:
                    if ',' in e:
                        # array but not a string
                        values.append([x for x in e.split(',') if x != ''])
                    else:
                        # not an array
                        values.append(e)
            else:
                # add string array (stripped)
                values.append(str.strip(el))
        self.set_values(values)

    def payload_to_binary(self):
        struct_string = "<"
        data = []
        length = 0
        for idx, t in enumerate(self.fieldtypes):
            bin_type = self.fieldbintypes(t)
            r = re.compile('[\[\]]')
            s = r.split(t)
            if len(s) > 1: # this is an array
                array_length = len(self.fieldvalues[idx])
                if len(s[1]) == 0: # this is a variable length array
                    struct_string += 'B'
                    data.append(array_length)
                    length += 1
                struct_string += bin_type[0] * array_length
                length += bin_type[1] * array_length
                for x in self.fieldvalues[idx]:
                    if bin_type[0]=='f' or bin_type[0]== 'd':
                        data.append(float(x))
                    elif bin_type[0]== 'B' or bin_type[0]== 'H' or bin_type[0]== 'L' or bin_type[0]== 'b' or bin_type[0]== 'h' or bin_type[0]== 'l':
                        data.append(int(x))
                    else:
                        data.append(x)
            else:
                struct_string += bin_type[0]
                length += bin_type[1]
                # Assign the right type according to field description
                if bin_type[0]=='f' or bin_type[0]== 'd':
                    data.append(float(self.fieldvalues[idx]))
                elif bin_type[0]== 'B' or bin_type[0]== 'H' or bin_type[0]== 'L' or bin_type[0]== 'b' or bin_type[0]== 'h' or bin_type[0]== 'l':
                    data.append(int(self.fieldvalues[idx]))
                else:
                    data.append(self.fieldvalues[idx])
        msg = struct.pack(struct_string, *data)
        return msg

    def binary_to_payload(self, data):
        msg_offset = 0
        values = []
        for idx, t in enumerate(self.fieldtypes):
            bin_type = self.fieldbintypes(t)
            if '[' in t:
                array_length = data[msg_offset]
                msg_offset += 1
                array_value = []
                for count in range(0, array_length):
                    array_value.append(struct.unpack('<' + bin_type[0], data[msg_offset:msg_offset + bin_type[1]])[0])
                    msg_offset = msg_offset + bin_type[1]
                values.append(array_value)
            else:
                value = struct.unpack('<' + bin_type[0], data[msg_offset:msg_offset + bin_type[1]])[0]
                msg_offset = msg_offset + bin_type[1]
                values.append(value)
        self.set_values(values)


def test():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", help="path to messages.xml file")
    parser.add_argument("-c", "--class", help="message class", dest="msg_class", default="telemetry")
    args = parser.parse_args()
    messages_xml_map.parse_messages(args.file)
    messages = [PprzMessage(args.msg_class, n) for n in messages_xml_map.get_msgs(args.msg_class)]
    print("Listing %i messages in '%s' msg_class" % (len(messages), args.msg_class))
    for msg in messages:
        print(msg)


if __name__ == '__main__':
    test()
