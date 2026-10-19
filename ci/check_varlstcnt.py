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

"""Report send_msg/gtm_putmsg/rts_error call sites that do not pass the arguments
their consumption loop reads.

Both consumption loops, send_msg_va() in sr_unix/send_msg.c and gtm_putmsg_list()
in sr_unix/gtm_putmsg_list.c, trust the declared count. A count that is too high
makes the loop ask for an argument the call never pushed, so it reads past the end
of the va_list. See YDB#1310 for what that can do.

Other miscounts make the loop take part of a multi-argument macro such as
LEN_AND_STR() or DB_LEN_STR(), or the msg_id of the next message, where it wants a
FAO count, or a literal number where it wants a msg_id. These are reported too.

Calls to op_*() and cli_check_any2() also start with VARLSTCNT but are not message
calls. Their VARLSTCNT is a plain count of the arguments that follow, so for these
the declared count must equal the number of arguments passed. A FAO count held in a variable takes its value from
the nearest assert(var == N) before the call in the same function, provided nothing
assigns var between the two. Every site that is neither simulated nor counted is
listed with the reason and fails the check, so that nothing is skipped silently.

Run from the top of a YottaDB working tree. A pass prints two lines. A failure prints
every site reported or skipped, with what is needed to debug it, and exits with
status 1.

Two things are deliberately NOT checked, because both are legal and would produce
only false positives:

  * a plain count of the arguments. A trailing sentinel 0 that the loop never
    reaches is an accepted idiom, so

        RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT(x),
                          CALLFROM, status, 0)

    declares 8 for 9 arguments and is correct. That is why this simulates the loop
    rather than counting.

  * the FAO number at the call against the fao= of that message in
    sr_port/merrors.msg. Passing fewer parameters than a message declares is legal
    and only leaves a field of the message empty.
"""

import glob
import os
import re
import sys


def mask_strings(text):
    """Blank the contents of string and character literals, preserving length, so
    that parentheses and commas inside them cannot confuse the scanner."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c in '"\'':
            out.append(c)
            i += 1
            while i < n:
                if text[i] == '\\':
                    out.append('XX')        # two characters, to keep offsets exact
                    i += 2
                    continue
                if text[i] == c:
                    break
                out.append('X')
                i += 1
            if i < n:
                out.append(c)
                i += 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def split_top(text):
    """Split on commas that are not inside parentheses or brackets."""
    out = []
    depth = 0
    cur = ''
    for c in text:
        if c in '([':
            depth += 1
        elif c in ')]':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += c
    if cur.strip() or out:
        out.append(cur)
    return out


def macro_bodies(roots):
    """The body of each single-line #define in the headers, with comments removed
    and string literals masked."""
    bodies = {}
    for root in roots:
        for header in glob.glob(os.path.join(root, '*.h')):
            text = open(header, errors='replace').read().replace('\\\n', ' ')
            text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
            text = mask_strings(text)
            for m in re.finditer(r'^#[ \t]*define[ \t]+([A-Z_][A-Z0-9_]*)(\([^)]*\))?[ \t]+(.+)$',
                                 text, re.M):
                body = m.group(3).strip()
                if '{' in body or ';' in body:
                    continue
                bodies.setdefault(m.group(1), body)
    return bodies


def expand(arg, bodies, depth=0):
    """The arguments that arg turns into, as a list of strings: REG_LEN_STR() and
    DB_LEN_STR() give 2 each, CALLFROM gives 3, and so on. A numeric literal in a
    macro body is kept, so PUT_SYS_ERRNO(x) gives ['0', ...]. Any other argument of
    a multi-argument expansion is only a placeholder."""
    arg = arg.strip()
    lead = re.match(r'^([A-Z_][A-Z0-9_]*)\s*(\(|$)', arg)
    if not lead or lead.group(1) not in bodies or 12 < depth:
        return [arg]
    out = []
    for part in split_top(bodies[lead.group(1)]):
        part = part.strip()
        out.extend([part] if re.match(r'^\d+$', part) else expand(part, bodies, depth + 1))
    if 1 == len(out) and not re.match(r'^\d+$', out[0]):
        return [arg]
    return out


def asserted_value(scan, before, var):
    """The N of the nearest assert(var == N) or assert(N == var) in scan before offset
    before, if no function ends and nothing assigns var between it and before."""
    name = re.escape(var)
    found = None
    for m in re.finditer(r'\bassert\s*\(\s*(?:%s\s*==\s*(\d+)|(\d+)\s*==\s*%s)\s*\)'
                         % (name, name), scan[:before]):
        found = m
    if not found:
        return None
    between = scan[found.end():before]
    if re.search(r'^}', between, re.M) or re.search(r'\b%s\s*=[^=]' % name, between):
        return None
    return found.group(1) or found.group(2)


def main():
    roots = ['sr_port', 'sr_unix']
    missing = [r for r in roots if not os.path.isdir(r)]
    if missing:
        sys.stderr.write("run this from the top of a YottaDB working tree; no %s\n"
                         % ", ".join(missing))
        return 2

    bodies = macro_bodies(roots)

    reported = []
    skipped = []
    simulated = 0
    counted = 0
    from_asserts = []
    for source in sorted(sum([glob.glob(os.path.join(r, '*.c')) for r in roots], [])):
        src = open(source, errors='replace').read()
        scan = mask_strings(re.sub(r'/\*.*?\*/', lambda m: ' ' * len(m.group(0)), src, flags=re.S))
        for m in re.finditer(r'VARLSTCNT(1?)\((\d+)\)', scan):
            line = src[:m.start()].count('\n') + 1
            callee = re.search(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*$', scan[:m.start()])
            plain = callee and re.match(r'^(op_\w+|cli_check_any2)$', callee.group(1))
            declared = int(m.group(2)) + (1 if m.group(1) == '1' else 0)
            i = m.end()
            depth = 1
            end = None
            while i < len(scan):
                if scan[i] == '(':
                    depth += 1
                elif scan[i] == ')':
                    depth -= 1
                    if depth == 0:
                        end = i
                        break
                i += 1
            if end is None:
                skipped.append((source, line, 'no closing parenthesis'))
                continue
            body = scan[m.end():end].replace('\\', ' ')    # the call may sit in a #define
            args = split_top(body)
            if not args:
                skipped.append((source, line, 'no arguments'))
                continue
            # Each element is (text, the macro it is a non-literal part of, or None).
            flat = []
            for arg in args:
                parts = expand(arg, bodies)
                if 1 == len(parts):
                    flat.append((arg.strip(), None))
                else:
                    macro = re.match(r'^\s*(\w+)', arg).group(1)
                    flat.extend((part, None if re.match(r'^\d+$', part) else macro)
                                for part in parts)

            if plain:
                counted += 1
                if len(flat) != declared:
                    reported.append((source, line, declared, len(flat),
                                     '%s() reads %s' % (callee.group(1), 'only the first %d'
                                     % declared if declared < len(flat) else 'past the end'),
                                     re.sub(r'\s+', ' ', src[m.end():end].strip())))
                continue

            # Simulate the loop: read a msg_id, then while the count is positive read
            # a FAO count and skip that many parameters, until the count runs out.
            pos = 0
            remaining = declared
            verdict = None
            while True:
                if pos >= len(flat):
                    verdict = 'reads a msg_id past the end'
                    break
                if re.match(r'^\d+$', flat[pos][0]):
                    verdict = 'reads the literal %s as a msg_id' % flat[pos][0]
                    break
                pos += 1
                remaining -= 1
                if remaining > 0:
                    if pos >= len(flat):
                        verdict = 'reads a FAO count past the end'
                        break
                    token, in_macro = flat[pos]
                    pos += 1
                    remaining -= 1
                    if in_macro:
                        verdict = 'reads part of %s() as a FAO count' % in_macro
                        break
                    if re.match(r'^(MAKE_MSG_\w+\(\s*)?ERR_', token):
                        verdict = 'reads the msg_id %s as a FAO count' % token
                        break
                    if re.match(r'^[A-Za-z_]\w*$', token):
                        value = asserted_value(scan, m.start(), token)
                        if value is not None:
                            from_asserts.append((source, line, token, value))
                            token = value
                    if not re.match(r'^\d+$', token):
                        skipped.append((source, line, 'FAO count %s is not a literal'
                                        % re.sub(r'\s+', ' ', token)))
                        verdict = 'skip'
                        break
                    fao = int(token)
                    if pos + fao > len(flat):
                        verdict = 'reads a FAO parameter past the end'
                        break
                    pos += fao
                    remaining -= fao
                if remaining <= 0:
                    break
            if verdict == 'skip':
                continue
            simulated += 1
            if verdict:
                reported.append((source, line, declared, len(flat), 'the loop ' + verdict,
                                 re.sub(r'\s+', ' ', src[m.end():end].strip())))

    print("# Checking %d VARLSTCNT message call sites." % (simulated + len(skipped)))
    if not reported and not skipped:
        print("PASS : Every site passes what its VARLSTCNT and FAO counts declare")
        return 0
    print("FAIL : %d site(s) do not pass what their VARLSTCNT and FAO counts declare, "
          "and %d site(s) could not be checked" % (len(reported), len(skipped)))
    print("\nSimulated %d message call sites and counted the arguments of %d other "
          "VARLSTCNT calls." % (simulated, counted))
    for source, line, var, value in from_asserts:
        print("    took FAO count %s at %s:%d as %s from its assert" % (var, source, line, value))
    for source, line, reason in skipped:
        print("    skipped %s:%d because %s" % (source, line, reason))
    for source, line, declared, total, verdict, text in reported:
        print("\n%s:%d" % (source, line))
        print("    VARLSTCNT(%d) with %d arguments passed, and %s"
              % (declared, total, verdict))
        print("    %s" % text[:150])
    if reported:
        print("\nMake each declared count, and each FAO count, match the arguments the call "
              "passes, counting macro expansions as they expand.")
    if skipped:
        print("\nFor a skipped site, make its FAO count a literal, or assert the value of the "
              "variable that holds it before the call.")
    return 1

if __name__ == '__main__':
    sys.exit(main())
