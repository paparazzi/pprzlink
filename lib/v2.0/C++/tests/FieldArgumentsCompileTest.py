#!/usr/bin/env python3
"""Check GCC diagnostics for invalid public Message::setField calls."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--include-dirs', required=True)
    args = parser.parse_args()
    command = [args.compiler, '-std=c++23', '-fsyntax-only', '-fdiagnostics-color=never']
    command.extend('-I' + path for path in args.include_dirs.split(';') if path)
    environment = dict(os.environ, LC_ALL='C')

    # The positive control prevents a missing header or broken compiler from
    # accidentally making all the negative cases pass.
    cases = [
        ('valid', '''
            message.setField("index", 7);
            message.setField<int>("index", 7);
            message.setField("index", 7, std::string("ac_id"), 42, "value", 12.5);
            message.setField("label", "settings", "samples", std::vector<int>{1, 2});
        ''', None),
        ('missing_value', 'message.setField("index", 7, "ac_id");',
         r'required for the satisfaction of .EvenNumberOfFieldArguments<'),
        ('missing_third_value', 'message.setField("index", 7, "ac_id", 42, "value");',
         r'required for the satisfaction of .EvenNumberOfFieldArguments<'),
        ('first_key', 'message.setField(17, 7);', r'cannot convert .*17.*const std::string'),
        ('middle_key', 'message.setField("index", 7, 42, 12.5);',
         r'required for the satisfaction of .FieldNamesAreStrings<'),
        ('last_key', 'message.setField("index", 7, "ac_id", 42, true, 12.5);',
         r'required for the satisfaction of .FieldNamesAreStrings<'),
    ]

    with tempfile.TemporaryDirectory(prefix='pprzlink-field-arguments-') as temporary:
        for name, statement, diagnostic in cases:
            source = Path(temporary) / (name + '.cpp')
            source.write_text('#include <pprzlink/Message.h>\n'
                              'void check(pprzlink::Message& message) {\n' + statement + '\n}\n')
            result = subprocess.run(command + [str(source)], env=environment,
                                    capture_output=True, text=True, timeout=20)
            if diagnostic is None:
                assert result.returncode == 0, result.stderr
            else:
                assert result.returncode != 0, f'{name}: invalid call compiled successfully'
                assert re.search(diagnostic, result.stderr), result.stderr
                assert str(source) + ':3:' in result.stderr, result.stderr
            print(f'{name}: passed', flush=True)


if __name__ == '__main__':
    main()
