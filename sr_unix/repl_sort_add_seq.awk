#################################################################
#                                                               #
#       Copyright 2002 Sanchez Computer Associates, Inc.  	#
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################


# repl_sort_add_seq.awk

# awk script to put journal sequence number into tstart record
# This script is used when sorting the non-replicated transaction
# file prior to reapplication to the database.  The script
# repl_sort_rem_tran.awk is a companion script.

# Usage:
# mupip journal -rollback -backward -resync=<JNL_SEQNO> -lost=lostxn.log "*"
# cat lostxn.log | awk -f repl_sort_add_seq.awk | sort -n -k4 ,4 -t\\ | awk -f repl_sort_rem_tran.awk
 

BEGIN   { FS="\\"; tstart = 0;}
        {
                if ("08" == $1)
                {
                        tstart = 1;
                        printf "00\\%s\\%s\\",$2,$3;
                        tid = $5;
                }
                else
                {
                        if (1 == tstart)
                        {
                                printf "%s\\%s\\\n",$4,tid;
                                tstart = 0;
                        }
                        printf "%s\n",$0;
                }
        }
