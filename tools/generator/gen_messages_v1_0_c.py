#!/usr/bin/env python3
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
#include "pprzlink/pprzlink_utils.h"

#ifndef PPRZLINK_DEFAULT_VER
#define PPRZLINK_DEFAULT_VER 1
#endif

#ifndef PPRZLINK_ENABLE_FD
#define PPRZLINK_ENABLE_FD FALSE
#endif

// dummy fd to save ROM if this is not used
#if !PPRZLINK_ENABLE_FD
#define _FD 0
#define _FD_ADDR 0
#else
#define _FD_ADDR &_FD
#endif

#if DOWNLINK
${{message:#define DL_${msg_name} ${id}
#define PPRZ_MSG_ID_${msg_name} ${id}
}}
#define DL_MSG_${class_name}_NB ${nb_messages}

${{message:
#define DOWNLINK_SEND_${msg_name}(_trans, _dev${{fields:, ${attrib_macro}}}) pprz_msg_send_${msg_name}(&((_trans).trans_tx), &((_dev).device), AC_ID${{fields:, ${attrib_macro}}})
static inline void pprz_msg_send_${msg_name}(struct transport_tx *trans, struct link_device *dev, uint8_t ac_id${{fields:, ${attrib_fun}}}) {
#if PPRZLINK_ENABLE_FD
  long _FD = 0; /* can be an address, an index, a file descriptor, ... */
#endif
  if (trans->check_available_space(trans->impl, dev, _FD_ADDR, trans->size_of(trans->impl, 0${{fields:${array_extra_length}+${length}}} +2 /* msg header overhead */))) {
    trans->count_bytes(trans->impl, dev, trans->size_of(trans->impl, 0${{fields:${array_extra_length}+${length}}} +2 /* msg header overhead */));
    trans->start_message(trans->impl, dev, _FD, 0${{fields:${array_extra_length}+${length}}} +2 /* msg header overhead */);
    trans->put_bytes(trans->impl, dev, _FD, DL_TYPE_UINT8, DL_FORMAT_SCALAR, &ac_id, 1);
    trans->put_named_byte(trans->impl, dev, _FD, DL_TYPE_UINT8, DL_FORMAT_SCALAR, DL_${msg_name}, "${msg_name}");
    ${{fields:${array_byte}trans->put_bytes(trans->impl, dev, _FD, DL_TYPE_${type_upper}, ${dl_format}, (void *) _${field_name}, ${length});
    }}trans->end_message(trans->impl, dev, _FD);
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

${{message:${{fields:${read_array_byte}#define DL_${msg_name}_${field_name}(_payload) _PPRZ_VAL_${read_type}(_payload, ${offset})\n}}\n\n}}

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
    hlist = [ 'pprzlink_device.h', 'pprzlink_transport.h', 'pprzlink_utils.h' ]
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
                f.array_byte = 'trans->put_bytes(trans->impl, dev, _FD, DL_TYPE_ARRAY_LENGTH, DL_FORMAT_SCALAR, (void *) &nb_%s, 1);\n    ' % f.field_name
                f.read_type = f.type+'_array'
                if (offset + 1) % min(4, int(f.type_length)) == 0: # data are aligned
                    f.read_array_byte = '#define DL_%s_%s_length(_payload) _PPRZ_VAL_uint8_t(_payload, %d)\n' % (m.msg_name, f.field_name, offset)
                else: # rely on arch capability to read or not
                    f.read_array_byte = '#define DL_%s_%s_length(_payload) _PPRZ_VAL_len_aligned(_payload, %d)\n' % (m.msg_name, f.field_name, offset)
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
                if offset % min(4, int(f.type_length)) == 0: # data are aligned
                    f.read_array_byte = '#define DL_%s_%s_length(_payload) (%d)\n' % (m.msg_name, f.field_name, int(f.array_length))
                else: # rely on arch capability to read or not
                    f.read_array_byte = '#define DL_%s_%s_length(_payload) _PPRZ_VAL_fixed_len_aligned(%d)\n' % (m.msg_name, f.field_name, int(f.array_length))
                f.offset = offset
                offset += int(eval(f.length))
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

    generate_messages_h(directory, name, xml)
    copy_fixed_headers(directory, xml.protocol_version)
