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
# create the output files from the build and install directory listings and compare them
# returns 1 if the comparison fails and 0 if the comparison passes
source $1/pro/gtmcshrc
mkdir $2/dircompare
cd $2/dircompare
cp $gtm_tools/dircompare.m.txt ./dircompare.m
echo "setenv gtm_dist $gtm_dist"				>&! repeat
echo "setenv gtmroutines '$gtmroutines'"			>>& repeat
echo "$gtm_dist/mumps -r dircompare $2/build.dir '$defgroup'"	>>& repeat
echo "cp $2/install.dir install.out"				>>& repeat

$gtm_dist/mumps -r dircompare $2/build.dir "$defgroup"		> dev_orig.out
cp $2/install.dir install.out
# The names of the files are modified from build.dir -> dev.out (.gtc extension removed)
# So re-sort the file listing. And do not sort the last line which is output of pro/gtmsecsrdir directory
# "head -n -1" is not available on AIX and hence the workaround of doing "head -<totallines - 1> dev_orig.out"
set totallines = `wc -l dev_orig.out`
@ prolines = $totallines[1] - 1
head -$prolines dev_orig.out | sort -k2	>  dev.out
tail -1 dev_orig.out 			>> dev.out
diff dev.out install.out 		>  diff.out
if ($status) then
	echo "DIR-E-COMPARE : dev.out (extracted from $2/build.dir) and install.out (extracted from $2/install.dir) differs"
	echo "Check $PWD/diff.out"
	exit 1
endif
exit 0
