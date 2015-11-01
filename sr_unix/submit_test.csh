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

# This shell submits  the GTM tests
#

setenv gtm_tst_out_save $gtm_tst_out

@ runs = 0
while ($test_num_runs != $runs)
	if ($test_num_runs != 1) then
		setenv gtm_tst_out ${gtm_tst_out_save}/run_${runs}
		mkdir $tst_dir/$gtm_tst_out
	endif
	@ j = 1
	foreach i ($tst_suite)
		setenv tst $i
		mkdir $tst_dir/$gtm_tst_out/$tst
		mkdir $tst_dir/$gtm_tst_out/$tst/tmp
		cd $tst_dir/$gtm_tst_out/$tst/tmp
		if ( $?test_no_background != 0) then
			$shell $gtm_tst/com/run_test.csh |& tee $tst_dir/$gtm_tst_out/$tst/$tst.log
		else if ($test_want_concurrency != "yes") then
			$shell $gtm_tst/com/run_test.csh >& $tst_dir/$gtm_tst_out/$tst/$tst.log
		else
			setenv tst_num $j
			$shell $gtm_tst/com/run_test.csh $i >& $tst_dir/$gtm_tst_out/$tst/$tst.log &
			@ j = $j + 1
		endif
	end
	@ runs = $runs + 1
end
#
