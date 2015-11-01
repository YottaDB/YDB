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

set upperc_cp_ext = ".m"
foreach upperc_cp_i ($*)
	set upperc_cp_base = `basename $upperc_cp_i $upperc_cp_ext`
	if ( "$upperc_cp_i" != "$upperc_cp_base" ) then
		# If they _don't_ match, then basename found $upperc_cp_ext and stripped it off.
		set upperc_cp_newbase = `echo $upperc_cp_base | tr '[a-z]' '[A-Z]'`
		if ( "$upperc_cp_base" != "$upperc_cp_newbase" ) then
			# If _not_ overwriting an existing file.
			set upperc_cp_dirname = `dirname $upperc_cp_i`
			echo "cp $upperc_cp_dirname/$upperc_cp_i ---> $upperc_cp_dirname/$upperc_cp_newbase$upperc_cp_ext"
			cp $upperc_cp_dirname/$upperc_cp_i $upperc_cp_dirname/$upperc_cp_newbase$upperc_cp_ext
		endif
	endif
end

unset upperc_cp_base
unset upperc_cp_dirname
unset upperc_cp_ext
unset upperc_cp_i
unset upperc_cp_newbase
