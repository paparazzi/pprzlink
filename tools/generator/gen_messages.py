#!/usr/bin/env python3

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

# XSD schema file => must be pprz_schema.xsd and be stored in the same directory as messages.xml
schemaFileName = "pprz_schema.xsd"

# Set defaults for generating MAVLink code
DEFAULT_PROTOCOL = pprz_parse.PROTOCOL_2_0
DEFAULT_MESSAGES = pprz_parse.MESSAGES_1_0
DEFAULT_LANGUAGE = 'C'
DEFAULT_VALIDATE = True

# List the supported languages. This is done globally because it's used by the GUI wrapper too
supportedLanguages = ["C", "C_standalone"]


def gen_messages(opts) :
    """Generate pprzlink message formatters and parsers using options.
    This function allows python scripts under Windows to control generators
    using the same interface as shell scripts under Unix"""

    fname = opts.definition

    # Enable validation by default, disabling it if explicitly requested
    if opts.validate:
        try:
            from lxml import etree
        except:
            print("WARNING: Unable to load XML validator libraries. XML validation will not be performed")
            opts.validate = False

    def pprz_validate(fname, schema) :
        """Uses minixsv to validate an XML file with a given XSD schema file. We define pprz_validate
           here because it relies on the XML libs that were loaded in gen_messages(), so it can't be called standalone"""
        try:
            xmlschema_doc = etree.parse(schema)
            xmlschema = etree.XMLSchema(xmlschema_doc)
            f_doc = etree.parse(fname)
            xmlschema.assertValid(f_doc)
        except Exception as errstr:
            print(errstr)
            return 1
        return 0

    # Process XML file, validating as necessary.
    validation_result = 0
    if opts.validate:
        import os.path
        directory = os.path.dirname(fname)
        schemaFile = os.path.join(directory, schemaFileName)
        if not os.path.isfile(schemaFile):
            print ("Schema file %s does not exist. Trying default path." % (schemaFile))
            schemaFile = os.path.join(os.path.dirname(os.path.realpath(__file__)), schemaFileName)
            if not os.path.isfile(schemaFile):
                print ("Schema default file %s does not exist. Leaving." % (schemaFile))
                sys.exit(1)
        print("Validating msg_class %s in %s with %s" % (opts.class_name, fname, schemaFile))

        validation_result = pprz_validate(fname, schemaFile)
    else:
        print("Validation skipped for msg_class %s in %s." % (opts.class_name, fname))

    print("Parsing %s" % fname)
    xml = pprz_parse.PPRZXML(fname, opts.class_name, opts.protocol)

    if pprz_parse.check_duplicates(xml):
        sys.exit(1)

    print("Found %u PPRZLink messages of msg_class %s in XML file %s" % (
        pprz_parse.total_msgs(xml), opts.class_name, fname))

    if opts.only_validate:
        sys.exit(validation_result)

    # Convert language option to lowercase and validate
    opts.language = opts.language.lower()
    if opts.language == 'c':
        gen_message_c = __import__(xml.generator_module + "_c")
        gen_message_c.generate(opts.output, xml)
    elif opts.language == 'c_standalone':
        gen_message_c_standalone = __import__(xml.generator_module + "_c_standalone")
        gen_message_c_standalone.generate(opts.output, xml, opts.opt)
    else:
        print("Unsupported language %s" % opts.language)


if __name__ == "__main__":
    import pprz_parse
    from argparse import ArgumentParser

    parser = ArgumentParser(description="This tool generate implementations from PPRZLink message definitions")
    parser.add_argument("-o", "--output", default="stdout", help="output file or stream [default: %(default)s]")
    parser.add_argument("--lang", dest="language", choices=supportedLanguages, default=DEFAULT_LANGUAGE, help="language of generated code [default: %(default)s]")
    parser.add_argument("--protocol", choices=[pprz_parse.PROTOCOL_1_0,pprz_parse.PROTOCOL_2_0], default=DEFAULT_PROTOCOL, help="PPRZLink protocol version. [default: %(default)s]")
    parser.add_argument("--messages", choices=[pprz_parse.MESSAGES_1_0], default=DEFAULT_MESSAGES, help="PPRZLink message definitino version. [default: %(default)s]")
    parser.add_argument("--no-validate", action="store_false", dest="validate", default=DEFAULT_VALIDATE, help="Do not perform XML validation. Can speed up code generation if XML files are known to be correct.")
    parser.add_argument("--only-validate", action="store_true", dest="only_validate", help="Only validate messages without generation.")
    parser.add_argument("--opt", default='', help="extra options that can be passed to the generator")
    parser.add_argument("definition", metavar="XML", help="PPRZLink messages definition")
    parser.add_argument("class_name", help="PPRZLink message class to parse and generate")
    args = parser.parse_args()

    gen_messages(args)

