#################################################################
#                                                               #
#       Copyright 2008 Fidelity Information Services, Inc       #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

set save_gtm_dist = "$gtm_dist"
setenv gtm_dist "$gtm_root/$gtm_curpro/pro"
set save_gtmroutines = "$gtmroutines"
setenv gtmroutines "$gtm_obj($gtm_pct)"

# Generate ttt.c from $gtm_tools/ttt.txt, $gtm_inc/opcode_def.h, and $gtm_inc/vxi.h, if needed
if (-e ttt.c && $gtm_verno !~ V9*) then
	echo "GTN_TTT-I-EXIST : ttt.c already exists for production version $gtm_verno. Not recreating."
else
	if ((-e ttt.c) && ((-M $gtm_tools/ttt.txt) <= (-M $gtm_src/ttt.c))) then
		echo "GEN_TTT-I-EXIST : ttt.c already exists for development version $gtm_verno. Recreating."
	else
		echo "GEN_TTT-I-NOTEXIST : ttt.c out of date or missing. Recreating."
	endif
	if (-e ttt.c) then
		chmod +w ttt.c
		rm -f ttt.c
	endif
	cd $gtm_exe/obj
	cp $gtm_inc/opcode_def.h $gtm_inc/vxi.h $gtm_tools/ttt.txt .
	set timestamp = `date +%m%d_%H%M%S`
	$gtm_root/$gtm_curpro/pro/mumps -direct <<GTM_in_tttgen >& $gtm_log/tttgen_$timestamp.log
Set \$ZROUTINES=". $gtmroutines"
Do ^tttgen
ZContinue
Halt
GTM_in_tttgen
	cp ttt.c $gtm_src
	chmod 444 $gtm_src/ttt.c
	# clean up the files that we just copied here and the generated ttt.c
	chmod +w ttt.c opcode_def.h vxi.h ttt.txt
	rm -f ttt.c opcode_def.h vxi.h ttt.txt
	# remove the .o-s we just created so they're not put into libraries
	rm -f chk2lev.o chkop.o gendash.o genout.o loadop.o loadvx.o tttgen.o tttscan.o
endif
setenv gtmroutines "$save_gtmroutines"
unset save_gtmroutines
setenv gtm_dist "$save_gtm_dist"
unset save_gtm_dist
