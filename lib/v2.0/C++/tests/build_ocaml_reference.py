#!/usr/bin/env python3
"""Build the real Paparazzi link.ml in an isolated directory for differential tests.

Requires OCaml 4.14, xml-light, lablgtk3 (including cairo2), the Ivy OCaml
bindings' sources, and Ivy C/GLib. No Paparazzi build products are overwritten.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--paparazzi', type=Path, required=True)
    parser.add_argument('--ivy-ocaml', type=Path, required=True)
    parser.add_argument('--ivy-prefix', type=Path, required=True)
    parser.add_argument('--ocaml-packages', type=Path, default=Path('/usr/lib/ocaml'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    pprz = args.paparazzi / 'sw/ext/pprzlink'
    sources = []
    for module in ['debug', 'serial', 'latlong', 'os_calls', 'defivybus']:
        sources += [(args.paparazzi / 'sw/lib/ocaml', module)]
    sources += [(pprz / 'lib/common/ocaml', name) for name in
                ['debugPL', 'protocol', 'pprz_transport', 'xbee_transport']]
    sources += [(pprz / 'lib/v2.0/ocaml', 'pprzLink')]
    sources += [(args.paparazzi / 'sw/ground_segment/tmtc', 'link')]
    sources.insert(0, (args.ivy_ocaml, 'ivy'))
    originals = {}
    for directory, module in sources:
        for suffix in ['.mli', '.ml']:
            src = directory / (module + suffix)
            if src.exists():
                shutil.copyfile(src, out / src.name)
                originals[str(src)] = hashlib.sha256(src.read_bytes()).hexdigest()
    # Mechanical safe-string migration in the old Ivy binding's two unused
    # Marshal helpers. link.ml, pprzLink.ml and transport logic remain unchanged.
    ivy = out / 'ivy.ml'
    text = ivy.read_text().replace('String.create', 'Bytes.create')
    text = text.replace('h.[2*i] <- hexa_char (c lsr 4);', 'Bytes.set h (2*i) (hexa_char (c lsr 4));')
    text = text.replace('h.[2*i+1] <- hexa_char (c land 0xf)', 'Bytes.set h (2*i+1) (hexa_char (c land 0xf))')
    text = text.replace('s.[i] <- Char.chr (hexa_code h.[2*i] lsl 4 + hexa_code h.[2*i+1])',
                        'Bytes.set s i (Char.chr (hexa_code h.[2*i] lsl 4 + hexa_code h.[2*i+1]))')
    text = text.replace('done;\n  h', 'done;\n  Bytes.to_string h').replace('done;\n  s', 'done;\n  Bytes.to_string s')
    ivy.write_text(text)
    include = []
    for name in ['xml-light', 'cairo2', 'lablgtk3', 'stublibs']:
        include += ['-I', str((args.ocaml_packages / name).resolve())]

    def run(command):
        print(' '.join(map(str, command)), flush=True)
        subprocess.run(command, cwd=out, check=True)

    glib_flags = subprocess.check_output(['pkg-config', '--cflags', 'glib-2.0'], text=True).split()
    c_sources = [args.ivy_ocaml / name for name in ['civy.c', 'civyloop.c', 'cglibivy.c']]
    c_sources += [pprz / 'lib/common/ocaml/convert.c', args.paparazzi / 'sw/lib/ocaml/cserial.c']
    for src in c_sources:
        shutil.copyfile(src, out / src.name)
        originals[str(src)] = hashlib.sha256(src.read_bytes()).hexdigest()
        flags = ['-fPIC', '-I' + str(args.ivy_prefix / 'include')] + glib_flags
        run(['ocamlc', '-cc', '/usr/bin/gcc-13', '-ccopt', ' '.join(flags), '-c', src.name])
    run(['ocamlmklib', '-o', 'reference_stubs'] + [src.stem + '.o' for src in c_sources] +
        ['-L' + str(args.ivy_prefix / 'lib'), '-lglibivy', '-lglib-2.0', '-lpcre2-8'])
    for _, module in sources:
        if (out / (module + '.mli')).exists():
            run(['ocamlc', '-g'] + include + ['-c', module + '.mli'])
        run(['ocamlc', '-g'] + include + ['-c', module + '.ml'])
    run(['ocamlc', '-g', '-o', 'link_ocaml'] + include +
        ['unix.cma', 'str.cma', 'xml_light.cma', 'cairo.cma', 'lablgtk3.cma'] +
        [module + '.cmo' for _, module in sources] + ['-dllib', '-lreference_stubs', '-dllpath', str(out),
         '-dllpath', str((args.ocaml_packages / 'stublibs').resolve())])
    (out / 'source-sha256.json').write_text(json.dumps(originals, indent=2) + '\n')
    print(out / 'link_ocaml')


if __name__ == '__main__':
    main()
