#!/usr/bin/env python
'''
parse a PPRZLink protocol XML file and generate a OpenDDS compatible
IDL file. See http://opendds.org/ for details. 

Copyright (C) 2017 Michal Podhradsky <mpodhradsky@galois.com>
For the Paparazzi UAV and PPRZLINK projects
'''
import re
import sys, os
import gen_messages, pprz_template, pprz_parse

t = pprz_template.PPRZTemplate()

def generate_messages_h(directory, name, xml):
    print(xml)
    '''generate messages header'''
    if name == 'stdout':
        f = sys.stdout
    else:
        f = open(os.path.join(directory, name), mode='w')
    t.write(f,'''
/*
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

  typedef sequence<unsigned short> unsigned_short_array; // uint8 ot uint16[]
  typedef sequence<short> short_array; // int8 or int16[]
  typedef sequence<unsigned long> unsigned_long_array; // uint32[]
  typedef sequence<long> long_array; // int32[]
  typedef sequence<float> float_array; // float[]
  typedef sequence<double> double_array; // double[]

module ${class_name} {
${{message:
#pragma DCPS_DATA_TYPE "${class_name}::${msg_name}"
#pragma DCPS_DATA_TYPE "${class_name}::${msg_name} msg_id"

struct ${msg_name} {
    short msg_id;
${{fields:    ${opendds_type} field_${attrib_macro}}}
    };
}}

};
''', xml)
    if name != 'stdout':
        f.close()

def eval_int(expr):
    import ast
    return int(eval(compile(ast.parse(expr, mode='eval'), '<string>', 'eval')))

def generate(output, xml):
    '''generate complete PPRZLink C implemenation from a XML messages file'''

    directory, name = os.path.split(output)


    print("Generating C implementation in %s" % output)
    if directory != '':
        pprz_parse.mkdir_p(directory)
    
    # fix the class name to conform with standards
    xml.class_name = xml.class_name.title() 

    # add some extra field attributes for convenience with arrays
    for m in xml.message:
        m.msg_name = m.msg_name.title()
        m.msg_name = re.sub('[_]', '', m.msg_name) 
        offset = 2 # 2 bytes initial offset (sender id, message id)
        for f in m.fields:
            if f.type == 'uint8_t' or f.type == 'uint16_t' or f.type == 'char':
                f.opendds_type = 'unsigned short'
            elif f.type == 'int8_t' or f.type == 'int16_t':
                f.opendds_type = 'short'
            elif f.type == 'uint32_t':
                f.opendds_type = 'unsigned long'
            elif f.type == 'int32_t':
                f.opendds_type = 'long'
            elif f.type == 'float':
                f.opendds_type = 'float'
            elif f.type == 'double':
                f.opendds_type = 'double'
            else:
                f.opendds_type = 'void'
            
            if not type(f.array_type) == type(None):
                # it is an array - wrap as a stream
                f.opendds_type = re.sub('[ ]', '_', f.opendds_type) + '_array'

            f.attrib_macro = '%s;\n' % f.field_name

    generate_messages_h(directory, name, xml)
