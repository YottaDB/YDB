#################################################################
#                                                               #
#       Copyright 2010 Fidelity Information Services, Inc       #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################
# Generate ttt.c from $gtm_tools/ttt.txt, $gtm_inc/opcode_def.h, and $gtm_inc/vxi.h, if needed

if (-e $work_dir/vvms/new) then
	cd $work_dir/port/new/
	foreach cmpnt (opcode_def.h vxi.h)
		if (-e $cmpnt) then
			echo "Using existing $cmpnt"
		else
			$work_tools/workfetch.csh $cmpnt
		endif
	end
	cd $work_dir/vvms/new/
	foreach cmpnt (ttt.txt ttt.c)
		if (-e $cmpnt) then
			echo "Using existing $cmpnt"
		else
			$work_tools/workfetch.csh $cmpnt
		endif
	end
	set save_gtm_dist = "$gtm_dist"
	setenv gtm_dist "$gtm_root/$gtm_curpro/pro"
	set save_gtmroutines = "$gtmroutines"
	setenv gtmroutines ". $gtm_obj($gtm_pct)"
	if (-e ttt.c) then
		chmod +w ttt.c
		rm -f ttt.c
	endif
	set timestamp = `date +%m%d_%H%M%S`
	$gtm_root/$gtm_curpro/pro/mumps -run tttgen "ttt.txt $work_dir/port/new/opcode_def.h $work_dir/port/new/vxi.h"
	# remove the .o-s we just created so they're not put into libraries
	rm -f chk2lev.o chkop.o gendash.o genout.o loadop.o loadvx.o tttgen.o tttscan.o
	setenv gtmroutines "$save_gtmroutines"
	unset save_gtmroutines
	setenv gtm_dist "$save_gtm_dist"
	unset save_gtm_dist
	cd ../../
	echo "workclean may be needed to clear out implicitly fetched modules"
else
	echo "No vvms/new so No action"
endif
