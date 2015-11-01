#!/usr/local/bin/tcsh
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


echo ""
if ($1 == "") then
	echo $0 "[ctags|tags|xrefs|cscope|all]"
	echo ""
	echo "      ctags | tags ---> build the tag database in $gtm_ver/misc/tags"
	echo "      xrefs        ---> build the cross-reference database in $gtm_ver/misc"
	echo "      cscope       ---> build the cscope database in $gtm_ver/misc/cscope.out (currently works in solaris only)"
	echo "      all          ---> build the tag-database and xrefs database and if-possible cscope-database"
	echo ""
	exit
endif

echo ""
echo "Building tags and/or xrefs and/or cscope for --- $gtm_ver"
echo ""

set host = $HOST:r:r:r

if ($host == "mars" || $host == "sol") then
	set user=`id | sed 's/) .*//g' | sed 's/.*(//g'`
else
	set user=`id -u -n`
endif

set build_cscope = 0
set build_ctags = 0
set build_xrefs = 0

while ($1 != "")
	switch($1)
		case cscope:
			set build_cscope = 1; shift;
			breaksw;
		case ctags:
		case tags:
			set build_ctags = 1; shift;
			breaksw;
		case xrefs:
			set build_xrefs = 1; shift;
			breaksw;
		case all:
			set build_cscope = 1; set build_ctags = 1; set build_xrefs = 1; shift;
			breaksw;
	endsw
end

echo $build_cscope
echo $build_ctags
echo $build_xrefs

set workdir = $gtm_ver/misc

if !(-e $workdir) then
	echo "Creating directory ------- $workdir -------------"
	mkdir -p $workdir
	cp /gtc/gtm/misc/* $workdir
endif

setenv gtmroutines "$gtm_ver/misc($gtm_ver/misc $gtm_dist)"
set tmpfile = /tmp/__xrefs__${user}_${host}_${gtm_verno}.tags
rm -f /tmp/__xrefs__${user}_* >& /dev/null

setenv gtm_xrefs $workdir

if (($host == "sol" || $host == "mars") && ($build_cscope != 0)) then
	echo ""
	echo "Building database ----------- cscope -------- $workdir/cscope.out"
	echo ""
	rm -f $workdir/cscope.out >& /dev/null
	cscope -f $workdir/cscope.out -b $gtm_src/*.c $gtm_inc/*.h
endif

if ($build_ctags != 0) then
	rm -f $workdir/tags >& /dev/null

	ls -1 $gtm_src | sed 's/^/\/usr\/library\/'$gtm_verno'\/src\//g' >! $tmpfile
	ls -1 $gtm_inc | sed 's/^/\/usr\/library\/'$gtm_verno'\/inc\//g' >>! $tmpfile
	echo ""
	echo "Building database ----------- ctags -------- $workdir/tags"
	echo ""

	/usr/local/bin/ctags -L $tmpfile -o $workdir/tags

	if ($status == 0) then
		rm -f $tmpfile >& /dev/null
	endif
endif

echo ""

if ($build_xrefs == 1) then
	echo "Building database ----------- xrefs -------- $workdir"
	echo ""
	$gtm_xrefs/bldxrefs
endif
