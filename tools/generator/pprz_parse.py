#!/usr/bin/env python3
'''
mavlink python parse functions

Copyright (C) 2015 Gautier Hattenberger <gautier.hattenberger@enac.fr>
For the Paparazzi UAV and PPRZLINK projects

based on:
    Copyright Andrew Tridgell 2011
    Released under GNU GPL version 3 or later
'''

import xml.parsers.expat, os, errno, time, sys, operator, struct

PROTOCOL_1_0 = "1.0"
PROTOCOL_2_0 = "2.0"
MESSAGES_1_0 = "1.0"

class PPRZParseError(Exception):
    def __init__(self, message, inner_exception=None):
        self.message = message
        self.inner_exception = inner_exception
        self.exception_info = sys.exc_info()
    def __str__(self):
        return self.message

class PPRZField(object):
    def __init__(self, name, type, xml, description=''):
        self.field_name = name
        #self.name_upper = name.upper()
        self.description = description
        self.array_type = None
        self.array_length = ''
        self.array_extra_length = ''
        lengths = {
        'float'    : '4',
        'double'   : '8',
        'char'     : '1',
        'int8_t'   : '1',
        'uint8_t'  : '1',
        'int16_t'  : '2',
        'uint16_t' : '2',
        'int32_t'  : '4',
        'uint32_t' : '4',
        'int64_t'  : '8',
        'uint64_t' : '8',
        }

        aidx = type.find("[")
        if aidx != -1:
            assert type[-1:] == ']'
            if type[aidx+1] == ']':
                self.array_type = 'VariableArray'
                self.array_length = 'nb_'+name
                self.array_extra_length = '+1'
            else:
                self.array_type = 'FixedArray'
                self.array_length = type[aidx+1:-1]
            type = type[0:aidx]
        if type in lengths:
            self.type_length = lengths[type]
            self.type = type
        elif (type+"_t") in lengths:
            self.type_length = lengths[type+"_t"]
            self.type = type+'_t'
        else:
            raise PPRZParseError("unknown type '%s'" % type)
        if self.array_length != '':
            self.length = self.array_length+'*'+self.type_length
        else:
            self.length = self.type_length
        if self.type[-2:] == '_t':
            self.type_upper = self.type[0:-2].upper()
        else:
            self.type_upper = self.type.upper()


class PPRZMsg(object):
    def __init__(self, name, id, linenumber, description=''):
        self.msg_name = name
        self.linenumber = linenumber
        self.id = int(id)
        self.description = description
        self.fields = []
        self.fieldnames = []

class PPRZXML(object):
    '''parse a pprzlink XML file for a given class'''
    def __init__(self, filename, class_name, protocol_version=PROTOCOL_1_0):
        self.filename = filename
        self.class_name = class_name
        self.class_id= None
        self.message = []
        self.protocol_version = protocol_version

        if protocol_version == PROTOCOL_1_0:
            self.protocol_version_major = 1
            self.protocol_version_minor = 0
            self.generator_module = "gen_messages_v1_0"
        elif protocol_version == PROTOCOL_2_0:
            self.protocol_version_major = 2
            self.protocol_version_minor = 0
            self.generator_module = "gen_messages_v2_0"
        else:
            print("Unknown wire protocol version")
            print("Available versions are: %s" % (PROTOCOL_1_0))
            raise PPRZParseError('Unknown PPRZLink protocol version %s' % protocol_version)

        in_element_list = []
        self.current_class = ''
        self.current_class_id = None

        def check_attrs(attrs, check, where):
            for c in check:
                if not c in attrs:
                    raise PPRZParseError('expected missing %s "%s" attribute at %s:%u' % (
                        where, c, filename, p.CurrentLineNumber))

        def start_element(name, attrs):
            in_element_list.append(name)
            in_element = '.'.join(in_element_list)
            #print in_element
            if in_element == "protocol.msg_class":
                check_attrs(attrs, ['name'], 'msg_class')
                self.current_class = attrs['name']
                try:
                    check_attrs(attrs, ['id'], 'msg_class')
                    self.current_class_id = attrs['id']
                except PPRZParseError:
                    self.current_class_id = None
            elif in_element == "protocol.msg_class.message":
                check_attrs(attrs, ['name', 'id'], 'message')
                if self.current_class == self.class_name:
                    self.message.append(PPRZMsg(attrs['name'], attrs['id'], p.CurrentLineNumber))
            elif in_element == "protocol.msg_class.message.field":
                check_attrs(attrs, ['name', 'type'], 'field')
                if self.current_class == self.class_name:
                    self.message[-1].fields.append(PPRZField(attrs['name'], attrs['type'], self))

        def end_element(name):
            in_element = '.'.join(in_element_list)
            if in_element == "protocol.msg_class":
                if self.current_class == self.class_name:
                    self.nb_messages = len(self.message)
                    if self.current_class_id is not None:
                        self.class_id = int(self.current_class_id)
                    else:
                        raise PPRZParseError('Cannot generate code for class (%s) without id' % self.current_class)
                self.current_class = ''
            in_element_list.pop()

        def char_data(data):
            in_element = '.'.join(in_element_list)
            if in_element == "protocol.msg_class.message.description":
                if self.current_class == self.class_name:
                    self.message[-1].description += data
            elif in_element == "protocol.msg_class.message.field":
                if self.current_class == self.class_name:
                    self.message[-1].fields[-1].description += data

        f = open(filename, mode='rb')
        p = xml.parsers.expat.ParserCreate()
        p.StartElementHandler = start_element
        p.EndElementHandler = end_element
        p.CharacterDataHandler = char_data
        p.ParseFile(f)
        f.close()

        self.message_names = [ None ] * 256

        for m in self.message:
            m.length = 0
            m.fieldnames = []
            for f in m.fields:
                if f.array_type == 'VariableArray':
                    m.fieldnames.append('nb_'+f.field_name)
                m.fieldnames.append(f.field_name)
            m.num_fields = len(m.fieldnames)
            self.message_names[m.id] = m.msg_name

    def __str__(self):
        return "PPRZXML %s class from %s (%u message)" % (
            self.class_name, self.filename, len(self.message))


def check_duplicates(xml):
    '''check for duplicate message IDs'''

    msgmap = {}
    for m in xml.message:
        if m.id in msgmap:
            print("ERROR: Duplicate message id %u for %s (%s:%u) also used by %s" % (
                m.id, m.msg_name,
                xml.filename, m.linenumber,
                msgmap[m.id]))
            return True
        fieldset = set()
        for f in m.fields:
            if f.field_name in fieldset:
                print("ERROR: Duplicate field %s in message %s (%s:%u)" % (
                    f.field_name, m.msg_name,
                    xml.filename, m.linenumber))
                return True
            fieldset.add(f.field_name)
        msgmap[m.id] = '%s (%s:%u)' % (m.msg_name, xml.filename, m.linenumber)

    return False



def total_msgs(xml):
    '''count total number of msgs'''
    return len(xml.message)

def mkdir_p(dir):
    try:
        os.makedirs(dir)
    except OSError as exc:
        if exc.errno == errno.EEXIST:
            pass
        else: raise

# check version consistent
# add test.xml
# finish test suite
# printf style error macro, if defined call errors
