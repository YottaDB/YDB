#! /usr/local/bin/tcsh -f
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
#############################################################################################
#
#	this shell routine sets up the environment and runs the tests
#
##############################################################################################
#
if ("$test_want_concurrency" == "yes") then
	pushd $test_load_dir
	set old_gtmroutines = "$gtmroutines"
	set run_test_temp_hostname = `basename $HOST .sanchez.com`
	setenv gtmroutines "."
	setenv gtmgbldir load.gld
	$gtm_exe/mumps -direct << GTMEOF
	d ^load("$tst","$run_test_temp_hostname",$$,$tst_num)
GTMEOF
	setenv gtmroutines "$old_gtmroutines"
	popd
endif

setenv gtmgbldir ./mumps.gld
setenv GTM "$gtm_exe/mumps -direct"
setenv MUPIP "$gtm_exe/mupip"
setenv LKE "$gtm_exe/lke"
setenv DSE "$gtm_exe/dse"
setenv GDE "$gtm_exe/mumps -r GDE"
setenv gtmroutines ".($gtm_tst/com $gtm_tst/$tst/inref .) $gtm_exe"
#
echo " "
echo $gtmgbldir
echo $GTM
echo $MUPIP
echo $DSE
echo $GDE
echo $LKE
echo $gtmroutines
echo " "
echo Testing $tst
echo `pwd`
#
switch ($HOSTOS)
case "AIX":
case "HP-UX":
        setenv local_awk awk
        breaksw
case "OSF1":
        setenv local_awk gawk
        breaksw
case "SunOS":
	setenv local_awk nawk
        breaksw
default:
        setenv local_awk awk
	echo "run_test.csh : (Warning) Unsupported OS version while choosing an implmentation of awk to run"
	echo "                Setting to the alias local_awk to default value awk"
        breaksw
endsw
#
if ($?test_no_background != 0) then
	$shell $gtm_tst/$tst/instream.csh |& tee $tst_dir/$gtm_tst_out/$tst/outstream.log
else
	$shell $gtm_tst/$tst/instream.csh >& $tst_dir/$gtm_tst_out/$tst/outstream.log
endif
#
$local_awk -f $gtm_tst/com/outref.awk $gtm_tst/$tst/outref.txt >&! $tst_dir/$gtm_tst_out/$tst/outstream.cmp
#
diff -b $tst_dir/$gtm_tst_out/$tst/outstream.cmp $tst_dir/$gtm_tst_out/$tst/outstream.log >& $tst_dir/$gtm_tst_out/$tst/diff.log
set stat = "$status"
#
echo "$ver" > $tst_dir/$gtm_tst_out/$tst/tmp/mail.tmp
echo "$image" >> $tst_dir/$gtm_tst_out/$tst/tmp/mail.tmp
echo "$tst" >> $tst_dir/$gtm_tst_out/$tst/tmp/mail.tmp
#
$GTM << xyzzy
Open "mail.tmp"
Use "mail.tmp"
Read ver
Read image
Read tst
Close "mail.tmp"
;
Zsystem
Set sver=\$Extract(ver,1,2)_"."_\$Extract(ver,3,3)_"-"_\$Extract(ver,4,\$Length(ver))
If \$Zversion[(" "_sver_" ") Set vmatch="Version match -- Expected: '"_sver_"', Actual: '"_\$Zversion,submat="version matched"
Else  Set vmatch="Version mismatch FAILURE  -- Expected: '"_sver_"', Actual: '"_\$Zversion,submat="*VERSION MISMATCH*"
Write vmatch
;
Open "match.msg"
Use "match.msg"
Write submat
Close "match.msg"
xyzzy
#

if ("$test_want_concurrency" == "yes") then
	pushd $test_load_dir
	set run_test_temp_hostname = `basename $HOST .sanchez.com`
	set old_gtmroutines = "$gtmroutines"
	setenv gtmroutines "."
	setenv gtmgbldir load.gld
	$gtm_exe/mumps -direct << GTMEOF
	d ^unload("$tst","$run_test_temp_hostname",$$,$tst_num)
GTMEOF
	setenv gtmroutines "$old_gtmroutines"
	popd
endif

################	Routine to send mail 	##############

if ( $stat == 0 ) then
	set subject="`basename $HOST .sanchez.com` $ver $image $tst PASSED $tst_dir/$gtm_tst_out `cat match.msg`"
	mailx -s "$subject" $mailing_list < $tst_dir/$gtm_tst_out_save/config.log
	cd $tst_dir/$gtm_tst_out/$tst
	rm -rf $tst_dir/$gtm_tst_out/$tst/tmp/
	rm -f $tst_dir/$gtm_tst_out/$tst/diff.log
	rm -f $tst_dir/$gtm_tst_out/$tst/outstream.cmp
else
	set subject="`basename $HOST .sanchez.com` $ver $image $tst *FAILED* $tst_dir/$gtm_tst_out `cat match.msg`"
	mailx -s "$subject" $mailing_list < $tst_dir/$gtm_tst_out/$tst/diff.log
endif
