#!/usr/bin/env python
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

from pathlib import Path
import glob
import os
import subprocess

def parse_set(param : str) -> set[str]:
    """Parse symbol set based on a file or comma-separate symbols."""
    symbol_set = set()
    if len(param) == 0:
        return symbol_set

    if param[0] == "@":
        with open(param[1:], "r") as f:
            for line in f:
                line = line.strip()
                symbol_set.add(line)
        return symbol_set

    list_symbols = param.split(",")
    symbol_set.update(list_symbols)
    return symbol_set

def parse_list(param : str) -> list[str]:
    """Parse partial order based on a file or comma-separate symbols."""
    symbol_order = []
    if len(param) == 0:
        return symbol_order

    if param[0] == "@":
        with open(param[1:], "r") as f:
            for line in f:
                line = line.strip()
                symbol_order.append(line)
        return symbol_order

    symbol_order = param.split(",")
    return symbol_order

def parse_merge_list(param : str) -> list[tuple[str,int]]:
    """Parse partial order based on a file, folder, or comma-separate symbols."""
    file_list = []
    if len(param) == 0:
        return file_list

    if param[0] == "@":
        file_dir = Path(param[1:]).resolve().parent
        with open(param[1:], "r") as f:
            for line in f:
                line = line.strip()
                line_list = line.split(",")
                # Name, Weight
                file_list.append((file_dir / line_list[0], int(line_list[1])))
        return file_list

    if param[0] == "^":
        file_lst = glob.glob(param[1:]+"/*.orderfile")
        # Assumig weight of 1 for all the files. Sorting of files provides
        # a deterministic order of orderfile.
        file_list = sorted([(orderfile, 1) for orderfile in file_lst])
        return file_list

    file_lst = param.split(",")
    file_list = [(orderfile, 1) for orderfile in file_lst]
    return file_list

def check_call(cmd, *args, **kwargs):
    """subprocess.check_call."""
    subprocess.check_call(cmd, *args, **kwargs)


def check_output(cmd, *args, **kwargs):
    """subprocess.check_output."""
    return subprocess.run(
        cmd, *args, **kwargs, check=True, text=True,
        stdout=subprocess.PIPE).stdout

def check_error(cmd, *args, **kwargs):
    """subprocess.check_error."""
    return subprocess.run(
        cmd, *args, **kwargs, check=True, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout

def android_build_top():
    """Get top directory to find files."""
    THIS_DIR = os.path.realpath(os.path.dirname(__file__))
    return os.path.realpath(os.path.join(THIS_DIR, '../../../..'))