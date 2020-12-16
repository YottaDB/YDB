#!/usr/bin/env python3
#
#################################################################
#                                                               #
# Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.       #
# All rights reserved.                                          #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

import re
import sys

YOTTADB = re.compile('Copyright \(c\) (?P<start_date>20[0-9][0-9])(?P<end_date>-20[0-9][0-9])? YottaDB')

# Goes through the file one line at a time until it finds a copyright that needs to be updated.
# Returns whether the copyright was updated.
def look_for_copyrights():
    for line in sys.stdin:
        # Simple case: existing YottaDB copyright
        # If an end date exists, replace it with this year;
        # Otherwise, add this year as the end date.
        matches = YOTTADB.search(line)
        if matches is not None:
            # Already up to date, no need to run.
            if matches.group("start_date") == "2020" or matches.group("end_date") == "-2020":
                print(line, end="")
                return False
            print(YOTTADB.sub("Copyright (c) \g<start_date>-2020 YottaDB", line), end="")
            return True
        # More difficult case: no YottaDB copyrights in the file, so we have to add them.
        # This assumes that 'This source code ...' comes after all copyrights.
        elif 'This source code contains the intellectual' in line:
            start, end = (" *", "*") if line.startswith(" *") else ("#", "#")
            print(start, " Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	", end, sep='')
            print(start, "								", end, sep='')
            print(line, end="")
            return True
        else:
            # Not a copyright; pass the line through unchanged.
            print(line, end="")
    # If we get here, there was no copyright in the file.
    # Exit with an error
    exit(2)

def main():
    replaced = look_for_copyrights()
    for line in sys.stdin:
        print(line, end="")
    if replaced:
        exit(1)

if __name__ == '__main__':
    main()
