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

#
#################################################################
#
#	newverdir.csh - set up directories for a new GT.M version
#
#################################################################

set newverdir_dirname = "`$shell $gtm_tools/gtm_version_dirname.csh $1`"
mkdir $gtm_root/$newverdir_dirname
cd    $gtm_root/$newverdir_dirname

mkdir inc pct src tools
chmod 775 inc pct src tools

cp $gtm_tools/gtmsrc.csh .
chmod 664 gtmsrc.csh

mkdir bta dbg pro log
chmod 775 bta dbg pro

cd bta
mkdir map obj
chmod 775 map obj

cd ../dbg
mkdir map obj
chmod 775 map obj

cd ../pro
mkdir map obj
chmod 775 map obj

cd ..
unset newverdir_dirname
