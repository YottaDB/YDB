#!/usr/local/bin/tcsh
#################################################################
#								#
#	Copyright 2011 Fidelity Information Services, Inc       #
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
# gtm_compare_dir.csh $install $tmp_dist deletefile addfile dirdeletefile osname
# create the output files from the build and install directory listings and compare them
# returns the number of lines which are different - should be 0
source $1/pro/gtmcshrc
mkdir $2/dircompare
cd $2/dircompare
setenv gtmgbldir mumps.gld
mumps -run ^GDE <<GDE_EOF >& /dev/null
ch -r DEFAULT -KEY_SIZE=252
exit
GDE_EOF
mupip create >& /dev/null
cp $gtm_tools/dircompare.m.txt ./dircompare.m
mumps -r dircompare $2/build.dir $3 $4 $5 > bout
mumps -r dircompare $2/install.dir NOP NOP $5 > iout
if ($6 == "os390") then
	# we expect 4 diff lines at the beginning on zos so account for them
	diff bout iout | tail -n +5 > diff.out
else
	diff bout iout > diff.out
endif
set numdiff=`wc -l < diff.out`
echo
echo "number of lines difference = $numdiff"
exit $numdiff
