#!/bin/ksh
#################################################################
#                                                               #
#       Copyright 2009 Fidelity Information Services, Inc 	#
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

if [[ $# -ne 1 ]]; then
	echo "Usage: $0 <entropy_pid>"
	exit 1
fi
pid=$1
wait_for=600
iter=0
while [[ $iter -ne $wait_for ]]; do
	sleep 1
	iter=`expr $iter + 2`
done
kill -9 $pid
