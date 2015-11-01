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

set lowerc_ext = ".m"
foreach lowerc_i ($*)
	set lowerc_base = `basename $lowerc_i $lowerc_ext`
	if ( "lowerc_i" != "$lowerc_base" ) then
		# If they _don't_ match, then basename found $lowerc_ext and stripped it off.
		set lowerc_newbase = `echo $lowerc_base | tr '[A-Z]' '[a-z]'`
		if ( "$lowerc_base"  != "$lowerc_newbase" ) then
			# If _not_ overwriting an existing file.
			set lowerc_dirname = `dirname $lowerc_i`
			echo "mv $lowerc_dirname/$lowerc_i ---> $lowerc_dirname/$lowerc_newbase$lowerc_ext"
			mv $lowerc_dirname/$lowerc_i $lowerc_dirname/$lowerc_newbase$lowerc_ext
		endif
	endif
end

unset lowerc_base
unset lowerc_dirname
unset lowerc_ext
unset lowerc_i
unset lowerc_newbase
