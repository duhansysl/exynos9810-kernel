#!/usr/bin/env python3
#
# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Sample Usage:
# $ python3 create_orderfile.py --profile-file ../orderfiles/test/example.prof --mapping-file ../orderfiles/test/example-mapping.txt
#
# Try '-h' for a full list of command line arguments.

import argparse
import orderfile_utils

def parse_args():
    """Parses and returns command line arguments."""
    parser = argparse.ArgumentParser(prog="create_orderfile",
                                    description="Create orderfile from profile file and mapping file")

    parser.add_argument(
        "--profile-file",
        required=True,
        help="Parsed profile file that represents the order of the symbol execution")

    parser.add_argument(
        "--mapping-file",
        required=True,
        help="Mapped file that provides the mapping between MD5 hash and symbol name")

    parser.add_argument(
        "--output",
        default="default.orderfile",
        help="Provide the output file name for the order file. Default Name: default.orderfile")

    parser.add_argument(
        "--denylist",
        default="",
        help=f"Exclude symbols based on a symbol-per-line file with @ or comma separarted values within a quotation."
             f"For example, you can say @file.txt or 'main,bar,foo'")

    parser.add_argument(
        "--last-symbol",
        help=f"Create an order file until the passed last symbol and ignore the symbols after it."
             f"Useful if you want an order file only for startup so you should pass the last startup symbol."
             f"Last-symbol has priority over leftover so we will output until the last symbol and ignore the leftover flag.")

    parser.add_argument(
        "--leftover",
        action='store_true',
        default=False,
        help="Add the symbols seen in mapping file but not in profile file at the end")

    return parser.parse_args()

def main():
    args = parse_args()

    symbols = []
    mapping = {}
    seen = set()
    denylist = orderfile_utils.parse_set(args.denylist)

    # Load the MD5 hash mappings of the symbols.
    with open(args.mapping_file, "r") as f:
        for line in f:
            line = line.strip().split()
            mapping[line[1]] = line[2]

    # Parse the profile file
    with open(args.profile_file, "r") as f:
        for line in f:
            line = line.strip().split()

            # Every line should have 2 MD5 hashes in reverse order (little Endian)
            # so we need to reverse them to get the actual md5 hashes
            if len(line) >= 8:
                md5_1_b_list = line[1:9]
                md5_2_b_list = line[9:17]

                md5_1_b_list.reverse()
                md5_2_b_list.reverse()

                md5_1 = "".join(md5_1_b_list)
                md5_2 = "".join(md5_2_b_list)

                if(md5_1 in mapping):
                    symbol_1 = mapping[md5_1]
                    seen.add(symbol_1)

                    if symbol_1 not in denylist:
                        symbols.append(symbol_1)

                if(md5_2 in mapping):
                    symbol_2 = mapping[md5_2]
                    seen.add(symbol_2)

                    if symbol_2 not in denylist:
                        symbols.append(symbol_2)

    # Functions in the mapping but not seen in the partial order.
    # If you want to add them, you can use the leftover flag.
    # Note: You can only use the leftover flag if the last-symbol flag was not passed
    if args.leftover and args.last_symbol != None:
        for md5 in mapping:
            if mapping[md5] not in seen:
                symbols.append(mapping[md5])

    # Write it to output file
    with open(args.output, "w") as f:
        for symbol in symbols:
            f.write(symbol+"\n")

            # If we are at the last-symbol, we do not write the rest of the symbols
            if symbol == args.last_symbol:
                break

if __name__ == '__main__':
    main()
