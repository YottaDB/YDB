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
###########################################################################################
#
#	gt_cc.csh - Compile C programs from $gtm_src
#
#	Arguments
#		A list of C source file names (minus directory paths).
#
#	Output
#		Object code in file with suffix ".o".
#
#	Environment
#		comlist_gt_cc	current C compiler alias set by comlist.csh for appropriate
#				version and image type
#
###########################################################################################

if ( $?comlist_gt_cc == "0" ) then
	echo "gt_cc-E-nocomlist: gt_cc.csh should only be invoked from within comlist.csh"
	exit 1
endif

set	gs = ($gtm_src)

alias	gt_cc_local	"$comlist_gt_cc"

foreach i ($*)
	echo $gs[1]/$i
	gt_cc_local $gs[1]/$i
end
