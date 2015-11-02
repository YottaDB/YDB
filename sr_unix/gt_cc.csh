#################################################################
#								#
#	Copyright 2001, 2012 Fidelity Information Services, Inc	#
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

# Bourne shell is less verbose about background processes, so use that.

# However, comlist_gt_cc is generally an alias, which we can't pass to the Bourne shell,
# so expand it repeatedly until it isn't an alias anymore.
# Assumes aliases expand as basic commands.

set comlist_gt_cc=($comlist_gt_cc)

while(`alias $comlist_gt_cc[1]` != "")
	set comlist_gt_cc= ( `alias $comlist_gt_cc[1]` $comlist_gt_cc[2-$#comlist_gt_cc] )
end

/bin/sh <<ENDSH
files="$*"

for i in \$files
do
	outfile="$gtm_log/gt_cc_local_$$_\`basename \$i\`.out"
	echo \$i > \$outfile
	$comlist_gt_cc \$i >> \$outfile 2>&1 &
done

wait

for i in \$files
do
	outfile="$gtm_log/gt_cc_local_$$_\`basename \$i\`.out"
	/bin/cat \$outfile
	/bin/rm \$outfile
done
ENDSH

exit 0
