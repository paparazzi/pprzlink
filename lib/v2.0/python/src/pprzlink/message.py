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

from __future__ import division, print_function, annotations
import sys
import json,csv
import struct
import re
import typing
import sys

try:
    import messages_xml_map 
except ImportError:
    from pprzlink import messages_xml_map

from enum import IntEnum,Enum
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
    - `values`: IntEnum
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
    values:typing.Optional[IntEnum] = None
    alt_unit:typing.Optional[str] = None
    alt_unit_coef:typing.Optional[float] = None
    
    @property
    def array_type(self) -> bool:
        return '[]' in self.typestr or 'string' in self.typestr
    
    @property
    def is_enum(self) -> bool:
        """
        Check if this field's values are enum-based
        """
        return isinstance(self.values,IntEnum)
    
    @property
    def val_enum(self) -> str:
        """
        Access and modify `self.val` through the names in the enum `self.values`
        
        Fails is no enum is set for `self.values`
        (i.e. it is not an instance of `IntEnum`)
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
        
    @property
    def __basetype(self) -> type:
        if "float" in self.typestr or "double" in self.typestr:
            return float
        else:
            return int
    
    @property
    def __basetype_str(self) -> str:
        if "float" in self.typestr or "double" in self.typestr:
            return "float"
        else:
            return "int"
    
    @property
    def python_typestring(self) -> str:
        basetype = self.__basetype_str
            
        if not(self.array_type):
            return basetype
        else:
            if "char" in self.typestr or self.typestr == "string":
                return "str"
            else:
                return f"typing.List[{basetype}]"
    
    @property
    def python_type(self) -> typing.Type:
        basetype = self.__basetype
            
        if not(self.array_type):
            return basetype
        else:
            if "char" in self.typestr or self.typestr == "string":
                return str
            else:
                return typing.List[{basetype}]

    @property        
    def python_simple_type(self) -> type:
        if "float" in self.typestr or  "double" in self.typestr:
            basetype = float
        else:
            basetype = int
            
        if not(self.array_type):
            return basetype
        else:
            if "char" in self.typestr or self.typestr == "string":
                return str
            else:
                return list
            
    def parse(self,strval:typing.Any) -> None:
        if self.is_enum:
            try:
                intval = int(strval)
                assert intval in [v.value for v in self.values]
                self.val = intval
            except (AssertionError,ValueError):
                try:
                    self.val_enum = strval
                except KeyError:
                    print(f"Warning: In field {self.name}, could not find {strval} in the enum:\n{[v for v in self.values]}\nFalling back to setting the value directly...",
                        file=sys.stderr)
                    self.val = strval
        else:
            try:
                        
                if self.python_simple_type == list:
                    if not(isinstance(strval,list)):
                        # If not a list, suppose it is a string with elements separated by commas
                        self.val = strval.split(',')
                    else:
                        self.val = strval                    
                    
                elif self.python_simple_type == str and (self.format == "csv" or self.format == ";sv"):
                    # Special treatment of CSV encoded strings: store them as list of strings instead of string
                    if self.format[0] == ';':
                        sep = ';'
                    else:
                        sep = ','
                    
                    if isinstance(strval,list):
                        self.val = strval
                    else:
                        # Remove one trailing separator, if it exists
                        if strval[-1] == sep:
                            strval = strval[:-1]
                            
                        # Parse using the Python CSV module
                        self.val = next(csv.reader([strval],delimiter=sep))
                else:
                    self.val = self.python_simple_type(strval)
                
                if self.array_type and self.python_simple_type != str:
                    for i,v in enumerate(self.val):
                        self.val[i] = self.__basetype(v)
                        
            except (TypeError,ValueError) as e:
                print(f"{self.name} : Tried to parse {strval} ({type(strval)}) using type {self.python_typestring}")
                raise e
                    
    
class PprzMessage(object):
    """base Paparazzi message class"""
    
    __slots__ = ('_class_id','_class_name','_component_id','_id','_name','_fields_order','broadcasted','_fields','_fields_typing_dict')
    
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
        
        _fieldvalues_enum = messages_xml_map.get_msg_values_enum(self._class_name, self._id)
        _fieldunits = messages_xml_map.get_msg_units(self._class_name, self._id)
        _fieldformats = messages_xml_map.get_msg_formats(self._class_name, self._id)
        _fieldalt_units = messages_xml_map.get_msg_alt_units(self._class_name, self._id)
        
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
            
            if _fieldvalues_enum[i] is not None:
                valcount:dict[str,int] = dict()
                new_names = []
                for s in _fieldvalues_enum[i]:
                    valcount.setdefault(s,0)
                    valcount[s] += 1
                    if valcount[s] == 1:
                        new_names.append(s)
                    else:
                        new_names.append(s+f"_{valcount[s]}")
                    
                
                fieldvalues_enum = Enum(f"{n}_ValuesEnum",new_names,start=0)
                
            else:
                fieldvalues_enum = None
            
            self._fields[n] = PprzMessageField(n,_fieldtypes[i],val=_fieldvalues[i],
                                               format=_fieldformats[i],
                                               unit=_fieldunits[i],
                                               values=fieldvalues_enum,
                                               alt_unit=_fieldalt_units[i],
                                               alt_unit_coef=_fieldcoefs[i])

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
    
    
    @property
    def fieldvalues_enum(self) -> typing.List[typing.Optional[IntEnum]]:
        """Get list of the values Enum (or None when there are no enum)"""
        return list(f.values for f in self._fields.values())

    @property
    def fieldunits(self) -> typing.List[typing.Optional[str]]:
        """Get list of the values units (or None when it is not specified)"""
        return list(f.unit for f in self._fields.values())

    @property
    def fieldformats(self) -> typing.List[typing.Optional[str]]:
        """Get list of the values formats (or None when it is not specified)"""
        return list(f.format for f in self._fields.values())

    @property
    def fieldalt_units(self) -> typing.List[typing.Optional[str]]:
        """Get list of the values alternative units (or None when it is not specified)"""
        return list(f.alt_unit for f in self._fields.values())


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

    def __getattr__(self, attr:str):
        # Try to dynamically return the field value for the given name
        return self._fields[attr].val

    def __getitem__(self, key:str):
        # Try to dynamically return the field value for the given name
        return self._fields[key].val
    
    def __setitem__(self, key:str, value) -> None:
        self.set_value_by_name(key, value)
        
    def parse_values(self, values:typing.List) -> None:
        if len(values) == len(self._fields_order):
            for i,v in enumerate(values):
                self._fields[self._fields_order[i]].parse(v)
        else:
            raise PprzMessageError("Error: Msg %s has %d fields, tried to set %i values" %
                                   (self.name, len(self.fieldnames), len(values)))

    def set_values(self, values:typing.List) -> None:
        """Set all values by index."""
        #print("msg %s: %s" % (self.name, ", ".join(self.fieldnames)))
        if len(values) == len(self._fields_order):
            for i,v in enumerate(values):
                self._fields[self._fields_order[i]].val = v
        else:
            raise PprzMessageError("Error: Msg %s has %d fields, tried to set %i values" %
                                   (self.name, len(self.fieldnames), len(values)))

    def parse_value_by_name(self, name:str, value) -> None:
        # Try to set then convert a value from its name
        self._fields[name].parse(value)

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
    
    @staticmethod
    def from_dict(d:typing.Dict[str,typing.Any]) -> PprzMessage:
        msg_name = d['msgname']
        msg_class = d['msgclass']
        
        pprz_msg = PprzMessage(msg_class,msg_name)
        for k,f in d.items():
            
            # Skip already processed keys
            if k == 'msgname' or k == 'msgclass':
                continue
            
            else:
                pprz_msg[k] = f
        return pprz_msg

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
                if isinstance(f.val,str):
                    str_value = f.val
                else:
                    str_value =''
                    try:
                        for c in f.val:
                            if isinstance(c, bytes):
                                str_value += c.decode()
                            else:
                                str_value += c
                    except TypeError:
                        str_value = str(f.val)
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
                    values.append(e)
            else:
                # add string array (stripped)
                values.append(str.strip(el))
        self.parse_values(values)

    def payload_to_binary(self) -> bytes:
        struct_string = "<"
        data = []
        length = 0
        r = re.compile(r'[\[\]]')
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
        r = re.compile(r'[\[\]]')
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
