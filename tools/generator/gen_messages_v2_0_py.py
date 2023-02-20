#!/usr/bin/env python3
'''
parse a PPRZLink protocol XML file and generate a Python implementation
for version 2.0 of the protocol
'''

from __future__ import print_function
import os, io
import pprz_template, pprz_parse

t = pprz_template.PPRZTemplate()

def generate_imports(file:io.TextIOWrapper) -> None:
    imports = """
from pprzlink.message import PprzMessage
import typing
    """
    file.write(imports)

def generate_one(file, xml:pprz_parse.PPRZXML, m:pprz_parse.PPRZMsg) -> None:
    template = """ 
class PprzMessage_${msg_name}(PprzMessage):
    def __init__(self,component_id=0):
        super().__init__('${class_name}', '${msg_name}', component_id)
    
    ${{fields:
    @property
    def ${field_name}_(self) -> ${py_type}:
        return self['${field_name}']
        
    @${field_name}_.setter
    def ${field_name}_(self,value:${py_type}) -> None:
        self['${field_name}'] = value
    }}
            """
    
    t.write(file,template,{'msg_name': m.msg_name, 'class_name' : xml.class_name, 'fields': m.fields})
    
def generate(output:str, xml:pprz_parse.PPRZXML):
    '''generate complete Python statically parsed interface'''

    directory, name = os.path.split(output)
    if directory != '':
        pprz_parse.mkdir_p(directory)

    file = open(os.path.join(directory,xml.class_name+".py"), mode='w')
    generate_imports(file)
    for m in xml.message:
        generate_one(file,xml,m)
