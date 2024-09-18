#!/usr/bin/env python3
#
# Copyright (c) 2024 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Linter checks that we don't add unnecessary locale dependence

import os
import re
import sys
import glob

# Known violations for certain files and the functions allowed in those files
KNOWN_VIOLATIONS = {
    "src/bitcoin-tx.cpp": ["stoul"],
    "src/qt/rpcconsole.cpp": ["atoi"],
    "src/rest.cpp": ["strtol"],
    "src/test/dbwrapper_tests.cpp": ["snprintf"],
    "src/torcontrol.cpp": ["atoi", "strtol"],
    "src/util/system.cpp": ["atoi"],
    "src/util/strencodings.cpp": ["atoi", "strtol", "strtoll", "strtoul", "strtoull"],
    "src/pubkey.cpp": ["normalize"],
    "src/logging.cpp": ["trim"],
    "src/dbwrapper.cpp": ["vsprintf", "stoul", "vsnprintf"],
    "src/bench/libauth_bench.cpp": ["trim"],
    "src/seeder/db.cpp": ["ctime"],
    "src/seeder/db.h": ["ctime"],
    "src/seeder/bitcoin.cpp": ["ctime"],
    "src/seeder/main.cpp": ["ctime", "strcasecmp", "strtoull", "strftime"],
    "src/seeder/dns.cpp": ["ctime", "strcasecmp"],
    "src/seeder/test/p2p_messaging_tests.cpp": ["ctime"],
    "src/test/token_tests.cpp": ["atoll", "setlocale"],
    "src/test/main.cpp": ["setlocale"],
    "src/test/util_tests.cpp": ["trim", "ctime"],
    "src/util/moneystr.cpp": ["trim", "sprintf"],
    "src/util/time.cpp": ["ctime"],
    "src/qt/bitcoinunits.cpp": ["sprintf"],
    "src/script/script.cpp": ["trim"],
    "src/node/blockstorage.cpp": ["atoi"],
    "src/tinyformat.h": ["atoi", "vprintf"],
    "src/util/strencodings.h": ["isspace", "atoi"],
}

# Locale-dependent functions to check
LOCALE_DEPENDENT_FUNCTIONS = [
    "alphasort",   # LC_COLLATE (via strcoll)
    "asctime",     # LC_TIME (directly)
    "asprintf",    # (via vasprintf)
    "atof",        # LC_NUMERIC (via strtod)
    "atoi",        # LC_NUMERIC (via strtol)
    "atol",        # LC_NUMERIC (via strtol)
    "atoll",       # (via strtoll)
    "atoq",
    "btowc",       # LC_CTYPE (directly)
    "ctime",       # (via asctime or localtime)
    "dprintf",     # (via vdprintf)
    "fgetwc",
    "fgetws",
    "fold_case",   # boost::locale::fold_case
    #"fprintf"      // (via vfprintf)
    "fputwc",
    "fputws",
    "fscanf",      # (via __vfscanf)
    "fwprintf",    # (via __vfwprintf)
    "getdate",     # via __getdate_r => isspace // __localtime_r
    "getwc",
    "getwchar",
    "is_digit",    # boost::algorithm::is_digit
    "is_space",    # boost::algorithm::is_space
    "isalnum",     # LC_CTYPE
    "isalpha",     # LC_CTYPE
    "isblank",     # LC_CTYPE
    "iscntrl",     # LC_CTYPE
    "isctype",     # LC_CTYPE
    "isdigit",     # LC_CTYPE
    "isgraph",     # LC_CTYPE
    "islower",     # LC_CTYPE
    "isprint",     # LC_CTYPE
    "ispunct",     # LC_CTYPE
    "isspace",     # LC_CTYPE
    "isupper",     # LC_CTYPE
    "iswalnum",    # LC_CTYPE
    "iswalpha",    # LC_CTYPE
    "iswblank",    # LC_CTYPE
    "iswcntrl",    # LC_CTYPE
    "iswctype",    # LC_CTYPE
    "iswdigit",    # LC_CTYPE
    "iswgraph",    # LC_CTYPE
    "iswlower",    # LC_CTYPE
    "iswprint",    # LC_CTYPE
    "iswpunct",    # LC_CTYPE
    "iswspace",    # LC_CTYPE
    "iswupper",    # LC_CTYPE
    "iswxdigit",   # LC_CTYPE
    "isxdigit",    # LC_CTYPE
    "localeconv",  # LC_NUMERIC + LC_MONETARY
    "mblen",       # LC_CTYPE
    "mbrlen",
    "mbrtowc",
    "mbsinit",
    "mbsnrtowcs",
    "mbsrtowcs",
    "mbstowcs",    # LC_CTYPE
    "mbtowc",      # LC_CTYPE
    "mktime",
    "normalize",   # boost::locale::normalize
    #"printf"       // LC_NUMERIC
    "putwc",
    "putwchar",
    "scanf",       # LC_NUMERIC
    "setlocale",
    "snprintf",
    "sprintf",
    "sscanf",
    "stod",
    "stof",
    "stoi",
    "stol",
    "stold",
    "stoll",
    "stoul",
    "stoull",
    "strcasecmp",
    "strcasestr",
    "strcoll",     # LC_COLLATE
    #"strerror"
    "strfmon",
    "strftime",    # LC_TIME
    "strncasecmp",
    "strptime",
    "strtod",      # LC_NUMERIC
    "strtof",
    "strtoimax",
    "strtol",      # LC_NUMERIC
    "strtold",
    "strtoll",
    "strtoq",
    "strtoul",     # LC_NUMERIC
    "strtoull",
    "strtoumax",
    "strtouq",
    "strxfrm",     # LC_COLLATE
    "swprintf",
    "to_lower",    # boost::locale::to_lower
    "to_title",    # boost::locale::to_title
    "to_upper",    # boost::locale::to_upper
    "tolower",     # LC_CTYPE
    "toupper",     # LC_CTYPE
    "towctrans",
    "towlower",    # LC_CTYPE
    "towupper",    # LC_CTYPE
    "trim",        # boost::algorithm::trim
    "trim_left",   # boost::algorithm::trim_left
    "trim_right",  # boost::algorithm::trim_right
    "ungetwc",
    "vasprintf",
    "vdprintf",
    "versionsort",
    "vfprintf",
    "vfscanf",
    "vfwprintf",
    "vprintf",
    "vscanf",
    "vsnprintf",
    "vsprintf",
    "vsscanf",
    "vswprintf",
    "vwprintf",
    "wcrtomb",
    "wcscasecmp",
    "wcscoll",     # LC_COLLATE
    "wcsftime",    # LC_TIME
    "wcsncasecmp",
    "wcsnrtombs",
    "wcsrtombs",
    "wcstod",      # LC_NUMERIC
    "wcstof",
    "wcstoimax",
    "wcstol",      # LC_NUMERIC
    "wcstold",
    "wcstoll",
    "wcstombs",    # LC_CTYPE
    "wcstoul",     # LC_NUMERIC
    "wcstoull",
    "wcstoumax",
    "wcswidth",
    "wcsxfrm",     # LC_COLLATE
    "wctob",
    "wctomb",      # LC_CTYPE
    "wctrans",
    "wctype",
    "wcwidth",
    "wprintf",
]

# Advice message
ADVICE_MESSAGE = """
Unnecessary locale dependence can cause bugs and should be avoided.
Otherwise, an exception can be added to the LocaleDependenceLinter.
"""

EXCLUDE_FOLDERS = [
    'src/crypto/ctaes',
    'src/leveldb',
    'src/secp256k1',
    'src/univalue',
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

def lint_file(root, file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        file_content = f.readlines()

    # Get the base filename and check if it's a known violation
    relative_path = os.path.relpath(file_path, root)
    exceptions = KNOWN_VIOLATIONS.get(relative_path, [])

    # Create a pattern for locale-dependent functions
    locale_function_pattern = re.compile(r'\b(' + '|'.join(LOCALE_DEPENDENT_FUNCTIONS) + r')\b')

    found_issue = False
    # Check each line for locale-dependent functions
    for line_number, line_content in enumerate(file_content):
        if locale_function_pattern.search(line_content):
            function_name = locale_function_pattern.search(line_content).group(0)

            # Check if the found function is an exception
            if function_name not in exceptions:
                print(f"In {file_path}, line {line_number + 1}: Locale dependent function '{function_name}' found.")
                found_issue = True

    return not found_issue

def main():
    if len(sys.argv) != 2:
        root = os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', '..')
        print(f'Using root dir {root}')
    else:
        root = sys.argv[1]

    # Find all source files to lint
    sources = find_source_files(root)

    ok = True
    for source in sources:
        file_ok = lint_file(root, source)
        if not file_ok:
            ok = False

    if not ok:
        print(ADVICE_MESSAGE)

    if not ok:
        sys.exit(1)

if __name__ == '__main__':
    main()

