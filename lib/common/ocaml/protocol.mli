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

type payload

val string_of_payload : payload -> string
val payload_of_string : string -> payload
val bytes_of_payload : payload -> bytes
val payload_of_bytes : bytes -> payload

exception Not_enough

module type PROTOCOL =
  sig
    val index_start : string -> int
    (** Must return the index of the first char of the the first message.
       May raise Not_found or Not_enough *)

    val length : string -> int -> int
    (** [length buf start] Must return the length of the message starting at
       [start]. May raise Not_enough *)

    val checksum : string -> bool
    (** [checksum message] *)

    val payload : string -> payload
    val packet : payload -> string
  end

(** Builds a parser module from a [PROTOCOL] description *)
module Transport :
  functor (Protocol : PROTOCOL) ->
    sig
      val nb_err : int ref (* Errors on checksum *)
      val discarded_bytes : int ref
      val parse : (payload -> unit) -> string -> int
      (** [parse f buf] Scans [buf] according to [Protocol] and applies [f] on
       payload of every recognised message. Returns the number of consumed bytes. *)
    end
