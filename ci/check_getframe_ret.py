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

"""Report a "ret" after "getframe" in the assembler routines that transfer control to M code other than their caller.

"getframe" loads the registers of the M frame on top of the stack. On x86_64 it then pushes the address of the M
code (the frame's mpc) so that a "ret" goes there, and on aarch64 it puts that address in x30. A "ret" is predicted
from the return stack, which holds the address after the "call" (x86_64) or "bl" (aarch64) that entered the routine.

Most routines that end with "getframe" followed by "ret" return to the M code that called them, so the mpc is that
same address and the "ret" is predicted correctly. The routines in ROUTINES below instead go to M code other than
their caller (a new frame, another routine or the caller of the frame), so a "ret" there is mispredicted every time it
runs. Those routines use "getframe_jmp" (x86_64) or "br x30" (aarch64) instead of "getframe" followed by "ret". Add a
routine to ROUTINES when it is changed that way.

A "ret" is reported when it comes after a "getframe" and before the next label or branch instruction. Run from the
top of a YottaDB working tree. A pass prints one line. A failure prints every routine reported and exits with
status 1.
"""

import os
import sys

ROUTINES = (
    "op_extcall",
    "op_extexfun",
    "op_mprofextcall",
    "op_mprofextexfun",
    "op_retarg",
    "opp_commarg",
    "opp_ret",
)

ARCHS = (
    # (source directory, how a comment line starts, branch instructions that end the search, what to use instead)
    ("sr_x86_64", ("#",), ("jmp", "call", "getframe_jmp"), '"getframe_jmp"'),
    ("sr_aarch64", ("/*", "*", "//"), ("b", "br", "bl", "blr"), '"br x30"'),
)


def first_word(line):
    words = line.split()
    return words[0] if words else ""


def main():
    reported = []
    nfiles = 0
    for directory, comment_starts, branches, use in ARCHS:
        for routine in ROUTINES:
            path = os.path.join(directory, routine + ".s")
            if not os.path.isfile(path):
                reported.append("%s: not found; update ROUTINES in this script" % path)
                continue
            nfiles += 1
            with open(path, encoding="utf-8", errors="replace") as f:
                lines = f.read().split("\n")
            for i, line in enumerate(lines):
                if "getframe" != first_word(line):
                    continue
                for j in range(i + 1, len(lines)):
                    stripped = lines[j].strip()
                    if ("" == stripped) or stripped.startswith(comment_starts):
                        continue
                    word = first_word(lines[j])
                    if word.endswith(":") or ("ENTRY" == word) or (word in branches) or word.startswith("b."):
                        break
                    if "ret" == word:
                        reported.append("%s:%d: ret at line %d after getframe; use %s" % (path, i + 1, j + 1, use))
                        break
    if reported:
        print("check_getframe_ret.py: %d problem(s) in routines that must not return with a mispredicted ret:"
              % len(reported))
        for r in reported:
            print("  " + r)
        return 1
    print("check_getframe_ret.py: no ret after getframe in %d assembler files" % nfiles)
    return 0


if __name__ == "__main__":
    sys.exit(main())
