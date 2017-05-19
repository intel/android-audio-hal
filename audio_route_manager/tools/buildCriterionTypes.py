#! /usr/bin/python
#
# Copyright (c) 2017, Intel Corporation
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
        audio criterion type file generator.\n\
        Exit with the number of (recoverable or not) error that occured.")
    argparser.add_argument('--routes',
            help="Route Manager Configration file, in \
            '<mixPorts> \
                <mixPort> name="" role=<sink|source> ...>' \
        format. Mandatory.",
            metavar="ROUTES_FILE",
            type=argparse.FileType('r'),
            required=True)
    argparser.add_argument('--criteriontypes',
            help="Criterion types file, in \
            '<criterion_types> \
                <criterion_type name="" type=<inclusive|exclusive> values=<value1,value2,...>/>' \
        format. Mandatory.",
            metavar="CRITERION_TYPE_FILE",
            type=argparse.FileType('r'),
            required=True)
    argparser.add_argument('--outputfile',
            help="Criterion types outputfile file. Mandatory.",
            metavar="CRITERION_TYPE_OUTPUT_FILE",
            type=argparse.FileType('w'),
            required=True)
    argparser.add_argument('--verbose',
            action='store_true')

    return argparser.parse_args()

def parseCriteria(routesFile, criterionTypesFile, outputFile):
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
    routes_tree = ET.parse(routesFile)
    logging.info("Importing criteriaFile {}".format(routesFile))
    criterion_types_tree = ET.parse(criterionTypesFile)
    logging.info("Importing criterionTypesFile {}".format(criterionTypesFile))

    routes_root = routes_tree.getroot()
    criterion_types_root = criterion_types_tree.getroot()

    modules_root = routes_root.find('modules')
    for module in modules_root.findall('module'):
        if module.get('name') == "primary":
            routes_mix_ports_root = module.find('mixPorts')
            break

    all_playback_routes = ""
    all_capture_routes = ""
    for route in routes_mix_ports_root.findall('mixPort'):
        route_name = route.get('name')
        route_role = route.get('role')
        if route_role == "source":
            if all_playback_routes:
                all_playback_routes += ","
            all_playback_routes += route_name
        if route_role == "sink":
            if all_capture_routes:
                all_capture_routes += ","
            all_capture_routes += route_name

    logging.info("adding {} to RoutePlaybackType criterion type".format(all_playback_routes))
    logging.info("adding {} to RouteCaptureType criterion type".format(all_capture_routes))

    for criterion_type in criterion_types_tree.findall('criterion_type'):
        criterion_type_name = criterion_type.get('name')
        if criterion_type_name == "RoutePlaybackType":
            criterion_type.set('values', all_playback_routes)
        if criterion_type_name == "RouteCaptureType":
            criterion_type.set('values', all_capture_routes)

    logging.info("Generating outputFile {}".format(outputFile))
    criterion_types_tree.write(outputFile)

def main():
    logging.root.setLevel(logging.INFO)
    args = parseArgs()

    parseCriteria(args.routes, args.criteriontypes, args.outputfile)

# If this file is directly executed
if __name__ == "__main__":
    exit(main())
