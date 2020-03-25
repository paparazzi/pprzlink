(*
 * Reading logged timestamped messages
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

type message = {
  source : int;
  timestamp : int32;
  pprz_data : Protocol.payload
}


module Transport = struct
  let stx = Char.chr 0x99 (** pprz_transport.h *)
  let offset_length = 1
  let offset_source = 2
  let offset_timestamp = 3
  let offset_payload = 2

  let index_start = fun buf ->
    String.index buf stx

  let length = fun buf start ->
    let len = String.length buf - start in
    if len > offset_length then
      let l = Char.code buf.[start+offset_length] in
      DebugPL.call 'T' (fun f -> fprintf f "Pprzlog_transport len=%d\n" l);
      l + 8 (* pprz data + stx, length, source, time, ck *)
    else
      raise Protocol.Not_enough

  let (+=) = fun r x -> r := (!r + x) land 0xff
  let compute_checksum = fun msg ->
    let l = String.length msg in
    let ck_a = ref 0 in
    for i = 1 to l - 2 do
      ck_a += Char.code msg.[i]
    done;
    !ck_a

  let checksum = fun msg ->
    let l = String.length msg in
    let ck_a = compute_checksum msg in
    DebugPL.call 'T' (fun f -> fprintf f "Pprzlog_transport cs: %d %d\n" ck_a (Char.code msg.[l-1]));
    ck_a = Char.code msg.[l-1]

  let payload = fun msg ->
    let l = String.length msg in
    assert(Char.code msg.[offset_length] + 8 = l);
    Protocol.payload_of_string (String.sub msg offset_payload (l-3))

  let packet = fun payload ->
    let payload = Protocol.bytes_of_payload payload in
    let n = Bytes.length payload in
    let msg_length = n + 3 in (** + stx, len, ck_a *)
    if msg_length >= 256 then
      invalid_arg "Pprzlog_transport.Transport.packet";
    let m = Bytes.create msg_length in
    Bytes.blit payload 0 m offset_payload n;
    Bytes.set m 0 stx;
    Bytes.set m offset_length (Char.chr (msg_length - 8));
    let ck_a = compute_checksum (Bytes.to_string m) in
    Bytes.set m (msg_length-1) (Char.chr ck_a);
    Bytes.to_string m
end

let pprz_payload_of_payload = fun s ->
  let n = String.length s in
  Protocol.payload_of_string (String.sub s 5 (n - 5))

let parse = fun p ->
  let s = Protocol.string_of_payload p in
  let source = Char.code s.[0]
  and timestamp = PprzLink.int32_of_bytes (Bytes.of_string s) 1 in
  {
    source = source;
    timestamp = timestamp;
    pprz_data = pprz_payload_of_payload s
  }

