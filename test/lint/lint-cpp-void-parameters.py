#!/usr/bin/env python3
#
# Copyright (c) 2024 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Linter tests that C++ style parameters are used instead of C-style void parameters

import os
import re
import sys
import glob

EXCLUDE_FOLDERS = [
    'src/crypto/ctaes',
    'src/leveldb',
    'src/secp256k1',
    'src/univalue',
    'src/compat/glibc_compat.cpp'
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
    # Read file content
    with open(file_path, 'r', encoding='utf-8') as f:
        file_content = f.read()

    # Create regex pattern for matching C-style void parameters
    pattern = re.compile(r'\S+\s?\(void\)')

    # Find all occurrences of C-style void parameters
    matches = list(pattern.finditer(file_content))

    # Perform linting and suggest replacements
    for match in matches:
        function_declaration = match.group(0)
        corrected_declaration = function_declaration.replace('(void)', '()')
        print(f"In {file_path}: C++ style parameters should be used: {corrected_declaration} instead of {function_declaration}")

    # Return True if no matches were found (success), otherwise return False (failure)
    return len(matches) == 0

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

