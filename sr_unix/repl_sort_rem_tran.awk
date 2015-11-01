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


# repl_sort_rem_tran.awk

# Removes redundant tstart and tcommit extract records

# This script is used when sorting the non-replicated transaction
# file prior to reapplication to the database.  The script
# repl_sort_add_seq.awk is a companion script

# Usage: 
# mupip journal -rollback -backward -resync=<JNL_SEQNO> -lost=lostxn.log "*"
# cat lostxn.log | awk -f repl_sort_add_seq.awk | sort -n -k4 ,4 -t\\ | awk -f repl_sort_rem_tran.awk


BEGIN   { FS="\\"; pnt8=0; pnt9=0 }
        {
                if ("00" == $1  &&  1 != pnt8)
                {
                        printf "08\\%s\\%s\\%s\\%s\\\n",$2,$3,$4,$5;
                        pnt8 = 1;
			pnt9 = 0;
                }
                else if ("09" == $1  &&  1 != pnt9)
                {
                        printf "%s\n",$0;
                        pnt9 = 1;
			pnt8 = 0;
                }
                else if ("00" != $1  &&  "09" != $1)
		{
                        printf "%s\n",$0;
		        pnt8 = 0;
			pnt9 = 0;
	        }
        }
