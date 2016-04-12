#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2015 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# This is modeled on gt_cc.csh. It issues parallel compilations and aggregates results for caller (runall.csh)
#
# $1 - linkonly
# $2 onwards - list of filenames to compile
#

set linkonly = $1
shift

set filelist = ( $* )

set cmdfile="$gtm_log/runall_$$__batch.csh"
set background="&"

rm -f $cmdfile.err
set dollar = '$'
set err_check = "if (${dollar}status) touch $cmdfile.err"
foreach cfile ($filelist)
	set outfile="$gtm_log/runall_$$_${cfile:t:r}.out"
	set redir=">& $outfile"
	echo "($gtm_tools/runall_cc_one.csh $linkonly $cfile; ${err_check}) $redir $background" >> $cmdfile
end

echo "wait" >> $cmdfile

set cmdout="$gtm_log/runall_$$__batch.out"
source $cmdfile >& $cmdout

set stat=$status

foreach cfile ($filelist)
	set file = ${cfile:t:r}
	set outfile="$gtm_log/runall_$$_$file.out"
	/bin/cat $outfile
	/bin/rm $outfile
	# Note: TMP_DIR env var is set by parent caller runall.csh
	# Check if a file of the form ${TMP_DIR}_lib_${file}.* exists.
	# If so move it to ${TMP_DIR}_lib_.*
	set filename = `ls -1 ${TMP_DIR}_lib_${file}.* |& grep ${TMP_DIR}_lib_${file}`
	if ("" != "$filename") then
		set newfilename = `echo $filename | sed 's/'$file'//g'`
		cat $filename >> $newfilename
		rm -f $filename
	endif
	# Check if a file of the form ${TMP_DIR}_main_${file}.misc exists.
	if (-e ${TMP_DIR}_main_${file}.misc) then
		cat ${TMP_DIR}_main_${file}.misc >> ${TMP_DIR}_main_.misc
		rm -f ${TMP_DIR}_main_${file}.misc
	endif
end

set exit_status = 0

if ($stat) then
	/bin/cat $cmdout
	set exit_status = 1
else
	/bin/rm $cmdfile
	/bin/rm $cmdout
endif

if (-e $cmdfile.err) then
	rm -f $cmdfile.err
	set exit_status = 1
endif

exit $exit_status
