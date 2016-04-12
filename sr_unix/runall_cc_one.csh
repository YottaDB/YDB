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

# This does compile of one module on behalf of runall.csh/runall_cc.csh
#
# $1 - linkonly
# $2 - one filename to compile
#

set linkonly = $1
set file = $2

source $gtm_tools/gtm_env.csh
alias runall_cc gt_cc_${RUNALL_IMAGE}
alias gt_as $gt_as_assembler $gt_as_options_common $gt_as_option_I $RUNALL_EXTRA_AS_FLAGS
alias runall_as gt_as_${RUNALL_IMAGE}

set file = $file:t
set ext = $file:e
if ("$ext" == "") then
	set ext = "c"
endif
set file = $file:r		# take the non-extension part for the obj file
set objfile = ${file}.o

@ runall_status = 0
if ($linkonly == 0) then
	# remove pre-existing object files before the current compilation
	# to ensure they do not get used if the current compile fails
	rm -f $gtm_obj/$file.o
	if ($ext == "s") then
		echo "$gtm_src/$file.$ext   ---->  $gtm_obj/$file.o"
		runall_as $gtm_src/${file}.s
		if (0 != $status) @ runall_status = $status
	else if ($ext == "c") then
		echo "$gtm_src/$file.$ext   ---->  $gtm_obj/$file.o"
		runall_cc $RUNALL_EXTRA_CC_FLAGS $gtm_src/$file.c
		if (0 != $status) @ runall_status = $status
		if ($file == "omi_srvc_xct") then
			chmod a+w $gtm_src/omi_sx_play.c
			\cp $gtm_src/omi_srvc_xct.c $gtm_src/omi_sx_play.c
			chmod a-w $gtm_src/omi_sx_play.c
			echo "$gtm_src/omi_sx_play.c   ---->  $gtm_obj/omi_sx_play.o"
			# remove pre-existing object
			rm -f $gtm_obj/omi_sx_play.o
			runall_cc -DFILE_TCP $RUNALL_EXTRA_CC_FLAGS $gtm_src/omi_sx_play.c
			if (0 != $status) @ runall_status = $status
		endif
	else if ($ext == "msg") then
		echo "$gtm_src/$file.$ext   ---->  $gtm_obj/${file}_ctl.c  ---->  $gtm_obj/${file}_ctl.o"
		# gtm_startup_chk requires gtm_dist setup
		rm -f ${file}_ctl.c ${file}_ansi.h	# in case an old version is lying around
		set real_gtm_dist = "$gtm_dist"
		if ($?gtmroutines) set save_gtmroutines = "$gtmroutines"
		setenv gtm_dist "$gtm_root/$gtm_curpro/pro"
		setenv gtmroutines "$gtm_obj($gtm_pct)"
		$gtm_root/$gtm_curpro/pro/mumps -run msg $gtm_src/$file.msg Unix
		if (0 != $status) @ runall_status = $status
		setenv gtm_dist "$real_gtm_dist"
		unset real_gtm_dist
		if ($?save_gtmroutines) setenv gtmroutines "$save_gtmroutines"
		\mv -f ${file}_ctl.c $gtm_src/${file}_ctl.c
		if ( -f ${file}_ansi.h ) then
			\mv -f ${file}_ansi.h $gtm_inc
		endif
		runall_cc $RUNALL_EXTRA_CC_FLAGS $gtm_src/${file}_ctl.c
		if (0 != $status) @ runall_status = $status
		set objfile = ${file}_ctl.o
	endif
endif

# Note: TMP_DIR env var is set by grandparent caller runall.csh
set library=`grep "^$file " ${TMP_DIR}_exclude`
if ("$library" == "") then
	set library=`grep " $file " ${TMP_DIR}_list | awk '{print $1}' | uniq`
	if ("$library" == "") then
		set library="libmumps.a"
	endif
	echo $objfile >> ${TMP_DIR}_lib_${file}.$library
else
	if ("$library[1]" != "$library") then
		echo $library[2] >> ${TMP_DIR}_main_${file}.misc
		if ($file == "omi_srvc_xct.c") then
			echo "gtcm_play" >> ${TMP_DIR}_main_${file}.misc
		endif
	endif
endif

exit $runall_status
