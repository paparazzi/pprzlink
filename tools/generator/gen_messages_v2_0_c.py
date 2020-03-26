#!/usr/bin/env python3
'''
parse a PPRZLink protocol XML file and generate a C implementation
for version 2.0 of the protocol

Copyright (C) 2017 Fabien Garcia <fabien.garcia@enac.fr>
For the Paparazzi UAV and PPRZLINK projects

based on:
    Copyright Andrew Tridgell 2011
    Released under GNU GPL version 3 or later
'''

from __future__ import print_function
import sys, os
import gen_messages, pprz_template, pprz_parse

t = pprz_template.PPRZTemplate()

def generate_main_h(directory, name, xml):
    '''generate main header per XML file'''
    f = open(os.path.join(directory, name), mode='w')
    t.write(f, '''
/** @file
 *  @brief PPRZLink message header built from ${filename}
 *  @see http://paparazziuav.org
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PPRZLINK_DEFAULT_VER
#define PPRZLINK_DEFAULT_VER 2
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

#ifdef __cplusplus
} // extern "C"
#endif

#if DOWNLINK
#define DL_MSG_${class_name}_NB ${nb_messages}
#endif // DOWNLINK

#define DL_${class_name}_CLASS_ID ${class_id}

${{message:#include "${class_name}/${msg_name}.h"
}}

// Macros for keeping compatibility between versions
// These should not be used directly
#define _inner_send_msg(NAME,PROTO_VERSION) pprzlink_msg_v##PROTO_VERSION##_send_##NAME
#define _send_msg(NAME,PROTO_VERSION) _inner_send_msg(NAME,PROTO_VERSION)

''', xml)

    f.close()


def copy_fixed_headers(directory, protocol_version):
    '''copy the fixed protocol headers to the target directory'''
    import shutil
    hlist = [ 'pprzlink_device.h', 'pprzlink_transport.h', 'pprzlink_utils.h', 'pprzlink_message.h' ]
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

def generate_one(directory, xml, m):
    f = open(os.path.join(os.path.join(directory, xml.class_name), m.msg_name + ".h"), mode='w')
    t.write(f, '''
/** @file
 *  @brief PPRZLink message header for ${msg_name} in class ${class_name}
 *
 *  ${description}
 *  @see http://paparazziuav.org
 */

#ifndef _VAR_MESSAGES_${class_name}_${msg_name}_H_
#define _VAR_MESSAGES_${class_name}_${msg_name}_H_


#include "pprzlink/pprzlink_device.h"
#include "pprzlink/pprzlink_transport.h"
#include "pprzlink/pprzlink_utils.h"
#include "pprzlink/pprzlink_message.h"


#ifdef __cplusplus
extern "C" {
#endif

#if DOWNLINK

#define DL_${msg_name} ${id}
#define PPRZ_MSG_ID_${msg_name} ${id}

/**
 * Macro that redirect calls to the default version of pprzlink API
 * Used for compatibility between versions.
 */
#define pprzlink_msg_send_${msg_name} _send_msg(${msg_name},PPRZLINK_DEFAULT_VER)

/**
 * Sends a ${msg_name} message (API V2.0 version)
 *
 * @param msg the pprzlink_msg structure for this message${{fields:\n * @param ${attrib_param} ${description}}}
 */
static inline void pprzlink_msg_v2_send_${msg_name}(struct pprzlink_msg * msg${{fields:, ${attrib_fun}}}) {
#if PPRZLINK_ENABLE_FD
  long _FD = 0; /* can be an address, an index, a file descriptor, ... */
#endif
  const uint8_t size = msg->trans->size_of(msg, /* msg header overhead */4${{fields:${array_extra_length}+${length}}});
  if (msg->trans->check_available_space(msg, _FD_ADDR, size)) {
    msg->trans->count_bytes(msg, size);
    msg->trans->start_message(msg, _FD, /* msg header overhead */4${{fields:${array_extra_length}+${length}}});
    msg->trans->put_bytes(msg, _FD, DL_TYPE_UINT8, DL_FORMAT_SCALAR, &(msg->sender_id), 1);
    msg->trans->put_named_byte(msg, _FD, DL_TYPE_UINT8, DL_FORMAT_SCALAR, msg->receiver_id, NULL);
    uint8_t comp_class = (msg->component_id & 0x0F) << 4 | (${class_id} & 0x0F);
    msg->trans->put_named_byte(msg, _FD, DL_TYPE_UINT8, DL_FORMAT_SCALAR, comp_class, NULL);
    msg->trans->put_named_byte(msg, _FD, DL_TYPE_UINT8, DL_FORMAT_SCALAR, DL_${msg_name}, "${msg_name}");
    ${{fields:${array_byte}msg->trans->put_bytes(msg, _FD, DL_TYPE_${type_upper}, ${dl_format}, (void *) _${field_name}, ${length});
    }}msg->trans->end_message(msg, _FD);
  } else
        msg->trans->overrun(msg);
}

// Compatibility with the protocol v1.0 API
#define pprzlink_msg_v1_send_${msg_name} pprz_msg_send_${msg_name}
#define DOWNLINK_SEND_${msg_name}(_trans, _dev${{fields:, ${attrib_macro}}}) pprz_msg_send_${msg_name}(&((_trans).trans_tx), &((_dev).device), AC_ID${{fields:, ${attrib_macro}}})
/**
 * Sends a ${msg_name} message (API V1.0 version)
 *
 * @param trans A pointer to the transport_tx structure used for sending the message
 * @param dev A pointer to the link_device structure through which the message will be sent
 * @param ac_id The id of the sender of the message${{fields:\n * @param ${attrib_param} ${description}}}
 */
static inline void pprz_msg_send_${msg_name}(struct transport_tx *trans, struct link_device *dev, uint8_t ac_id${{fields:, ${attrib_fun}}}) {
    struct pprzlink_msg msg;
    msg.trans = trans;
    msg.dev = dev;
    msg.sender_id = ac_id;
    msg.receiver_id = 0;
    msg.component_id = 0;
    pprzlink_msg_v2_send_${msg_name}(&msg${{fields:,${attrib_param}}});
}


#else // DOWNLINK

#define DOWNLINK_SEND_${msg_name}(_trans, _dev${{fields:, ${attrib_macro}}}) {}
static inline void pprz_send_msg_${msg_name}(struct transport_tx *trans __attribute__((unused)), struct link_device *dev __attribute__((unused)), uint8_t ac_id __attribute__((unused))${{fields:, ${attrib_fun_unused}}}) {}

#endif // DOWNLINK

${{fields:${fun_read_array_byte}
/** Getter for field ${field_name} in message ${msg_name}
  *
  * @param _payload : a pointer to the ${msg_name} message
  * @return ${description}
  */
static inline ${return_type} pprzlink_get_DL_${msg_name}_${field_name}(uint8_t * _payload __attribute__((unused)))
{
    return _PPRZ_VAL_${read_type}(_payload, ${offset});
}

}}

/* Compatibility macros */
${{fields:${read_array_byte}#define DL_${msg_name}_${field_name}(_payload) pprzlink_get_DL_${msg_name}_${field_name}(_payload)\n}}\n\n

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _VAR_MESSAGES_${class_name}_${msg_name}_H_

''', {'msg_name' : m.msg_name, 'description' : m.description ,'class_id' : xml.class_id, 'class_name' : xml.class_name, 'id' : m.id, 'fields' : m.fields, 'message' : xml.message})


def generate(output, xml):
    '''generate complete MAVLink C implemenation'''

    directory, name = os.path.split(output)
    print("Generating C implementation in %s" % output)
    if directory != '':
        pprz_parse.mkdir_p(directory)
        pprz_parse.mkdir_p(os.path.join(directory, xml.class_name))

    # add some extra field attributes for convenience with arrays
    for m in xml.message:
        offset = 4 # 4 bytes initial offset (sender id, receiver id, component and class id, message id)
        for f in m.fields:
            if f.array_type == 'VariableArray':
                f.attrib_macro = 'nb_%s, %s' % (f.field_name, f.field_name)
                f.attrib_param = 'nb_%s,_%s' % (f.field_name, f.field_name)
                f.attrib_fun = 'uint8_t nb_%s, %s *_%s' % (f.field_name, f.type, f.field_name)
                f.attrib_fun_unused = 'uint8_t nb_%s __attribute__((unused)), %s *_%s __attribute__((unused))' % (f.field_name, f.type, f.field_name)
                f.array_byte = 'msg->trans->put_bytes(msg, _FD, DL_TYPE_ARRAY_LENGTH, DL_FORMAT_SCALAR, (void *) &nb_%s, 1);\n    ' % f.field_name
                f.read_type = f.type+'_array'
                f.return_type = f.type + ' *'
                if (offset + 1) % min(4, int(f.type_length)) == 0: # data are aligned
                    f.fun_read_array_byte = '/** Getter for length of array %s in message %s\n *\n * @return %s : %s\n */\n static inline uint8_t pprzlink_get_%s_%s_length(void* _payload) {\n    return _PPRZ_VAL_uint8_t(_payload, %d);\n}\n' \
                                        % (f.field_name,m.msg_name, f.field_name, f.description, m.msg_name, f.field_name ,offset)
                else: # rely on arch capability to read or not
                    f.fun_read_array_byte = '/** Getter for length of array %s in message %s\n *\n * @return %s : %s\n */\n static inline uint8_t pprzlink_get_%s_%s_length(__attribute__ ((unused)) void* _payload) {\n    return _PPRZ_VAL_len_aligned(_payload, %d);\n}\n' \
                                        % (f.field_name,m.msg_name, f.field_name, f.description, m.msg_name, f.field_name ,offset)
                f.read_array_byte = '#define DL_%s_%s_length(_payload) pprzlink_get_%s_%s_length(_payload)\n' % (m.msg_name, f.field_name, m.msg_name, f.field_name)
                offset += 1
                # variable arrays are last (for now)
                f.offset = offset
                f.dl_format = 'DL_FORMAT_ARRAY'
            elif f.array_type == 'FixedArray':
                f.attrib_macro = '%s' % f.field_name
                f.attrib_fun = '%s *_%s' % (f.type, f.field_name)
                f.attrib_param = '_%s' % (f.field_name)
                f.attrib_fun_unused = '%s *_%s __attribute__((unused))' % (f.type, f.field_name)
                f.array_byte = ''
                f.read_type = f.type+'_array'
                f.return_type = f.type + ' *'
                if offset % min(4, int(f.type_length)) == 0: # data are aligned
                    f.fun_read_array_byte = '/** Getter for length of array %s in message %s\n *\n * @return %s : %s\n */\n static inline uint8_t pprzlink_get_%s_%s_length(void* _payload __attribute__ ((unused))) {\n    return %d;\n}\n' \
                                            % (f.field_name,m.msg_name, f.field_name, f.description, m.msg_name, f.field_name ,int(f.array_length))
                else: # rely on arch capability to read or not
                    f.fun_read_array_byte = '/** Getter for length of array %s in message %s\n *\n * @return %s : %s\n */\n static inline uint8_t pprzlink_get_%s_%s_length(void* _payload __attribute__ ((unused))) {\n    return _PPRZ_VAL_fixed_len_aligned(%d);\n}\n' \
                                            % (f.field_name,m.msg_name, f.field_name, f.description, m.msg_name, f.field_name ,int(f.array_length))
                f.read_array_byte = '#define DL_%s_%s_length(_payload) pprzlink_get_%s_%s_length(_payload)\n' % (m.msg_name, f.field_name, m.msg_name, f.field_name)
                f.offset = offset
                offset += int(eval(f.length))
                f.dl_format = 'DL_FORMAT_ARRAY'
            else:
                f.offset = offset
                offset += int(f.length)
                f.attrib_macro = '%s' % f.field_name
                f.attrib_fun = '%s *_%s' % (f.type, f.field_name)
                f.attrib_param = '_%s' % (f.field_name)
                f.attrib_fun_unused = '%s *_%s __attribute__((unused))' % (f.type, f.field_name)
                f.array_byte = ''
                f.read_type = f.type
                f.return_type = f.type
                f.read_array_byte = ''
                f.fun_read_array_byte = ''
                f.dl_format = 'DL_FORMAT_SCALAR'
        generate_one(directory, xml, m)

    generate_main_h(directory, name, xml)
    copy_fixed_headers(directory, xml.protocol_version)
