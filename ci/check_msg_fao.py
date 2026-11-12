#!/usr/bin/env python3
#
#################################################################
#                                                               #
# Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.       #
# All rights reserved.                                          #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

"""Report message definitions in sr_port/*.msg whose fao= count differs from the
number of parameters that the directives in the text of the message take.

util_format() (sr_unix/util_output.c) formats the text of a message and takes one
parameter for each !AC, !AZ, !AS, numeric (!SL, !UL, !XL, !ZL and the B, W and J
forms, with or without @) directive and # field width, and two for each !AD and !AF
directive. !/, !_, !^, !! and !n*c take none. mval2fao() (sr_port/mval2fao.c), which
converts the parameters of a ZMESSAGE command, trusts the fao= count, and skips the
text to the next directive once for each parameter, so a fao= count larger than the
directives of the text makes it run past the end of the text (YDB#1101).

ci/check_varlstcnt.py checks the call sites that issue messages, and not the fao=
counts of the definitions, so this is a separate check. The UNUSEDMSG entries are
placeholders that keep the message numbers of messages that are no longer used; no
code issues them, so their counts are not checked.

Run from the top of a YottaDB working tree. A pass prints one line. A failure prints
every mismatch and exits with status 1.
"""

import glob
import re
import sys


def consumed(text):
    """Return the number of parameters that the directives in "text" take, or None
    for a directive this does not know."""
    count = 0
    i = 0
    n = len(text)
    while True:
        i = text.find("!", i)
        if i < 0:
            return count
        i += 1
        if (i < n) and ("-" == text[i]):       # right justify
            i += 1
        if (i < n) and ("#" == text[i]):       # field width from a parameter
            count += 1
            i += 1
        else:
            while (i < n) and text[i].isdigit():    # field width or repeat count
                i += 1
        if (i < n) and ("@" == text[i]):       # indirect operand
            i += 1
        if i >= n:
            return None
        directive = text[i]
        i += 1
        if directive in "/_^!":
            continue
        if "*" == directive:                    # repeat the next character
            i += 1
            continue
        qualifier = text[i] if (i < n) else ""
        i += 1
        if "A" == directive:
            if qualifier in "DF":
                count += 2
            elif qualifier in "CSZ":
                count += 1
            else:
                return None
        elif (directive in "SUXZ") and (qualifier in "BWLJQ"):
            count += 1
        else:
            return None


def main():
    entry = re.compile(r"^([A-Z0-9_]+)\s+<(.*)>\s*/(\w+)\s*/fao=(\d+)")
    problems = []
    checked = 0
    for path in sorted(glob.glob("sr_port/*.msg")):
        with open(path, encoding="latin-1") as msgfile:
            for lineno, line in enumerate(msgfile, 1):
                if not re.match(r"^[A-Z0-9_]+\s+<", line):
                    continue
                match = entry.match(line)
                if not match:
                    problems.append("%s:%d: cannot parse the message definition" % (path, lineno))
                    continue
                name, text, fao = match.group(1), match.group(2), int(match.group(4))
                checked += 1
                if name.startswith("UNUSEDMSG"):
                    continue
                count = consumed(text)
                if count is None:
                    problems.append("%s:%d: %s: unknown directive in the text" % (path, lineno, name))
                elif count != fao:
                    problems.append("%s:%d: %s: fao=%d, but the directives of the text take %d parameters"
                                    % (path, lineno, name, fao, count))
    if problems:
        print("\n".join(problems))
        print("FAILED: %d message definitions with a fao= count that does not match the text" % len(problems))
        return 1
    print("PASSED: the fao= counts of all %d message definitions match their text" % checked)
    return 0


if "__main__" == __name__:
    sys.exit(main())
