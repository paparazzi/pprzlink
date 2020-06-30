(*
 * Generic communication protocol handling
 *
 * Copyright (C) 2004 CENA/ENAC, Pascal Brisset
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

type payload = string

let string_of_payload = fun x -> x
let payload_of_string = fun x -> x
let bytes_of_payload = fun x -> Bytes.of_string x
let payload_of_bytes = fun x -> Bytes.to_string x

exception Not_enough

module type PROTOCOL = sig
  val index_start : string -> int (* raise Not_found *)
  val length : string -> int -> int (* raise Not_enough *)
  val checksum : string -> bool
  val payload : string -> payload
  val packet : payload -> string
end

module Transport(Protocol:PROTOCOL) = struct
  let nb_err = ref 0
  let discarded_bytes = ref 0
  let rec parse = fun use buf ->
    DebugPL.call 'T' (fun f -> fprintf f "Transport.parse: %s\n" (DebugPL.xprint buf));
    (** ref required due to Not_enough exception raised by Protocol.length *)
    let start = ref 0
    and n = String.length buf in
    try
      (* Looks for the beginning of the frame. May raise Not_found exception *)
      start := Protocol.index_start buf;

      (* Discards skipped characters *)
      discarded_bytes := !discarded_bytes + !start;

      (* Get length of the frame (may raise Not_enough exception) *)
      let length = Protocol.length buf !start in
      let end_ = !start + length in

      (* Checks if the complete frame is available in the buffer. *)
      if n < end_ then
        raise Not_enough;

      (* Extracts the complete frame *)
      let msg = String.sub buf !start length in

      (* Checks sum *)
      if Protocol.checksum msg then begin
        (* Calls the handler with the message *)
        use (Protocol.payload msg)
      end else begin
        (* Reports the error *)
        incr nb_err;
        discarded_bytes := !discarded_bytes + length;
        DebugPL.call 'T' (fun f -> fprintf f "Transport.chk: %s\n" (DebugPL.xprint msg))
      end;

      (* Continues with the rest of the message *)
      end_ + parse use (String.sub buf end_ (String.length buf - end_))
    with
        Not_found -> String.length buf
      | Not_enough -> !start
end
