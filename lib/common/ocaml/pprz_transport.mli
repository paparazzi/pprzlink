(*
 * Paparazzi protocol handling (v1.0)
 *
 * Copyright (C) 2003 Pascal Brisset, Antoine Drouin
 * Copyright (C) 2015 Gautier Hattenberger <gautier.hattenberger@enac.fr>
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
 *)

module Transport : Protocol.PROTOCOL
(** PprzLink frame (sw/airborne/pprz_transport.h):
    |STX|length|... payload=(length-4) bytes ...|Checksum A|Checksum B|
    Where checksum is computed over length and payload:
    ck_A = ck_B = 0;
    for all byte b in payload
      ck_A += b; ck_b += ck_A

    STX = 0x99
    [packet] raises Invalid_Argument if length >= 256
 *)


