#!/usr/bin/env python3
#
# Copyright (c) 2024 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Linter checks that source file names follow naming convention


import os
import re
import sys
import glob

EXCLUDE_FOLDERS = [
    'src/crypto/ctaes',
    'src/leveldb',
    'src/secp256k1',
    'src/tinyformat.h',
    'src/univalue'
]

SOURCE_PATTERNS = [
    "src/**/*.cpp",
    "src/**/*.h",
]

def find_source_files(root):
    # Find source files based on patterns and exclude folders
    sources = []
    for pattern in SOURCE_PATTERNS:
        for file_path in glob.glob(os.path.join(root, pattern), recursive=True):
            if not any(excluded in file_path for excluded in EXCLUDE_FOLDERS):
                sources.append(file_path)
    return sources

def lint_file(file_path):
    # Extract the filename without the path
    file_name = os.path.basename(file_path)

    # Check if the filename contains only valid characters
    if re.search(r'[^a-z0-9_-]', os.path.splitext(file_name)[0]):
        print(f"Invalid file name: {file_name} in {file_path}. File names should only contain [a-z0-9_-] characters.")

    # Return True if the file name is valid, otherwise return False
    return re.search(r'[^a-z0-9_-]', os.path.splitext(file_name)[0]) is None

def main():
    if len(sys.argv) != 2:
        root = os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', '..')
        print(f'Using root dir {root}')
    else:
        root = sys.argv[1]

    # Find all source files to lint
    sources = find_source_files(root)

    ok = True
    # Lint each file
    for source in sources:
        ok = ok and lint_file(source)

    if not ok:
        sys.exit(1)

if __name__ == '__main__':
    main()

