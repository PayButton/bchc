#!/usr/bin/env python3
#
# Copyright (c) 2024 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Linter checks that we don't unnecessarily use C-style imports.

import os
import re
import sys
import glob

CHEADERS = [
    'assert', 'ctype', 'errno', 'fenv', 'float', 'inttypes', 'limits',
    'locale', 'math', 'setjmp', 'signal', 'stdarg', 'stddef', 'stdint',
    'stdio', 'stdlib', 'string', 'time', 'uchar', 'wchar', 'wctype'
]

EXCLUDE_FOLDERS = [
    'src/crypto/ctaes',
    'src/leveldb',
    'src/secp256k1',
    'src/univalue',
    'src/util/syserror.cpp',
]

SOURCE_PATTERNS = [
    "src/**/*.h",
    "src/**/*.cpp",
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
    # Read file content
    with open(file_path, 'r', encoding='utf-8') as f:
        file_content = f.read()

    # Create regex pattern for matching C headers
    any_header_pattern = '|'.join(CHEADERS)
    pattern = re.compile(r'#include <(' + any_header_pattern + r')\.h>')

    # Find all C headers
    matches = pattern.finditer(file_content)

    # Perform linting and suggest replacements
    ok = True
    for match in matches:
        header = match.group(1)
        original = header + '.h'
        replacement = 'c' + header
        print(f"In {file_path}: Use C++ header <{replacement}> instead of C compatibility header <{original}>")
        ok = False

    return ok

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

