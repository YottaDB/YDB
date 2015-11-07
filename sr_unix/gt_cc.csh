#################################################################
#								#
#	Copyright 2001, 2013 Fidelity Information Services, Inc	#
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

alias	gt_cc_local	"comlist_gt_cc"

set cfilelist=($*)
set cmdfile="$gtm_log/gt_cc_$$__batch.csh"
set background="&"
if ($HOST:r:r:r =~ {snail,turtle}) set background=""

echo 'alias	gt_cc_local	"$comlist_gt_cc"' >> $cmdfile

foreach cfile ($cfilelist)
	set outfile="$gtm_log/gt_cc_$$_${cfile:t:r}.out"
	set redir=">& $outfile"
	echo "(echo $cfile ; eval 'gt_cc_local $cfile') $redir $background" >> $cmdfile
end

echo "wait" >> $cmdfile

set cmdout="$gtm_log/gt_cc_$$__batch.out"
source $cmdfile >& $cmdout

set stat=$status

foreach cfile ($cfilelist)
	set outfile="$gtm_log/gt_cc_$$_${cfile:t:r}.out"
	/bin/cat $outfile
	/bin/rm $outfile
end

if ($stat) then
	/bin/cat $cmdout
else
	/bin/rm $cmdfile
	/bin/rm $cmdout
endif

exit 0
