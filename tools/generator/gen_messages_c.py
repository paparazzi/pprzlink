#!/usr/bin/env python
'''
parse a PPRZLink protocol XML file and generate a C implementation

Copyright (C) 2015 Gautier Hattenberger <gautier.hattenberger@enac.fr>
For the Paparazzi UAV and PPRZLINK projects

based on:
    Copyright Andrew Tridgell 2011
    Released under GNU GPL version 3 or later
'''

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
/** @file
 *	@brief PPRZLink messages header built from ${filename}
 *	@see http://paparazziuav.org
 */
#ifndef _VAR_MESSAGES_${class_name}_H_
#define _VAR_MESSAGES_${class_name}_H_

#define PPRZLINK_PROTOCOL_VERSION "${protocol_version}"
#define PPRZLINK_PROTOCOL_VERSION_MAJOR ${protocol_version_major}
#define PPRZLINK_PROTOCOL_VERSION_MINOR ${protocol_version_minor}

#ifdef __cplusplus
extern "C" {
#endif

#include "pprzlink/pprzlink_device.h"
#include "pprzlink/pprzlink_transport.h"

#if DOWNLINK
${{message:#define DL_${msg_name} ${id}
#define PPRZ_MSG_ID_${msg_name} ${id}
}}
#define DL_MSG_${class_name}_NB ${nb_messages}

${{message:
#define DOWNLINK_SEND_${msg_name}(_trans, _dev${{fields:, ${attrib_macro}}}) pprz_msg_send_${msg_name}(&((_trans).trans_tx), &((_dev).device), AC_ID${{fields:, ${attrib_macro}}})
static inline void pprz_msg_send_${msg_name}(struct transport_tx *trans, struct link_device *dev, uint8_t ac_id${{fields:, ${attrib_fun}}}) {
  if (trans->check_available_space(trans->impl, dev, trans->size_of(trans->impl, 0${{fields:${array_extra_length}+${length}}} +2 /* msg header overhead */))) {
    trans->count_bytes(trans->impl, dev, trans->size_of(trans->impl, 0${{fields:${array_extra_length}+${length}}} +2 /* msg header overhead */));
    trans->start_message(trans->impl, dev, 0${{fields:${array_extra_length}+${length}}} +2 /* msg header overhead */);
    trans->put_bytes(trans->impl, dev, DL_TYPE_UINT8, DL_FORMAT_SCALAR, 1, &ac_id);
    trans->put_named_byte(trans->impl, dev, DL_TYPE_UINT8, DL_FORMAT_SCALAR, DL_${msg_name}, "${msg_name}");
    ${{fields:${array_byte}trans->put_bytes(trans->impl, dev, DL_TYPE_${type_upper}, ${dl_format}, ${length}, (void *) _${field_name});
    }}trans->end_message(trans->impl, dev);
  } else
    trans->overrun(trans->impl, dev);
}
}}

#else // DOWNLINK
${{message:
#define DOWNLINK_SEND_${msg_name}(_trans, _dev${{fields:, ${attrib_macro}}}) {}
static inline void pprz_send_msg_${msg_name}(struct transport_tx *trans __attribute__((unused)), struct link_device *dev __attribute__((unused)), uint8_t ac_id __attribute__((unused))${{fields:, ${attrib_fun_unused}}}) {}
}}

#endif // DOWNLINK

#define _PPRZ_${class_name}_RET_char(_payload, _offset) ((char)(*((uint8_t*)_payload+_offset)))
#define _PPRZ_${class_name}_RET_int8_t(_payload, _offset) ((int8_t)(*((uint8_t*)_payload+_offset)))
#define _PPRZ_${class_name}_RET_uint8_t(_payload, _offset) ((int8_t)(*((uint8_t*)_payload+_offset)))
#define _PPRZ_${class_name}_RET_int16_t(_payload, _offset) ((uint16_t)(*((uint8_t*)_payload+_offset)|*((uint8_t*)_payload+_offset+1)<<8))
#define _PPRZ_${class_name}_RET_uint16_t(_payload, _offset) ((uint16_t)(*((uint8_t*)_payload+_offset)|*((uint8_t*)_payload+_offset+1)<<8))
#define _PPRZ_${class_name}_RET_int32_t(_payload, _offset) (int32_t)(*((uint8_t*)_payload+_offset)|*((uint8_t*)_payload+_offset+1)<<8|((uint32_t)*((uint8_t*)_payload+_offset+2))<<16|((uint32_t)*((uint8_t*)_payload+_offset+3))<<24)
#define _PPRZ_${class_name}_RET_uint32_t(_payload, _offset) (uint32_t)(*((uint8_t*)_payload+_offset)|*((uint8_t*)_payload+_offset+1)<<8|((uint32_t)*((uint8_t*)_payload+_offset+2))<<16|((uint32_t)*((uint8_t*)_payload+_offset+3))<<24)
#define _PPRZ_${class_name}_RET_float(_payload, _offset) ({ union { uint32_t u; float f; } _f; _f.u = _PPRZ_${class_name}_RET_uint32_t(_payload, _offset); _f.f; })
#define _PPRZ_${class_name}_RET_int64_t(_payload, _offset) (int64_t)(*((uint8_t*)_payload+_offset)|((uint64_t)*((uint8_t*)_payload+_offset+1))<<8|((uint64_t)*((uint8_t*)_payload+_offset+2))<<16|((uint32_t)*((uint8_t*)_payload+_offset+3))<<24|((uint64_t*)*((uint8_t*)_payload+_offset+4))<<32|((uint64_t)*((uint8_t*)_payload+_offset+5))<<40|((uint64_t)*((uint8_t*)_payload+_offset+6))<<48|((uint64_t)*((uint8_t*)_payload+_offset+7))<<56)
#define _PPRZ_${class_name}_RET_uint64_t(_payload, _offset) (uint64_t)(*((uint8_t*)_payload+_offset)|((uint64_t)*((uint8_t*)_payload+_offset+1))<<8|((uint64_t)*((uint8_t*)_payload+_offset+2))<<16|((uint32_t)*((uint8_t*)_payload+_offset+3))<<24|((uint64_t*)*((uint8_t*)_payload+_offset+4))<<32|((uint64_t)*((uint8_t*)_payload+_offset+5))<<40|((uint64_t)*((uint8_t*)_payload+_offset+6))<<48|((uint64_t)*((uint8_t*)_payload+_offset+7))<<56)
#define _PPRZ_${class_name}_RET_double(_payload, _offset) ({ union { uint64_t u; double f; } _f; _f.u = (uint64_t)(_PPRZ_${class_name}_RET_uint64_t(_payload, _offset)); Swap32IfBigEndian(_f.u); _f.f; })
#define _PPRZ_${class_name}_RET_char_array(_payload, _offset) ((char*)(_payload+_offset))
#define _PPRZ_${class_name}_RET_int8_t_array(_payload, _offset) ((int8_t*)(_payload+_offset))
#define _PPRZ_${class_name}_RET_uint8_t_array(_payload, _offset) ((uint8_t*)(_payload+_offset))
#define _PPRZ_${class_name}_RET_int16_t_array(_payload, _offset) ((int16_t*)(_payload+_offset))
#define _PPRZ_${class_name}_RET_uint16_t_array(_payload, _offset) ((uint16_t*)(_payload+_offset))
#define _PPRZ_${class_name}_RET_int32_t_array(_payload, _offset) ((int32_t*)(_payload+_offset))
#define _PPRZ_${class_name}_RET_uint32_t_array(_payload, _offset) ((uint32_t*)(_payload+_offset))
#define _PPRZ_${class_name}_RET_int64_t_array(_payload, _offset) ((int64_t*)(_payload+_offset))
#define _PPRZ_${class_name}_RET_uint64_t_array(_payload, _offset) ((uint64_t*)(_payload+_offset))
#define _PPRZ_${class_name}_RET_float_array(_payload, _offset) ((float*)(_payload+_offset))
#define _PPRZ_${class_name}_RET_double_array(_payload, _offset) ((double*)(_payload+_offset))

${{message:${{read_fields:${read_array_byte}#define DL_${msg_name}_${field_name}(_payload) _PPRZ_${class_name}_RET_${read_type}(_payload, ${offset})\n}}${read_ret}}}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _VAR_MESSAGES_${class_name}_H_
''', xml)
    if name != 'stdout':
        f.close()

def copy_fixed_headers(directory, protocol_version):
    '''copy the fixed protocol headers to the target directory'''
    import shutil
    hlist = [ 'pprzlink_device.h', 'pprzlink_transport.h' ]
    basepath = os.path.dirname(os.path.realpath(__file__))
    srcpath = os.path.join(basepath, 'C/include_v%s' % protocol_version)
    if directory == '':
        print("Skip copying fixed headers")
        return
    print("Copying fixed headers")
    for h in hlist:
        src = os.path.realpath(os.path.join(srcpath, h))
        dest = os.path.realpath(os.path.join(directory, h))
        if src == dest:
            continue
        shutil.copy(src, dest)

def generate(output, xml):
    '''generate complete PPRZLink C implemenation from a XML messages file'''

    directory, name = os.path.split(output)

    
    print("Generating C implementation in %s" % output)
    if directory != '':
        pprz_parse.mkdir_p(directory)

    # add some extra field attributes for convenience with arrays
    for m in xml.message:
        offset = 2 # 2 bytes initial offset (sender id, message id)
        for f in m.fields:
            if f.array_type == 'VariableArray':
                f.attrib_macro = 'nb_%s, %s' % (f.field_name, f.field_name)
                f.attrib_fun = 'uint8_t nb_%s, %s *_%s' % (f.field_name, f.type, f.field_name)
                f.attrib_fun_unused = 'uint8_t nb_%s __attribute__((unused)), %s *_%s __attribute__((unused))' % (f.field_name, f.type, f.field_name)
                f.array_byte = 'trans->put_bytes(trans->impl, dev, DL_TYPE_ARRAY_LENGTH, DL_FORMAT_SCALAR, 1, (void *) &nb_%s);\n    ' % f.field_name
                f.read_type = f.type+'_array'
                f.read_array_byte = '#define DL_%s_%s_length(_payload) _PPRZ_%s_RET_uint8_t(_payload, %d)\n' % (m.msg_name, f.field_name, xml.class_name, offset)
                offset += 1
                # variable arrays are last (for now)
                f.offset = offset
                f.dl_format = 'DL_FORMAT_ARRAY'
            elif f.array_type == 'FixedArray':
                f.attrib_macro = '%s' % f.field_name
                f.attrib_fun = '%s *_%s' % (f.type, f.field_name)
                f.attrib_fun_unused = '%s *_%s __attribute__((unused))' % (f.type, f.field_name)
                f.array_byte = ''
                f.read_type = f.type+'_array'
                f.read_array_byte = '#define DL_%s_%s_length(_payload) _PPRZ_%s_RET_uint8_t(_payload, %d)\n' % (m.msg_name, f.field_name, xml.class_name, offset)
                offset += 1
                f.offset = offset
                offset += int(f.length)
                f.dl_format = 'DL_FORMAT_ARRAY'
            else:
                f.offset = offset
                offset += int(f.length)
                f.attrib_macro = '%s' % f.field_name
                f.attrib_fun = '%s *_%s' % (f.type, f.field_name)
                f.attrib_fun_unused = '%s *_%s __attribute__((unused))' % (f.type, f.field_name)
                f.array_byte = ''
                f.read_type = f.type
                f.read_array_byte = ''
                f.dl_format = 'DL_FORMAT_SCALAR'
        if xml.class_name != 'telemetry':
            m.read_fields = m.fields
            m.read_ret = '\n\n'
        else:
            # skip telemetry class
            m.read_fields = []
            m.read_ret = ''

    generate_messages_h(directory, name, xml)
    copy_fixed_headers(directory, xml.protocol_version)

