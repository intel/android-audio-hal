#! /usr/bin/python
#
# Copyright (c) 2011-2017, Intel Corporation
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation and/or
# other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
# may be used to endorse or promote products derived from this software without
# specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import EddParser
from PFWScriptGenerator import PfwScriptTranslator
import hostConfig

import argparse
import re
import sys
import tempfile
import os
import logging
import subprocess
import xml.etree.ElementTree as ET

def parseArgs():
    argparser = argparse.ArgumentParser(description="Parameter-Framework XML \
        Settings file generator.\n\
        Exit with the number of (recoverable or not) error that occured.")
    argparser.add_argument('--toplevel-config',
            help="Top-level parameter-framework configuration file. Mandatory.",
            metavar="TOPLEVEL_CONFIG_FILE",
            required=True)
    argparser.add_argument('--criteria',
            help="Criteria file, either in text format if no criterion types file is provided: \
                  in '<type> <name> : <value> <value...>' \
                  or in XML format if a criterion types XML file is provided: \
                  in '<criteria> \
                          <criterion name="" type=""/> \
                      </criteria>' \
        format. Mandatory.",
            metavar="CRITERIA_FILE",
            type=argparse.FileType('r'),
            required=True)
    argparser.add_argument('--criteriontypes',
            help="Criterion types XML file, in \
            '<criterion_types> \
                <criterion_type name="" type=<inclusive|exclusive> values=<value1,value2,...>/> \
             </criterion_types>' \
        format. Mandatory.",
            metavar="CRITERION_TYPE_FILE",
            type=argparse.FileType('r'),
            required=False)
    argparser.add_argument('--initial-settings',
            help="Initial XML settings file (containing a \
        <ConfigurableDomains>  tag",
            nargs='?',
            default=None,
            metavar="XML_SETTINGS_FILE")
    argparser.add_argument('--add-domains',
            help="List of single domain files (each containing a single \
        <ConfigurableDomain> tag",
            metavar="XML_DOMAIN_FILE",
            nargs='*',
            dest='xml_domain_files',
            default=[])
    argparser.add_argument('--add-edds',
            help="List of files in EDD syntax (aka \".pfw\" files)",
            metavar="EDD_FILE",
            type=argparse.FileType('r'),
            nargs='*',
            default=[],
            dest='edd_files')
    argparser.add_argument('--schemas-dir',
            help="Directory of parameter-framework XML Schemas for generation \
        validation",
            default=None)
    argparser.add_argument('--target-schemas-dir',
            help="Ignored. Kept for retro-compatibility")
    argparser.add_argument('--validate',
            help="Validate the settings against XML schemas",
            action='store_true')
    argparser.add_argument('--verbose',
            action='store_true')

    return argparser.parse_args()

def parseCriteria(criteriaFile):
    # Parse a criteria file
    #
    # This file define one criteria per line; they should respect this format:
    #
    # <type> <name> : <values>
    #
    # Where <type> is 'InclusiveCriterion' or 'ExclusiveCriterion';
    #       <name> is any string w/o whitespace
    #       <values> is a list of whitespace-separated values, each of which is any
    #                string w/o a whitespace
    criteria_pattern = re.compile(
        r"^(?P<type>(?:Inclusive|Exclusive)Criterion)\s*" \
        r"(?P<name>\S+)\s*:\s*" \
        r"(?P<values>.*)$")
    criterion_inclusiveness_table = {
        'InclusiveCriterion' : "inclusive",
        'ExclusiveCriterion' : "exclusive"}

    all_criteria = []
    for line_number, line in enumerate(criteriaFile):
        match = criteria_pattern.match(line)
        if not match:
            raise ValueError("The following line is invalid: {}:{}\n{}".format(
                criteriaFile.name, line_number, line))

        criterion_name = match.groupdict()['name']
        criterion_type = match.groupdict()['type']
        criterion_values = re.split("\s*", match.groupdict()['values'])

        criterion_inclusiveness = criterion_inclusiveness_table[criterion_type]

        all_criteria.append({
            "name" : criterion_name,
            "inclusive" : criterion_inclusiveness,
            "values" : criterion_values})

    return all_criteria

def parseCriteriaAndCriterionTypes(criteriaFile, criterionTypesFile):
    # Parse criteria and criterion types XML files
    #
    criteria_tree = ET.parse(criteriaFile)
    logging.info("Importing criteriaFile {}".format(criteriaFile))
    criterion_types_tree = ET.parse(criterionTypesFile)
    logging.info("Importing criterionTypesFile {}".format(criterionTypesFile))

    criteria_root = criteria_tree.getroot()
    criterion_types_root = criterion_types_tree.getroot()

    all_criteria = []
    for criterion in criteria_root.findall('criterion'):
        criterion_name = criterion.get('name')
        type_name = criterion.get('type')
        logging.info("Importing criterion_name {}".format(criterion_name))
        logging.info("Importing type_name {}".format(type_name))

        for criterion_types in criterion_types_tree.findall('criterion_type'):
            criterion_type_name = criterion_types.get('name')
            if criterion_type_name == type_name:
                criterion_inclusiveness = criterion_types.get('type')
                values = criterion_types.get('values')
                criterion_values = re.split(',', values)

                logging.info("Importing criterion_type_name {}".format(criterion_type_name))
                logging.info("Importing criterion_inclusiveness {}".format(criterion_inclusiveness))
                logging.info("Importing criterion_values {}".format(criterion_values))

                all_criteria.append({
                    "name" : criterion_name,
                    "inclusive" : criterion_inclusiveness,
                    "values" : criterion_values})
                break

    return all_criteria

def parseEdd(EDDFiles):
    parsed_edds = []

    for edd_file in EDDFiles:
        try:
            root = EddParser.Parser().parse(edd_file)
        except EddParser.MySyntaxError as ex:
            logging.critical(str(ex))
            logging.info("EXIT ON FAILURE")
            exit(2)

        try:
            root.propagate()
        except EddParser.MyPropagationError, ex :
            logging.critical(str(ex))
            logging.info("EXIT ON FAILURE")
            exit(1)

        parsed_edds.append((edd_file.name, root))
    return parsed_edds

def generateDomainCommands(logging, all_criteria, initial_settings, xml_domain_files, parsed_edds):
        # create and inject all the criteria
        logging.info("Creating all criteria")
        for criterion in all_criteria:
            yield ["createSelectionCriterion", criterion['inclusive'],
                   criterion['name']] + criterion['values']

        yield ["start"]

        # Import initial settings file
        if initial_settings:
            logging.info("Importing initial settings file {}".format(initial_settings))
            yield ["importDomainsWithSettingsXML", initial_settings]

        # Import each standalone domain files
        for domain_file in xml_domain_files:
            logging.info("Importing single domain file {}".format(domain_file))
            yield ["importDomainWithSettingsXML", domain_file]

        # Generate the script for each EDD file
        for filename, parsed_edd in parsed_edds:
            logging.info("Translating and injecting EDD file {}".format(filename))
            translator = PfwScriptTranslator()
            parsed_edd.translate(translator)
            for command in translator.getScript():
                yield command

def main():
    logging.root.setLevel(logging.INFO)
    args = parseArgs()

    if not args.criteriontypes:
        all_criteria = parseCriteria(args.criteria, args.criteriontypes)
    else :
        all_criteria = parseCriteriaAndCriterionTypes(args.criteria, args.criteriontypes)

    #
    # EDD files (aka ".pfw" files)
    #
    parsed_edds = parseEdd(args.edd_files)

    # We need to modify the toplevel configuration file to account for differences
    # between development setup and target (installation) setup, in particular, the
    # TuningMwith ode must be enforced, regardless of what will be allowed on the target
    fake_toplevel_config = tempfile.NamedTemporaryFile(mode='w', delete=False, suffix=".xml",
                                                       prefix="TMPdomainGeneratorPFConfig_")

    install_path = os.path.dirname(os.path.realpath(args.toplevel_config))
    hostConfig.configure(
            infile=args.toplevel_config,
            outfile=fake_toplevel_config,
            structPath=install_path)
    fake_toplevel_config.close()

    # Create the connector. Pipe its input to us in order to write commands;
    # connect its output to stdout in order to have it dump the domains
    # there; connect its error output to stderr.
    connector = subprocess.Popen(["domainGeneratorConnector",
                            fake_toplevel_config.name,
                            'verbose' if args.verbose else 'no-verbose',
                            'validate' if args.validate else 'no-validate',
                            args.schemas_dir],
                           stdout=sys.stdout, stdin=subprocess.PIPE, stderr=sys.stderr)

    initial_settings = None
    if args.initial_settings:
        initial_settings = os.path.realpath(args.initial_settings)

    for command in generateDomainCommands(logging, all_criteria, initial_settings,
                                       args.xml_domain_files, parsed_edds):
        connector.stdin.write('\0'.join(command))
        connector.stdin.write("\n")

    # Closing the connector's input triggers the domain generation
    connector.stdin.close()
    connector.wait()
    os.remove(fake_toplevel_config.name)
    return connector.returncode

# If this file is directly executed
if __name__ == "__main__":
    exit(main())
