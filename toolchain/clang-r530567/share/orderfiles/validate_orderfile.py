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
# $ python3 validate_orderfile.py --order-file ../orderfiles/test/example.orderfile
#
# Try '-h' for a full list of command line arguments.
#
# Currently, we check four things in an orderfile:
#   - A partial order is maintained in the orderfile
#   - All symbols in allowlist must be present in the orderfile
#   - No symbol in denylist should be present in the orderfile
#   - The orderfile has a minimum number of symbols

import argparse
import orderfile_utils

def parse_args():
    """Parses and returns command line arguments."""
    parser = argparse.ArgumentParser(prog="validate_orderfile",
                                    description="Validates the orderfile is correct and useful based on flag conditions")

    parser.add_argument(
        "--order-file",
        required=True,
        help="Orderfile that needs to be validated")

    parser.add_argument(
        "--partial",
        default="",
        help=f"A partial order of symbols that need to hold in the orderfile."
             f"Format: A symbol-per-line file with @ or comma separarted values within a quotation."
             f"For example, you can say @file.txt or 'main,bar,foo'.")

    parser.add_argument(
        "--allowlist",
        default="",
        help=f"Symbols that have to be present in the orderfile."
             f"Format: A symbol-per-line file with @ or comma separarted values within a quotation."
             f"For example, you can say @file.txt or 'main,bar,foo'.")

    parser.add_argument(
        "--denylist",
        default="",
        help=f"Symbols that should not be in orderfile. Denylist flag has priority over allowlist."
             f"Format: A symbol-per-line file with @ or comma separarted values within a quotation."
             f"For example, you can say @file.txt or 'main,bar,foo'.")

    parser.add_argument(
        "--min",
        type=int,
        default=0,
        help="Minimum number of entires needed for an orderfile")

    return parser.parse_args()

def main():
    args = parse_args()

    allowlist = orderfile_utils.parse_set(args.allowlist)
    partial = orderfile_utils.parse_list(args.partial)
    denylist = orderfile_utils.parse_set(args.denylist)

    # Check if there are symbols common to both allowlist and denylist
    # We give priority to denylist so the symbols in the intersection
    # will be removed from allowlist
    inter = allowlist.intersection(denylist)
    allowlist = allowlist.difference(inter)

    num_entries = 0
    file_indices = {}
    file_present = set()

    # Read the orderfile
    with open(args.order_file, "r") as f:
        for line in f:
            line = line.strip()

            # Check if a symbol not allowed is within the orderfile
            if line in denylist:
                raise RuntimeError(f"Orderfile should not contain {line}")

            if line in allowlist:
                file_present.add(line)

            file_indices[line] = num_entries
            num_entries += 1

    # Check if there are not a minimum number of symbols in orderfile
    if num_entries < args.min:
        raise RuntimeError(f"The orderfile has {num_entries} symbols but it "
                           f"needs at least {args.min} symbols")

    # Check if all symbols allowed must be allowlist
    if len(allowlist) != len(file_present):
        raise RuntimeError("Some symbols in allow-list are not in the orderfile")

    # Check if partial order passed with flag is maintained within orderfile
    # The partial order might contain symbols not in the orderfile which we allow
    # because the order is still maintained.
    old_index = None
    curr_symbol = None
    for symbol in partial:
        new_index = file_indices.get(symbol)
        if new_index is not None:
            if old_index is not None:
                if new_index < old_index:
                    raise RuntimeError(f"`{curr_symbol}` must be before `{symbol}` in orderfile")
            old_index = new_index
            curr_symbol = symbol

    print("Order file is valid")

if __name__ == '__main__':
    main()
