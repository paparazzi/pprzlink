(* Ocamlbuild plugin *)
open Ocamlbuild_plugin

let _ =
  dispatch & function
  | After_rules ->
    (* uncomment to compile any C files with -fPIC, should be default *)
    (*flag ["c"; "compile";] (S[A"-ccopt"; A"-fPIC"]);*)

    (* link a simple dependency, e.g. single object file: linkdep(foo.o) *)
    pdep ["link"] "linkdep" (fun param -> [param]);

    (* generate mix C/Caml library *)
    pflag ["ocamlmklib"; "c"] "linkclib" (fun param -> (S[A("-l"^param)]));

    (* Generate and link a library:
     * e.g. linklib(foo.a) to build libfoo.a from libfoo.clib
     *)
    (* add it as a dependency so the lib gets built *)
    pdep ["link"] "linklib" (fun param -> ["lib"^param]);
    (* link the lib in bytecode mode *)
    pflag ["link";"ocaml";"byte"] "linklib" (fun param ->
      let libname = String.sub param 0 (String.length param - 2) in
      (S[A"-dllib"; A("-l"^libname); A"-cclib"; A("-l"^libname)]));
    (* link the lib in native mode *)
    pflag ["link";"ocaml";"native"] "linklib" (fun param ->
      let libname = String.sub param 0 (String.length param - 2) in
      (S[A"-cclib"; A("-l"^libname)]));

    (* If `static' tag is given, then every ocaml link in bytecode will add -custom *)
    flag ["link"; "ocaml"; "byte"; "static"] (A"-custom");

    (* possibility to add defines for camlp4 in _tags file, e.g. define(FOO) *)
    pflag ["ocaml";"compile";] "define" (fun s -> S [A"-ppopt"; A ("-D"^s)]);
    pflag ["ocaml";"ocamldep";] "define" (fun s -> S [A"-ppopt"; A ("-D"^s)])
  | _ -> ()
