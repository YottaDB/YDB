#!/bin/csh
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

set lowerc_cp_ext = ".m"
foreach lowerc_cp_i ($*)
	set lowerc_cp_base = `basename $lowerc_cp_i $lowerc_cp_ext`
	if ( "lowerc_cp_i" != "$lowerc_cp_base" ) then
		# If they _don't_ match, then basename found $lowerc_cp_ext and stripped it off.
		set lowerc_cp_newbase = `echo $lowerc_cp_base | tr '[A-Z]' '[a-z]'`
		if ( "$lowerc_cp_base"  != "$lowerc_cp_newbase" ) then
			# If _not_ overwriting an existing file.
			set lowerc_cp_dirname = `dirname $lowerc_cp_i`
			echo "cp $lowerc_cp_dirname/$lowerc_cp_i ---> $lowerc_cp_dirname/$lowerc_cp_newbase$lowerc_cp_ext"
			cp $lowerc_cp_dirname/$lowerc_cp_i $lowerc_cp_dirname/$lowerc_cp_newbase$lowerc_cp_ext
		endif
	endif
end

unset lowerc_cp_base
unset lowerc_cp_dirname
unset lowerc_cp_ext
unset lowerc_cp_i
unset lowerc_cp_newbase
