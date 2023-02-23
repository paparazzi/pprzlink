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
import typing
from pprzlink import messages_xml_map
from enum import EnumMeta
from dataclasses import dataclass


class PprzMessageError(Exception):
    def __init__(self, message, inner_exception=None):
        self.message = message
        self.inner_exception = inner_exception
        self.exception_info = sys.exc_info()

    def __str__(self):
        return self.message

@dataclass
class PprzMessageField(object):
    """
    Dataclass representing a field in a Paparazzi Ivy message
    
    Required attributes:
    - `name`: str
        Field's name
    - `typestr`: str
        Field's type in C, as a string
    
    Optional attributes:
    - `val`: Any
        Field's current value
    - `format`: str
        C-like format string for readable display
    - `unit`: str
        Field's unit
    - `values`: EnumMeta
        Enumeration of the possible values for `val`
    - `alt_unit`: str
        Alternative unit for the value. Mostly used to be human-friendly.
    - `alt_unit_coef`: float
        Conversion factor from base to alternative unit
    - `alt_val`: float
        Access `val` through its alternative unit, automatically
        using the conversion factor (if defined) 
    """     
    
    name:str
    typestr:str
    val:typing.Any = None
    format:typing.Optional[str] = None
    unit:typing.Optional[str] = None
    values:typing.Optional[EnumMeta] = None
    alt_unit:typing.Optional[str] = None
    alt_unit_coef:typing.Optional[float] = None
    
    @property
    def is_enum(self) -> bool:
        """
        Check if this field's values are enum-based
        """
        return isinstance(self.values,EnumMeta)
    
    @property
    def val_enum(self) -> str:
        """
        Access and modify `self.val` through the names in the enum `self.values`
        
        Fails is no enum is set for `self.values`
        (i.e. it is not an instance of `EnumMeta`)
        """
        assert self.is_enum
        return self.values(self.val).name
    
    @val_enum.setter
    def val_enum(self,enum_attr:str) -> None:
        assert self.is_enum
        self.val = self.values[enum_attr]
    
    @property
    def alt_val(self) -> typing.Optional[float]:
        if self.alt_unit_coef:
            return self.alt_unit_coef * self.val
        else:
            return None
        
    @alt_val.setter
    def alt_val(self,value:float) -> None:
        if self.alt_unit_coef:
            self.val = value / self.alt_unit_coef
        else:
            raise AttributeError("No conversion coefficient set")
        
    def python_typestring(self) -> str:
        if self.type == "float" or self.type == "double":
            basetype = "float"
        else:
            basetype = "int"
            
        if self.array_type is None:
            return basetype
        else:
            if self.type == "char":
                return "str"
            else:
                return f"typing.List[{basetype}]"
    
    def python_type(self) -> typing.Type:
        if self.type == "float" or self.type == "double":
            basetype = float
        else:
            basetype = int
            
        if self.array_type is None:
            return basetype
        else:
            if self.type == "char":
                return str
            else:
                return typing.List[{basetype}]
        
        
    
class PprzMessage(object):
    """base Paparazzi message class"""
    
    def __init__(self, class_name, msg, component_id=0):
        if isinstance(class_name, int):
            # class_name is an integer, find the name
            # TODO handle None case
            self._class_id = class_name
            self._class_name = messages_xml_map.get_class_name(self._class_id)
        else:
            self._class_name = class_name
            self._class_id = messages_xml_map.get_class_id(class_name)
        self._component_id = component_id
        if isinstance(msg, int):
            self._id = msg
            self._name = messages_xml_map.get_msg_name(self._class_name, msg)
        else:
            self._name = msg
            self._id = messages_xml_map.get_msg_id(self._class_name, msg)
        
        self._fields_order:typing.List[str] = messages_xml_map.get_msg_fields(self._class_name, self._name)
        _fieldtypes = messages_xml_map.get_msg_fieldtypes(self._class_name, self._id)
        _fieldcoefs = messages_xml_map.get_msg_fieldcoefs(self._class_name, self._id)
        _fieldvalues = []
        # set empty values according to type
        for t in _fieldtypes:
            if t == "char[]":
                _fieldvalues.append('')
            elif '[' in t:
                _fieldvalues.append([0])
            else:
                _fieldvalues.append(0)
        if messages_xml_map.message_dictionary_broadcast[self._name]=='forwarded':
            self.broadcasted = False
        else:
            self.broadcasted = True
            
            
        self._fields:typing.Dict[str,PprzMessageField] = dict()
        for i,n in enumerate(self._fields_order):
            self._fields[n] = PprzMessageField(n,_fieldtypes[i],val=_fieldvalues[i],alt_unit_coef=_fieldcoefs[i])

    @property
    def name(self) -> str:
        """Get the message name."""
        return self._name

    @property
    def msg_id(self) -> int:
        """Get the message id."""
        return self._id

    @property
    def class_id(self) -> int:
        """Get the class id."""
        return self._class_id

    @property
    def msg_class(self) -> str:
        """Get the message class."""
        return self._class_name

    @property
    def fieldnames(self) -> typing.List[str]:
        """Get list of field names."""
        return list(self._fields.keys())

    @property
    def fieldvalues(self) -> typing.List:
        """Get list of field values."""
        return list(f.val for f in self._fields.values())

    @property
    def fieldtypes(self) -> typing.List[str]:
        """Get list of field types."""
        return list(f.typestr for f in self._fields.values())

    @property
    def fieldcoefs(self) -> typing.List[float]:
        """Get list of field coefs."""
        return list(f.alt_unit_coef for f in self._fields.values())

    @staticmethod
    def fieldbintypes(t:str) -> typing.Tuple[str,int]:
        """Get type and length for binary format"""
        data_types = {
            'float': ('f', 4),
            'double': ('d', 8),
            'uint8': ('B', 1),
            'uint16': ('H', 2),
            'uint32': ('L', 4),
            'int8': ('b', 1),
            'int16': ('h', 2),
            'int32': ('l', 4),
            'char': ('c', 1)
        }
        base_type = t.split('[')[0]
        return data_types[base_type]

    def get_field(self, idx:int):
        """Get field value by index."""
        return self._fields[self._fields_order[idx]].val
    
    def get_full_field(self,name:str) -> PprzMessageField:
        """Get the underlying PprzMessageField object"""
        return self._fields[name]
    
    def set_full_field(self,name:str, field:PprzMessageField) -> None:
        """Set the underlying PprzMessageField object"""
        self._fields[name] = field

    # def __getattr__(self, attr:str):
    #     # Try to dynamically return the field value for the given name
    #     return self._fields[attr].val

    def __getitem__(self, key:str):
        # Try to dynamically return the field value for the given name
        return self._fields[key].val
    
    def __setitem__(self, key:str, value) -> None:
        self.set_value_by_name(key, value)

    def set_values(self, values:typing.List) -> None:
        """Set all values by index."""
        #print("msg %s: %s" % (self.name, ", ".join(self.fieldnames)))
        if len(values) == len(self._fields_order):
            for i,v in enumerate(values):
                self._fields[self._fields_order[i]].val = v
        else:
            raise PprzMessageError("Error: Msg %s has %d fields, tried to set %i values" %
                                   (self.name, len(self.fieldnames), len(values)))

    def set_value_by_name(self, name:str, value) -> None:
        # Try to set a value from its name
        self._fields[name].val = value
        
    def __str__(self) -> str:
        ret = '%s.%s {' % (self.msg_class, self.name)
        for k, f in self._fields.items():
            ret += '%s : %s, ' % (k,f.val)
        ret = ret + '}'
        return ret

    def to_dict(self, payload_only=False) -> typing.Dict[str,typing.Any]:
        d = {}
        if not payload_only:
            d['msgname'] = self.name
            d['msgclass'] = self.msg_class
        for k, f in self._fields.items():
            d[k] = f.val
        return d

    def to_json(self, payload_only=False) -> str:
        return json.dumps(self.to_dict(payload_only))

    def to_csv(self, payload_only=False) -> str:
        """ return message as CSV string for use with RAW_DATALINK
        msg_name;field1;field2;
        """
        return str(self.name) + ';' + self.payload_to_ivy_string(sep=';')

    def payload_to_ivy_string(self, sep=' ') -> str:
        fields = []
        for n in self._fields_order:
            f = self._fields[n]
            if "char[" in f.typestr:
                str_value =''
                for c in f.val:
                    if isinstance(c, bytes):
                        str_value += c.decode()
                    else:
                        str_value += c
                fields.append('"' + str_value + '"')
            elif '[' in f.typestr:
                fields.append(','.join([str(x) for x in f.val]))
            else:
                fields.append(str(f.val))
        ivy_str = sep.join(fields)
        return ivy_str

    def ivy_string_to_payload(self, data:str) -> None:
        """
        parse Ivy data string to PPRZ values
        header and message name should have been removed

        Basically parts/args in string are separated by space, but char array can also contain a space:
        ``|f,o,o, ,b,a,r|`` in old format or ``"foo bar"`` in new format

        """
        # first split on array delimiters
        # then split on spaces and remove empty stings
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

    def payload_to_binary(self) -> bytes:
        struct_string = "<"
        data = []
        length = 0
        r = re.compile('[\[\]]')
        for n in self._fields_order:
            f = self._fields[n]
            bin_type = self.fieldbintypes(f.typestr)
            s = r.split(f.typestr)
            if len(s) > 1: # this is an array
                array_length = len(f.val)
                if len(s[1]) == 0: # this is a variable length array
                    struct_string += 'B'
                    data.append(array_length)
                    length += 1
                struct_string += bin_type[0] * array_length
                length += bin_type[1] * array_length
                for x in f.val:
                    if bin_type[0]=='f' or bin_type[0]== 'd':
                        data.append(float(x))
                    elif bin_type[0]== 'B' or bin_type[0]== 'H' or bin_type[0]== 'L' or bin_type[0]== 'b' or bin_type[0]== 'h' or bin_type[0]== 'l':
                        data.append(int(x))
                    elif bin_type[0]== 'c':
                        data.append(x.encode())
                    else:
                        data.append(x)
            else:
                struct_string += bin_type[0]
                length += bin_type[1]
                # Assign the right type according to field description
                if bin_type[0]=='f' or bin_type[0]== 'd':
                    data.append(float(f.val))
                elif bin_type[0]== 'B' or bin_type[0]== 'H' or bin_type[0]== 'L' or bin_type[0]== 'b' or bin_type[0]== 'h' or bin_type[0]== 'l':
                    data.append(int(f.val))
                else:
                    data.append(f.val)
        msg = struct.pack(struct_string, *data)
        return msg

    def binary_to_payload(self, data:bytes) -> None:
        msg_offset = 0
        values = []
        r = re.compile('[\[\]]')
        for n in self._fields_order:
            f = self._fields[n]
            bin_type = self.fieldbintypes(f.typestr)
            s = r.split(f.typestr)
            if len(s) > 1: # this is an array
                if len(s[1]) == 0: # this is a variable length array
                    array_length = data[msg_offset]
                    msg_offset += 1
                else:
                    array_length = int(s[1])
                array_value = []
                for _ in range(0, array_length):
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
