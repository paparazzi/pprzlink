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

open Printf


module type TRANSPORT_TYPE = sig
  val stx : char
  val offset_length : int
  val offset_payload : int
  val offset_timestamp : int
  val overhead_length : int
end

module PprzTransportBase(SubType:TRANSPORT_TYPE) = struct
  let index_start = fun buf ->
    CompatPL.bytes_index buf SubType.stx

  let length = fun buf start ->
    let len = CompatPL.bytes_length buf - start in
    if len > SubType.offset_length then
      let l = Char.code buf.[start+SubType.offset_length] in
      DebugPL.call 'T' (fun f -> fprintf f "Pprz_transport len=%d\n" l);
      max l SubType.overhead_length (** if l<4 (4=stx+length+ck_a+ck_b), it's not a valid length *)
    else
      raise Protocol.Not_enough

  let (+=) = fun r x -> r := (!r + x) land 0xff
  let compute_checksum = fun msg ->
    let l = CompatPL.bytes_length msg in
    let ck_a = ref 0  and ck_b = ref 0 in
    for i = 1 to l - 3 do
      ck_a += Char.code msg.[i];
      ck_b += !ck_a
    done;
    !ck_a, !ck_b

  let checksum = fun msg ->
    let l = CompatPL.bytes_length msg in
    let ck_a, ck_b = compute_checksum msg in
    DebugPL.call 'T' (fun f -> fprintf f "Pprz_transport cs: %d %d | %d %d\n" ck_a (Char.code msg.[l-2]) ck_b (Char.code msg.[l-1]));
    ck_a = Char.code msg.[l-2] && ck_b = Char.code msg.[l-1]

  let payload = fun msg ->
    let l = CompatPL.bytes_length msg in
    assert(Char.code msg.[SubType.offset_length] = l);
    assert(l >= SubType.overhead_length);
    Protocol.payload_of_string (CompatPL.bytes_sub msg SubType.offset_payload (l-SubType.overhead_length))

  let packet = fun payload ->
    let payload = Protocol.string_of_payload payload in
    let n = CompatPL.bytes_length payload in
    let msg_length = n + SubType.overhead_length in (** + stx, len, ck_a and ck_b *)
    if msg_length >= 256 then
      invalid_arg "Pprz_transport.Transport.packet";
    let m = CompatPL.bytes_create msg_length in
    CompatPL.bytes_blit payload 0 m SubType.offset_payload n;
    CompatPL.bytes_set m 0 SubType.stx;
    CompatPL.bytes_set m SubType.offset_length (Char.chr msg_length);
    let (ck_a, ck_b) = compute_checksum m in
    CompatPL.bytes_set m (msg_length-2) (Char.chr ck_a);
    CompatPL.bytes_set m (msg_length-1) (Char.chr ck_b);
    m
end

module PprzTypeStandard = struct
  let stx = Char.chr 0x99
  let offset_length = 1
  let offset_timestamp = -1
  let offset_payload = 2
  let overhead_length = 4
end

module Transport = PprzTransportBase (PprzTypeStandard)

(*module PprzTypeTimestamp = struct
  let stx = Char.chr 0x98
  let offset_length = 1
  let offset_timestamp = 2
  let offset_payload = 6
  let overhead_length = 8
end*)

(*module TransportExtended = PprzTransportBase (PprzTypeTimestamp)*)
