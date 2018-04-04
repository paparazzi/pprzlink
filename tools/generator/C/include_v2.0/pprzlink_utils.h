/*
 * Copyright (C) 2016 Gautier Hattenberger <gautier.hattenberger@enac.fr>
 *
 * This file is part of paparazzi.
 *
 * paparazzi is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * paparazzi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with paparazzi; see the file COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

/** \file pprzlink_utils.h
 *
 *  Utility macros and functions for PPRZLINK
 *
 * Reading macros to access incoming messages values (which might not be aligned).
 *
 * Define PPRZLINK_UNALIGNED_ACCESS to TRUE if the target CPU/MMU allows unaligned access.
 * This is true for x86/64 and most recent ARM platforms (ARMv7, Cortex-A, Cortex-M3/4).
 * Examples for targets WITHOUT unaligned access support: LPC21xx, Cortex-M0
 */

#ifndef PPRZLINK_UTILS_H
#define PPRZLINK_UTILS_H

 #ifdef __cplusplus
extern "C" {
#endif

#define PPRZLINK_PROTOCOL_VERSION "2.0"

#include <inttypes.h>

#ifdef __IEEE_BIG_ENDIAN /* From machine/ieeefp.h */
#define Swap32IfBigEndian(_u) { _u = (_u << 32) | (_u >> 32); }
#else
#define Swap32IfBigEndian(_) {}
#endif

// Single byte values are always aligned
#define _PPRZ_VAL_char(_payload, _offset) ((char)(*((uint8_t*)_payload+_offset)))
#define _PPRZ_VAL_int8_t(_payload, _offset) ((int8_t)(*((uint8_t*)_payload+_offset)))
#define _PPRZ_VAL_uint8_t(_payload, _offset) ((uint8_t)(*((uint8_t*)_payload+_offset)))
#define _PPRZ_VAL_char_array(_payload, _offset) ((char*)(_payload+_offset))
#define _PPRZ_VAL_int8_t_array(_payload, _offset) ((int8_t*)(_payload+_offset))
#define _PPRZ_VAL_uint8_t_array(_payload, _offset) ((uint8_t*)(_payload+_offset))

// Use macros according to alignment capabilities
// be conservative by default
#ifndef PPRZLINK_UNALIGNED_ACCESS
#define PPRZLINK_UNALIGNED_ACCESS 0
#endif

#if PPRZLINK_UNALIGNED_ACCESS

// This way of reading is more efficient when data is actually aligned
// but is still working if the CPU/MMU supports unaligned access.
// The use of 'packed' forces the compiler to assume no better
// than 1-byte alignment, and issue sequential byte reads/writes.
typedef union __attribute__((packed)) {
  int16_t   int16;
  uint16_t  uint16;
  int32_t   int32;
  uint32_t  uint32;
  int64_t   int64;
  uint64_t  uint64;
  float     f32;
  double    f64;
} unaligned_t;

#define _PPRZ_VAL_int16_t(_payload, _offset) (((unaligned_t*)(_payload+_offset))->int16)
#define _PPRZ_VAL_uint16_t(_payload, _offset) (((unaligned_t*)(_payload+_offset))->uint16)
#define _PPRZ_VAL_int32_t(_payload, _offset) (((unaligned_t*)(_payload+_offset))->int32)
#define _PPRZ_VAL_uint32_t(_payload, _offset) (((unaligned_t*)(_payload+_offset))->uint32)
#define _PPRZ_VAL_int64_t(_payload, _offset) (((unaligned_t*)(_payload+_offset))->int64)
#define _PPRZ_VAL_uint64_t(_payload, _offset) (((unaligned_t*)(_payload+_offset))->uint64)
#define _PPRZ_VAL_float(_payload, _offset) (((unaligned_t*)(_payload+_offset))->f32)
#ifndef __IEEE_BIG_ENDIAN
#define _PPRZ_VAL_double(_payload, _offset) (((unaligned_t*)(_payload+_offset))->f64)
#else
#define _PPRZ_VAL_double(_payload, _offset) ({ \
    union { uint64_t u; double f; } _f; \
    _f.u = (uint64_t)(_PPRZ_VAL_uint64_t(_payload, _offset)); \
    Swap32IfBigEndian(_f.u); \
    _f.f; })
#endif

// In this case, data is not aligned but we are still able to read them
#define _PPRZ_VAL_int16_t_array(_payload, _offset) (&_PPRZ_VAL_int16_t(_payload, _offset))
#define _PPRZ_VAL_uint16_t_array(_payload, _offset) (&_PPRZ_VAL_uint16_t(_payload, _offset))
#define _PPRZ_VAL_int32_t_array(_payload, _offset) (&_PPRZ_VAL_int32_t(_payload, _offset))
#define _PPRZ_VAL_uint32_t_array(_payload, _offset) (&_PPRZ_VAL_uint32_t(_payload, _offset))
#define _PPRZ_VAL_int64_t_array(_payload, _offset) (&_PPRZ_VAL_int64_t(_payload, _offset))
#define _PPRZ_VAL_uint64_t_array(_payload, _offset) (&_PPRZ_VAL_uint64_t(_payload, _offset))
#define _PPRZ_VAL_float_array(_payload, _offset) (&_PPRZ_VAL_float(_payload, _offset))
#define _PPRZ_VAL_double_array(_payload, _offset) (&_PPRZ_VAL_double(_payload, _offset))
#define _PPRZ_VAL_len_aligned(_payload, _offset) _PPRZ_VAL_uint8_t(_payload, _offset)
#define _PPRZ_VAL_fixed_len_aligned(_len) (_len)

#else // PPRZLINK_UNALIGNED_ACCESS set to false

#define _PPRZ_VAL_int16_t(_payload, _offset) ({ \
    union { int16_t i; uint8_t t[2]; } _r; \
    _r.t[0] = _PPRZ_VAL_uint8_t(_payload, _offset); \
    _r.t[1] = _PPRZ_VAL_uint8_t(_payload, _offset+1); \
    _r.i; })
#define _PPRZ_VAL_uint16_t(_payload, _offset) ({ \
    union { uint16_t i; uint8_t t[2]; } _r; \
    _r.t[0] = _PPRZ_VAL_uint8_t(_payload, _offset); \
    _r.t[1] = _PPRZ_VAL_uint8_t(_payload, _offset+1); \
    _r.i; })
#define _PPRZ_VAL_int32_t(_payload, _offset) ({ \
    union { int32_t i; uint8_t t[4]; } _r; \
    _r.t[0] = _PPRZ_VAL_uint8_t(_payload, _offset); \
    _r.t[1] = _PPRZ_VAL_uint8_t(_payload, _offset+1); \
    _r.t[2] = _PPRZ_VAL_uint8_t(_payload, _offset+2); \
    _r.t[3] = _PPRZ_VAL_uint8_t(_payload, _offset+3); \
    _r.i; })
#define _PPRZ_VAL_uint32_t(_payload, _offset) ({ \
    union { uint32_t i; uint8_t t[4]; } _r; \
    _r.t[0] = _PPRZ_VAL_uint8_t(_payload, _offset); \
    _r.t[1] = _PPRZ_VAL_uint8_t(_payload, _offset+1); \
    _r.t[2] = _PPRZ_VAL_uint8_t(_payload, _offset+2); \
    _r.t[3] = _PPRZ_VAL_uint8_t(_payload, _offset+3); \
    _r.i; })
#define _PPRZ_VAL_float(_payload, _offset) ({ \
    union { uint32_t u; float f; } _f; \
    _f.u = _PPRZ_VAL_uint32_t(_payload, _offset); \
    _f.f; })
#define _PPRZ_VAL_int64_t(_payload, _offset) ({ \
    union { int64_t i; uint8_t t[8]; } _r; \
    _r.t[0] = _PPRZ_VAL_uint8_t(_payload, _offset); \
    _r.t[1] = _PPRZ_VAL_uint8_t(_payload, _offset+1); \
    _r.t[2] = _PPRZ_VAL_uint8_t(_payload, _offset+2); \
    _r.t[3] = _PPRZ_VAL_uint8_t(_payload, _offset+3); \
    _r.t[4] = _PPRZ_VAL_uint8_t(_payload, _offset+4); \
    _r.t[5] = _PPRZ_VAL_uint8_t(_payload, _offset+5); \
    _r.t[6] = _PPRZ_VAL_uint8_t(_payload, _offset+6); \
    _r.t[7] = _PPRZ_VAL_uint8_t(_payload, _offset+7); \
    _r.i; })
#define _PPRZ_VAL_uint64_t(_payload, _offset) ({ \
    union { uint64_t i; uint8_t t[8]; } _r; \
    _r.t[0] = _PPRZ_VAL_uint8_t(_payload, _offset); \
    _r.t[1] = _PPRZ_VAL_uint8_t(_payload, _offset+1); \
    _r.t[2] = _PPRZ_VAL_uint8_t(_payload, _offset+2); \
    _r.t[3] = _PPRZ_VAL_uint8_t(_payload, _offset+3); \
    _r.t[4] = _PPRZ_VAL_uint8_t(_payload, _offset+4); \
    _r.t[5] = _PPRZ_VAL_uint8_t(_payload, _offset+5); \
    _r.t[6] = _PPRZ_VAL_uint8_t(_payload, _offset+6); \
    _r.t[7] = _PPRZ_VAL_uint8_t(_payload, _offset+7); \
    _r.i; })
#define _PPRZ_VAL_double(_payload, _offset) ({ \
    union { uint64_t u; double f; } _f; \
    _f.u = (uint64_t)(_PPRZ_VAL_uint64_t(_payload, _offset)); \
    Swap32IfBigEndian(_f.u); \
    _f.f; })

// Macros returning array pointers might not be aligned
// but there is not much we can do about it.
// To prevent errors, the array size is forced to zero
// on platforms where unaligned data is not supported.
#define _PPRZ_VAL_int16_t_array(_payload, _offset) NULL
#define _PPRZ_VAL_uint16_t_array(_payload, _offset) NULL
#define _PPRZ_VAL_int32_t_array(_payload, _offset) NULL
#define _PPRZ_VAL_uint32_t_array(_payload, _offset) NULL
#define _PPRZ_VAL_int64_t_array(_payload, _offset) NULL
#define _PPRZ_VAL_uint64_t_array(_payload, _offset) NULL
#define _PPRZ_VAL_float_array(_payload, _offset) NULL
#define _PPRZ_VAL_double_array(_payload, _offset) NULL
#define _PPRZ_VAL_len_aligned(_payload, _offset) (0)
#define _PPRZ_VAL_fixed_len_aligned(_len) (0)

#endif // PPRZLINK_UNALIGNED_ACCESS

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // PPRZLINK_UTILS_H
