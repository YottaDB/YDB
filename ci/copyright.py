#!/usr/bin/env python3
#
#################################################################
#                                                               #
# Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.       #
# All rights reserved.                                          #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

# NOTE: This script is duplicated in YDB, YDBTest, YDBOcto & YDBPython. Maintaining a single copy is not possible
# as it will lead to outdated script usage by pre-commit hook. Any changes in one should also be made to the other.
# File name is expected as argument to the script. For ex: ./copyright.py file.csh

import re
import sys
import os.path
from datetime import datetime

YOTTADB = re.compile("Copyright \(c\) (?P<start_date>20[0-9][0-9])(?P<end_date>-20[0-9][0-9])? YottaDB")

# Goes through the file given by f one line at a time until it finds a copyright that needs to be updated.
# f   -> file under consideration
# ext -> extension of the file f
# Returns whether the copyright was updated.
def look_for_copyrights(f, ext):
    # my_cc_map is a dictionary having file extension and its comment character mapping
    # To support other file extensions include a key value pair for each file extension below
    # Note:
    # 1.
    # Following commands are helpful to identify file extensions which can be added to this map.
    # To list extensions with file count `find . -type f | sed -e 's/.*\.//' | sed -e 's/.*\///' | sort | uniq -c | sort -rn`.
    # To find files with specific extension `find ./ -type f -name "*.ext"`.
    # 2.
    # `needs_copyright.sh` in different repo's takes care of skipping files which doesn't need the copyright related changes
    # so even if an extension doesn't apply to that repo it should be fine to have it in the mapping. Example case is `.txt`
    # which is excluded in YDBPython isn't in YDB.
    # 3.
    # All the following will be taken care by the default case in code. So no entries required for these in the map.
    #    py start="#" end="#"
    #    yml start="#" end="#"
    #    toml start="#" end="#"
    #    pyi start="#" end="#"
    #    s start="#" end="#"
    #    csh start="#" end="#"
    #    si start="#" end="#"
    #    sh start="#" end="#"
    #    txt start="#" end="#"
    #    gtc start="#" end="#"
    #    awk start="#" end="#"
    #    exp start="#" end="#"
    #    awk start="#" end="#"
    #    sql start="#" end="#"
    #    in start="#" end="#"
    #    exp start="#" end="#"
    #    .cmake start="#" end="#"
    my_cc_map = {
        ".m": ";",
        ".rs": "*",
        ".c": "*",
        ".h": "*",
        ".mpt": ";",
        ".msg": "!",
        ".java": "*",
        "ctemplate": "*",
        ".y": "*",
    }
    current_year = datetime.now().strftime("%Y")
    for line in f:
        # Simple case: existing YottaDB copyright
        # If an end date exists, replace it with this year;
        # Otherwise, add this year as the end date.
        matches = YOTTADB.search(line)
        if matches is not None:
            # Already up to date, no need to run.
            if matches.group("start_date") == current_year or matches.group("end_date") == "-{}".format(current_year):
                print(line, end="")
                return False
            print(YOTTADB.sub("Copyright (c) \g<start_date>-{} YottaDB".format(current_year), line), end="")
            return True
        # More difficult case: no YottaDB copyrights in the file, so we have to add them.
        # This assumes that 'This source code ...' comes after all copyrights.
        elif "This source code contains the intellectual" in line:
            char = my_cc_map.get(ext, "#")
            start, end = (char, char)
            if os.path.basename(f.name) == "git-watcher.cmake":
                print(start, "Copyright (c) {} YottaDB LLC and/or its subsidiaries.".format(current_year))
                print(start, "All rights reserved.")
                print(start)
            else:
                if ext == ".c" or ext == ".h" or ext == ".ctemplate" or ext == ".y":
                    tmp_cc = " " + start
                    if line.startswith(tmp_cc):
                        start = tmp_cc
                elif ext == ".rst":
                    start = ".. " + start
                print(start, " Copyright (c) {} YottaDB LLC and/or its subsidiaries.	".format(current_year), end, sep="")
                print(start, " All rights reserved.                                     ", end, sep="")
                print(start, "								", end, sep="")
            print(line, end="")
            return True
        else:
            # Not a copyright; pass the line through unchanged.
            print(line, end="")
    # If we get here, there was no copyright in the file.
    # Exit with an error
    exit(2)


def main():
    if len(sys.argv) < 2:
        # Missing arguments
        print("ERROR: Missing filename argument to copyright.py", file=sys.stderr)
        exit(1)
    with open(sys.argv[1], "r") as f:
        replaced = look_for_copyrights(f, os.path.splitext(sys.argv[1])[1])
        for line in f:
            print(line, end="")
    if replaced:
        exit(1)


if __name__ == "__main__":
    main()
