(*
 * Copyright (C) 2006 ENAC, Pascal Brisset, Antoine Drouin
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

(** Utilities for XBee modules (AT and API modes) *)


(** AT commands *)
val at_command_sequence : string
val at_set_my : int -> string
val at_exit : string
val at_api_enable : string
val at_set_baud_rate : int -> string
val at_set_channel : int -> string


(** API protocol (payload is a frame_data) *)
module Transport : Protocol.PROTOCOL

(** The data inside the frames *)
type frame_data = string

(** Parsing frames sent by the module *)
type frame_id = int
type addr64 = Int64.t
type addr16 = int
type byte = int
type rssi = int
type frame =
    Modem_Status of byte
  | AT_Command_Response of frame_id * string * int * string
  | TX_Status of frame_id * byte (** [(frame_if, status)] *)
  | TX868_Status of frame_id * byte * int (** [(frame_if, status, nb_retries)] *)
  | RX_Packet_64 of addr64 * rssi * byte * string
  | RX868_Packet of addr64 * byte * string
  | RX_Packet_16 of addr16 * rssi * byte * string
val api_parse_frame : frame_data -> frame

(** Default to false *)
val mode868 : bool ref

(** Building API frames data *)
val api_tx64 : ?frame_id:int -> int64 -> bytes -> frame_data
val api_tx16 : ?frame_id:int -> int -> bytes -> frame_data


