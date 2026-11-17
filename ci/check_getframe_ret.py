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
runs. Those routines use "getframe_jmp" (x86_64) or "br x30" (aarch64) instead of "getframe" followed by "ret", and
leave the entry of the call that entered them on the return stack for the return from the new frame (see
SFF_RSB_ENTRY in sr_port/stack_frame.h). Add a routine to ROUTINES when it is changed that way.

A "ret" is reported when it comes after a "getframe" and before the next label or branch instruction, unless its line
has the comment CONSUMES_OWN_ENTRY, which marks a "ret" that consumes the entry of the routine's own call on purpose.

The script also checks that SFF_RSB_ENTRY is set where it is needed, in every assembler file of each directory in
ARCHS. A frame without the flag whose call left an entry, or with the flag whose call left none, does not change where
control goes, as both ways of returning go to the mpc, but makes every later "ret" mispredicted until the return stack
is balanced again. So these are reported:
1. A routine that calls "new_stack_frame" or "new_stack_frame_sp" and then jumps to the M code ("getframe_jmp" or an
   indirect "jmp" on x86_64, "br" on aarch64), but does not set SFF_RSB_ENTRY ("mark_rsb_entry" or an "or" of
   SFF_RSB_ENTRY).
2. A routine that sets SFF_RSB_ENTRY but does not jump to the M code, or that is not in ROUTINES (so the check of
   "ret" after "getframe" does not cover it).
3. A routine that tests SFF_RSB_ENTRY ("getframe_ret_or_jmp" or "getframe_ret_or_br") but is not in ROUTINES.
Frames that C code creates are not checked.

Run from the top of a YottaDB working tree. A pass prints one line. A failure prints every routine reported and exits
with status 1.
"""

import os
import sys

ROUTINES = (
    "op_extcall",
    "op_extexfun",
    "op_mprofextcall",
    "op_mprofextexfun",
    "op_retarg",
    "mum_tstart",
    "opp_commarg",
    "opp_ret",
)

ARCHS = (
    # (source directory, how a comment line starts, branch instructions that end the search, what to use instead,
    #  call instruction)
    ("sr_x86_64", ("#",), ("jmp", "call", "getframe_jmp"), '"getframe_jmp"', "call"),
    ("sr_aarch64", ("/*", "*", "//"), ("b", "br", "bl", "blr"), '"br x30"', "bl"),
)


def first_word(line):
    words = line.split()
    return words[0] if words else ""


def code_lines(path, comment_starts):
    """Return (line number, words) for each line of "path" that has code, without comments"""
    result = []
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.read().split("\n")
    in_block = False
    for number, line in enumerate(lines, 1):
        if in_block:
            if "*/" not in line:
                continue
            line = line.split("*/", 1)[1]
            in_block = False
        while "/*" in line:
            before, after = line.split("/*", 1)
            if "*/" in after:
                line = before + " " + after.split("*/", 1)[1]
            else:
                line = before
                in_block = True
        if "#" in comment_starts:
            line = line.split("#", 1)[0]
        line = line.split("//", 1)[0]
        words = line.replace(",", " ").split()
        if words and not words[0].startswith(comment_starts):
            result.append((number, words))
    return result


def check_flag(directory, comment_starts, call):
    """Report the routines in "directory" that do not set or test SFF_RSB_ENTRY as described at the top"""
    reported = []
    for name in sorted(os.listdir(directory)):
        if not name.endswith(".s"):
            continue
        path = os.path.join(directory, name)
        routine = name[:-2]
        creates = sets = tests = jumps = False
        for _, words in code_lines(path, comment_starts):
            op = words[0]
            if (call == op) and (1 < len(words)) and (words[1] in ("new_stack_frame", "new_stack_frame_sp")):
                creates = True
            elif "mark_rsb_entry" == op:
                sets = True
            elif op.startswith("or") and any("SFF_RSB_ENTRY" in w for w in words[1:]):
                sets = True
            elif op in ("getframe_ret_or_jmp", "getframe_ret_or_br"):
                tests = True
            elif ("getframe_jmp" == op) or ("br" == op) or (("jmp" == op) and (1 < len(words))
                                                             and words[1].startswith("*")):
                jumps = True
        if creates and jumps and not sets:
            reported.append("%s: creates a frame and jumps to it but does not set SFF_RSB_ENTRY; use mark_rsb_entry"
                            % path)
        if sets and not jumps:
            reported.append("%s: sets SFF_RSB_ENTRY but does not jump to the M code" % path)
        if (sets or tests) and (routine not in ROUTINES):
            reported.append("%s: sets or tests SFF_RSB_ENTRY but is not in ROUTINES in this script" % path)
    return reported


def main():
    reported = []
    nfiles = 0
    for directory, comment_starts, branches, use, call in ARCHS:
        reported.extend(check_flag(directory, comment_starts, call))
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
                    if ("ret" == word) and ("CONSUMES_OWN_ENTRY" not in lines[j]):
                        reported.append("%s:%d: ret at line %d after getframe; use %s" % (path, i + 1, j + 1, use))
                        break
    if reported:
        print("check_getframe_ret.py: %d problem(s) that unbalance the return stack:" % len(reported))
        for r in reported:
            print("  " + r)
        return 1
    print("check_getframe_ret.py: no ret after getframe in %d assembler files, and SFF_RSB_ENTRY is set where needed"
          % nfiles)
    return 0


if __name__ == "__main__":
    sys.exit(main())
