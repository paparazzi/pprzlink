#!/usr/bin/env python3
'''
parse a PPRZLink protocol XML file and generate a C implementation
for version 2.0 of the protocol

Copyright (C) 2017 Fabien Garcia <fabien.garcia@enac.fr>
Copyright (C) 2019 Gautier Hattenberger <gautier.hattenberger@enac.fr>
For the Paparazzi UAV and PPRZLINK projects

based on:
    Copyright Andrew Tridgell 2011
    Released under GNU GPL version 3 or later
'''

from __future__ import print_function
import os
import pprz_template, pprz_parse

t = pprz_template.PPRZTemplate()

def generate_main_h(directory, name, xml):
    '''generate main header per XML file'''
    f = open(os.path.join(directory, name), mode='w')
    t.write(f, '''
/** @file
 *  @brief PPRZLink message header built from ${filename}
 *  @see http://paparazziuav.org
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "pprzlink/pprzlink_standalone.h"
#include "pprzlink/pprzlink_utils.h"

${{message:#define PPRZ_MSG_ID_${msg_name} ${id}

static inline void pprzlink_msg_send_${msg_name}(struct pprzlink_device_tx *dev, uint8_t sender_id, uint8_t receiver_id${{fields:, ${attrib_fun}}}) {
  uint8_t size = 4+4${{fields:${array_extra_length}+${length}}};
  if (dev->check_space(size)) {
    dev->put_char(PPRZLINK_STX);
    dev->put_char(size);
    dev->ck_a = size;
    dev->ck_b = size;
    uint8_t head[4];
    head[0] = sender_id;
    head[1] = receiver_id;
    head[2] = (${class_id} & 0x0F); // class id but no component id for now
    head[3] = PPRZ_MSG_ID_${msg_name};
    pprzlink_put_bytes(dev, head, 4);
    ${{fields:${array_byte}pprzlink_put_bytes(dev, (uint8_t *) _${field_name}, ${length});
    }}dev->put_char(dev->ck_a);
    dev->put_char(dev->ck_b);
    if (dev->send_message != NULL) {
      dev->send_message();
    }
  }
}

${{fields:${fun_read_array_byte}
static inline ${return_type} pprzlink_get_${msg_name}_${field_name}(uint8_t * _payload __attribute__((unused)))
{
  return _PPRZ_VAL_${read_type}(_payload, ${offset});
}

}}

}}

#ifdef __cplusplus
} // extern "C"
#endif


''', xml)

    f.close()


def copy_fixed_headers(directory, protocol_version):
    '''copy the fixed protocol headers to the target directory'''
    import shutil
    hlist = [ 'pprzlink_utils.h', 'pprzlink_standalone.h' ]
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

def generate(output, xml, msg_list):
    '''
    generate PPRZLINK C standalone implemenation

    single header to send / read a limited set of messages
    '''
    # filter messages
    msg_filtered = []
    for m in xml.message:
        if m.msg_name in msg_list.split(','):
            msg_filtered.append(m)
    xml.message = msg_filtered
    print(xml)

    directory, name = os.path.split(output)
    print("Generating C standalone implementation in %s" % output)
    if directory != '':
        pprz_parse.mkdir_p(directory)

    # add some extra field attributes for convenience with arrays
    for m in xml.message:
        offset = 4 # 4 bytes initial offset (sender id, receiver id, component and class id, message id)
        for f in m.fields:
            if f.array_type == 'VariableArray':
                f.attrib_fun = 'uint8_t nb_%s, %s *_%s' % (f.field_name, f.type, f.field_name)
                f.attrib_fun_unused = 'uint8_t nb_%s __attribute__((unused)), %s *_%s __attribute__((unused))' % (f.field_name, f.type, f.field_name)
                f.array_byte = 'pprzlink_put_bytes(dev, (uint8_t *) &nb_%s, 1);\n    ' % f.field_name
                f.read_type = f.type+'_array'
                f.return_type = f.type + ' *'
                if (offset + 1) % min(4, int(f.type_length)) == 0: # data are aligned
                    f.fun_read_array_byte = 'static inline uint8_t pprzlink_get_%s_%s_length(void* _payload) {\n  return _PPRZ_VAL_uint8_t(_payload, %d);\n}\n' \
                                        % (m.msg_name, f.field_name ,offset)
                else: # rely on arch capability to read or not
                    f.fun_read_array_byte = 'static inline uint8_t pprzlink_get_%s_%s_length(void * _payload __attribute__ ((unused)))  {\n  return _PPRZ_VAL_len_aligned(_payload, %d);\n}\n' \
                                        % (m.msg_name, f.field_name ,offset)
                offset += 1
                # variable arrays are last (for now)
                f.offset = offset
            elif f.array_type == 'FixedArray':
                f.attrib_fun = '%s *_%s' % (f.type, f.field_name)
                f.attrib_fun_unused = '%s *_%s __attribute__((unused))' % (f.type, f.field_name)
                f.array_byte = ''
                f.read_type = f.type+'_array'
                f.return_type = f.type + ' *'
                if offset % min(4, int(f.type_length)) == 0: # data are aligned
                    f.fun_read_array_byte = 'static inline uint8_t pprzlink_get_%s_%s_length(void* _payload __attribute__ ((unused))) {\n  return %d;\n}\n' \
                                            % (m.msg_name, f.field_name ,int(f.array_length))
                else: # rely on arch capability to read or not
                    f.fun_read_array_byte = 'static inline uint8_t pprzlink_get_%s_%s_length(void* _payload __attribute__ ((unused))) {\n  return _PPRZ_VAL_fixed_len_aligned(%d);\n}\n' \
                                            % (m.msg_name, f.field_name ,int(f.array_length))
                f.offset = offset
                offset += int(eval(f.length))
            else:
                f.offset = offset
                offset += int(f.length)
                f.attrib_fun = '%s *_%s' % (f.type, f.field_name)
                f.attrib_fun_unused = '%s *_%s __attribute__((unused))' % (f.type, f.field_name)
                f.array_byte = ''
                f.read_type = f.type
                f.return_type = f.type
                f.fun_read_array_byte = ''

    generate_main_h(directory, name, xml)
    copy_fixed_headers(directory, xml.protocol_version)

