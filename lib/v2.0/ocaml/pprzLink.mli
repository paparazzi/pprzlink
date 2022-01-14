(*
 * PPRZLINK message protocol handling
 *
 * Copyright (C) 2003 Pascal Brisset, Antoine Drouin
 * Copyright (C) 2015-2020 Gautier Hattenberger <gautier.hattenberger@enac.fr>
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

val messages_xml : unit -> Xml.xml

val broadcast_id : int

type sender_id = int
type receiver_id = int
type component_id = int
type class_id = int
type message_id = int
type format = string

type _type =
    Scalar of string
  | ArrayType of string
  | FixedArrayType of string * int
type value =
    Int of int | Float of float | String of string | Int32 of int32 | Char of char | Int64 of int64
  | Array of value array
type field = {
    _type : _type;
    fformat : format;
    alt_unit_coef : string; (* May be empty *)
    enum : string list (* 'values' attribute *)
  }
type link_mode = Forwarded | Broadcasted

type header = {
    sender_id : sender_id;
    receiver_id : receiver_id;
    class_id : class_id;
    component_id: int;
    message_id : message_id;
  }

type message = {
    name : string;
    fields : (string * field) list;
    link : link_mode option
  }
(** Message specification *)


external int32_of_bytes : bytes -> int -> int32 = "c_int32_of_indexed_bytes"
external uint32_of_bytes : bytes -> int -> int64 = "c_uint32_of_indexed_bytes"
external int64_of_bytes : bytes -> int -> int64 = "c_int64_of_indexed_bytes"
(** [int32_of_bytes buffer offset] *)

val separator : string
(** Separator in array values *)

val is_array_type : string -> bool
val is_fixed_array_type : string -> bool

val size_of_field : field -> int
val string_of_value : value -> string
val formatted_string_of_value : format -> value -> string
val int_of_value : value -> int (* May raise Invalid_argument *)
type type_descr = {
    format : string ;
    glib_type : string;
    inttype : string;
    size : int;
    value : value
  }
val types : (string * type_descr) list
type values  = (string * value) list

val value : _type -> string -> value
(** return a value from a string and a type *)

val assoc : string -> values -> value
(** Safe assoc taking into accound characters case. May raise Failure ... *)

val string_assoc : string -> values -> string
(** May raise Not_found *)

val float_assoc : string -> values -> float
val int_assoc : string -> values -> int
val int32_assoc : string -> values -> Int32.t
val uint32_assoc : string -> values -> Int64.t
val int64_assoc : string -> values -> Int64.t
(** May raise Not_found or Invalid_argument *)

val hex_of_int_array : value -> string
(** Returns the hexadecimal string of an array of integers *)

exception Unit_conversion_error of string
(** Unit_conversion_error raised when parsing error occurs *)
exception Unknown_conversion of string * string
(** Unknown_conversion raised when conversion fails *)
exception No_automatic_conversion of string * string
(** No_automatic_conversion raised when no conversion found
 *  and from_unit or to_unit are empty string
 *)

val scale_of_units : ?auto:string -> string -> string -> float
(** scale_of_units from to
 *  Returns conversion factor between two units
 *  The possible conversions are described in conf/units.xml
 *  May raise Invalid_argument if one of the unit is not valid
 *  or if units.xml is not valid
 *)

val alt_unit_coef_of_xml : ?auto:string -> Xml.xml -> string
(** Return coef for alternate unit
 *)
exception Error of string
(** Generic error *)

val xml_attrib : Xml.xml -> string -> string
(** Extended version of Xml.attrib, may rise Error *)

val xml_int_attrib : Xml.xml -> string -> int
val xml_float_attrib : Xml.xml -> string -> float

val xml_child : Xml.xml -> ?select:(Xml.xml -> bool) -> string -> Xml.xml
(** [child xml ?p i] If [i] is an integer, returns the [i]'th (first is 0) child of [xml].
Else returns the child of [xml] with tag [i] (the first one satisfying [p]
if specified). Else raises [Not_found]. *)

exception Unknown_msg_name of string * string
(** [Unknown_msg_name (name, class_name)] Raised if message [name] is not
found in class [class_name]. *)

val offset_fields : int

module type CLASS = sig
  val name : string
end

module type CLASS_Xml = sig
  val xml : Xml.xml
  val name : string
end

module type MESSAGES = sig
  val messages : (message_id, message) Hashtbl.t
  val message_of_id : message_id -> message
  val message_of_name : string ->  message_id * message

  val values_of_payload : Protocol.payload -> header * values
  (** [values_of_payload payload] Parses a raw payload, returns the
   header and the list of (field_name, value) *)

  val payload_of_values : message_id -> sender_id -> receiver_id -> values -> Protocol.payload
  (** [payload_of_values id sender_id receiver_id vs] Returns a payload *)

  val values_of_string : string -> message_id * values
  (** May raise [(Unknown_msg_name msg_name)] *)

  val string_of_message : ?sep:string -> message -> values -> string
  (** [string_of_message ?sep msg values] Default [sep] is space *)

  val json_of_message : message -> values -> string
  (** [json_of_message msg values] JSON string of message *)

  val message_send : ?timestamp:float -> ?link_id:int -> string -> string -> values -> unit
  (** [message_send sender msg_name values] *)

  val message_bind : ?sender:string -> ?timestamp:bool -> string -> (string -> values -> unit) -> Ivy.binding
  (** [message_bind ?sender msg_name callback] *)

  val message_answerer : string -> string -> (string -> values -> values) -> Ivy.binding
  (** [message_answerer sender msg_name callback] Set a handler for a
      [message_req] (which will send a [msg_name]_REQ message).
      [callback asker args] must return the list of attributes of the answer. *)

  val message_req : string -> string -> values -> (string -> values -> unit) -> Ivy.binding * bool ref
  (** [message_req sender msg_name values receiver] Sends a request on the Ivy
      bus for the specified message. A [msg_name]_REQ message is send and a
      [msg_name] message is expected for the reply. On reception, [receiver]
      will be applied on [sender_name] and attribute values of the values.
      Message are unbinded automatically only when getting the answer,
      timeout should be implemenet on application side. *)
end

module Messages : functor (Class : CLASS) -> MESSAGES
module MessagesOfXml : functor (Class : CLASS_Xml) -> MESSAGES
