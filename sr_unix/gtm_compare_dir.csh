#!/usr/local/bin/tcsh
#################################################################
#								#
# Copyright (c) 2011-2020 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
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
mumps -run ^GDE <<GDE_EOF
ch -r DEFAULT -KEY_SIZE=252
exit
GDE_EOF
mupip create
cp $gtm_tools/dircompare.m.txt ./dircompare.m
echo "setenv gtm_dist $gtm_dist"				>&! repeat
echo "setenv gtmroutines '$gtmroutines'"			>>& repeat
echo "setenv gtmgbldir mumps.gld"				>>& repeat
echo "$gtm_dist/mumps -r dircompare $2/build.dir $3 NOP $5"	>>& repeat
echo "$gtm_dist/mumps -r dircompare $2/install.dir NOP NOP $5"	>>& repeat
$gtm_dist/mumps -r dircompare $2/build.dir $3 NOP $5 > dev.out
$gtm_dist/mumps -r dircompare $2/install.dir NOP NOP $5 > install.out
diff dev.out install.out > diff.out
set numdiff=`wc -l < diff.out`
echo
echo "number of lines difference = $numdiff"
exit $numdiff
