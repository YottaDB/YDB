#################################################################
#								#
#	Copyright 2001 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
#################################################################################################
#
#	gt_as.csh - Assemble Unix assembly language program using GT.M assembly language options.
#		    Note: this procedure only assembles one source file at a time.
#
#	Argument
#		The pathname of a single assembly language program in native dialect.
#
#	Output
#		Object code in file with suffix ".o".
#
#	Environment
#		comlist_gt_as	current assembler command set by comlist.csh for appropriate
#				version and image type
#
#################################################################################################

if ( $?comlist_gt_as == "0" ) then
	echo "gt_as-E-nocomlist: gt_as.csh should only be invoked from comlist.csh"
	exit 1
endif

alias	gt_as_local	"$comlist_gt_as"

gt_as_local $1
