#################################################################
#								#
#	Copyright 2001, 2009 Fidelity Information Services, Inc	#
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

set platform_name = `uname | sed 's/-//g' | sed 's,/,,' | tr '[A-Z]' '[a-z]'`
set mach_type = `uname -m`

if ( "ia64" == $mach_type && "linux" == $platform_name ) then
    set lfile = `basename $1`:r
    set file = $lfile:r

    gt_cpp -E $1 > ${gtm_obj}/${file}_cpp.s
    gt_as_local ${gtm_obj}/${file}_cpp.s -o ${gtm_obj}/${file}.o
    \rm ${gtm_obj}/${file}_cpp.s
else if ( "os390" == $platform_name ) then
    set file = `basename $1`
    set file = $file:r

    gt_as_local $1
    if ( -e $gtm_obj/${file}.dbg )  chmod ugo+r $gtm_obj/${file}.dbg
else
    gt_as_local $1
endif

