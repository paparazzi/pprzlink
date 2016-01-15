#!/usr/bin/env python

'''
parse a PPRZ protocol XML file and generate appropriate implementation

Copyright (C) 2015 Gautier Hattenberger <gautier.hattenberger@enac.fr>
For the Paparazzi UAV and PPRZLINK projects

based on:
    Copyright Andrew Tridgell 2011
    Released under GNU GPL version 3 or later


'''
import sys, textwrap, os
import pprz_parse

# XSD schema file
schemaFile = os.path.join(os.path.dirname(os.path.realpath(__file__)), "pprz_schema.xsd")

# Set defaults for generating MAVLink code
DEFAULT_PROTOCOL = pprz_parse.PROTOCOL_1_0
DEFAULT_LANGUAGE = 'C'
DEFAULT_ERROR_LIMIT = 200
DEFAULT_VALIDATE = True

# List the supported languages. This is done globally because it's used by the GUI wrapper too
supportedLanguages = ["C"]


def gen_messages(opts) :
    """Generate pprzlink message formatters and parsers using options.
    This function allows python scripts under Windows to control generators
    using the same interface as shell scripts under Unix"""

    fname = opts.definition

    # Enable validation by default, disabling it if explicitly requested
    if opts.validate:
        try:
            #from lib.genxmlif import GenXmlIfError
            from lib.minixsv import pyxsval
        except:
            print("WARNING: Unable to load XML validator libraries. XML validation will not be performed")
            opts.validate = False

    def pprz_validate(fname, schema, errorLimitNumber) :
        """Uses minixsv to validate an XML file with a given XSD schema file. We define pprz_validate
           here because it relies on the XML libs that were loaded in gen_messages(), so it can't be called standalone"""
        # use default values of minixsv, location of the schema file must be specified in the XML file
        domTreeWrapper = pyxsval.parseAndValidate(fname, xsdFile=schema, errorLimit=errorLimitNumber)

    # Process XML file, validating as necessary.
    if opts.validate:
        print("Validating %s" % fname)
        pprz_validate(fname, schemaFile, opts.error_limit);
    else:
        print("Validation skipped for %s." % fname)

    print("Parsing %s" % fname)
    xml = pprz_parse.PPRZXML(fname, opts.class_name, opts.protocol)

    if pprz_parse.check_duplicates(xml):
        sys.exit(1)

    print("Found %u PPPRZLink message types in XML file %s" % (
        pprz_parse.total_msgs(xml), fname))

    # Convert language option to lowercase and validate
    opts.language = opts.language.lower()
    if opts.language == 'c':
        import gen_messages_c
        gen_messages_c.generate(opts.output, xml)
    else:
        print("Unsupported language %s" % opts.language)


if __name__ == "__main__":
    import pprz_parse
    from argparse import ArgumentParser

    parser = ArgumentParser(description="This tool generate implementations from PPRZLink message definitions")
    parser.add_argument("-o", "--output", default="stdout", help="output file or stream [default: %(default)s]")
    parser.add_argument("--lang", dest="language", choices=supportedLanguages, default=DEFAULT_LANGUAGE, help="language of generated code [default: %(default)s]")
    parser.add_argument("--protocol", choices=[pprz_parse.PROTOCOL_1_0], default=DEFAULT_PROTOCOL, help="PPRZLink protocol version. [default: %(default)s]")
    parser.add_argument("--no-validate", action="store_false", dest="validate", default=DEFAULT_VALIDATE, help="Do not perform XML validation. Can speed up code generation if XML files are known to be correct.")
    parser.add_argument("--error-limit", default=DEFAULT_ERROR_LIMIT, help="maximum number of validation errors to display")
    parser.add_argument("definition", metavar="XML", help="PPRZLink messages definition")
    parser.add_argument("class_name", help="PPRZLink message class to parse and generate")
    args = parser.parse_args()

    gen_messages(args)

