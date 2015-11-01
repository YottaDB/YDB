#!/usr/local/bin/tcsh -f
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


source $gtm_tools/gtm_env.csh	# to get the alias for version and other gt_cc* stuff

#######################################################
# This shell runs the GTM tests. The tests can be run #
# as all or individually. Test results will be mailed #
# to the list of person(s) in the mailing list        #
#######################################################
#
#
if ( $?HOSTOS == "0" )		setenv HOSTOS `uname -s`	# operating system
#
#

#------------------------------------------------------------------------
#
#	Important assumption is that all test environment variables are
#	defined at tcsh startup. Therefore no need to check for
#	whether the variables are defined.
#
#------------------------------------------------------------------------



#########################################################################
#
#	Get the test_version
#
#########################################################################

if ("$test_version" == "") then
	set test_version_defined = 0
else if ("$test_version" == "default") then
	set test_version_defined = -1
else
	set test_version_defined = 1
endif

echo " "

set test_default_version = $gtm_verno

echo -n "Enter Version	"
echo -n "[$test_default_version]: -------------> "

if ($test_version_defined == 0) then
	setenv ver $<
else if ($test_version_defined == -1) then
	setenv ver $test_default_version
	echo $ver
else
	setenv ver $test_version
	echo $ver
endif

if ("$ver" == "") then
	setenv ver $test_default_version
endif

echo " "

#########################################################################
#
#	Get the test_image
#
#########################################################################

if ("$test_image" == "") then
	set test_image_defined = 0
else if ("$test_image" == "default") then
	set test_image_defined = -1
else
	set test_image_defined = 1
endif

echo " "

if ( "$gtm_exe" == "" ) then
	set test_image_def = "p"
else
	set test_image_def = `basename $gtm_exe`
	switch ( $test_image_def )
	case "b*":
		set test_image_def = "b"
		breaksw
	case "d*":
		set test_image_def = "d"
		breaksw
	case "p*":
	default:
		set test_image_def = "p"
		breaksw
	endsw
endif

echo -n "Enter Image	"
echo -n "b, d, or p [$test_image_def]: ------------> "

if ($test_image_defined == 0) then
	setenv image $<
else if ($test_image_defined == -1) then
	setenv image $test_image_def
	echo $image
else
	setenv image $test_image
	echo $image
endif

if ( "$image" == "" ) then
	setenv image $test_image_def
endif

echo " "



#########################################################################
#
#	Get the test_mail_list
#
#########################################################################

if ("$test_mail_list" == "") then
	set test_mail_list_defined = 0
else if ("$test_mail_list" == "default") then
	set test_mail_list_defined = -1
else
	set test_mail_list_defined = 1
endif

set test_default_mail_list = "$USER"

echo " "

set test_temp_hostname = `basename $HOST .sanchez.com`
set test_temp_verify_mailnames = 1
set test_temp_mailnames
set test_temp_i

while ( $test_temp_verify_mailnames == 1 )
	set test_temp_verify_mailnames = 0
	setenv mailing_list ""
	echo -n "Enter mailing list	"
	echo -n "[$test_default_mail_list]: ------------> "

	if ($test_mail_list_defined == 0) then
		set test_temp_mailnames = "$<"
	else if ($test_mail_list_defined == -1) then
		set test_temp_mailnames = $test_default_mail_list
		echo $test_temp_mailnames
	else
		set test_temp_mailnames = "$test_mail_list"
		echo $test_temp_mailnames
	endif

	if ( "$test_temp_mailnames" == "" ) then
		set test_temp_mailnames = $test_default_mail_list
	endif

	set test_temp_mailnames = "`echo $test_temp_mailnames | sed 's/,/ /g'`"
	foreach test_temp_i ( $test_temp_mailnames )

		set test_temp_i = `basename $test_temp_i @sanchez.com`
		set test_temp_i = `basename $test_temp_i @mailgst.sanchez.com`
		if ( $test_temp_i == "library"  ||  $test_temp_i == "root" ) then
			echo "$0 will not email test results to $test_temp_i; you will need to enter a new mailing list"
			set test_temp_verify_mailnames = 1
			set test_mail_list_defined = 0		# to ask for a correct mail list
								# although the user has defined one
								# to avoid an infinite loop
		endif

		if ( $test_temp_hostname == "mars"  ||  $test_temp_hostname == "p1" ) then
			set test_temp_i = "${test_temp_i}@sanchez.com"
		endif
		if ( $mailing_list == "" ) then
			setenv mailing_list "$test_temp_i"			# no leading space
		else
			setenv mailing_list "$mailing_list $test_temp_i"
		endif
	end
end

echo ""
unset test_temp_i
unset test_temp_mailnames
unset test_temp_verify_mailnames




#########################################################################
#
#	Get the test_output_dir
#
#########################################################################

if ("$test_output_dir" == "") then
	set test_output_dir_defined = 0
else if ("$test_output_dir" == "default") then
	set test_output_dir_defined = -1
else
	set test_output_dir_defined = 1
endif

echo " "

set test_default_output_dir = "/testarea/$USER"

echo -n "Test output dir	"
echo -n "[$test_default_output_dir]:"
echo -n " ------------> "
if ($test_output_dir_defined == 0) then
	setenv tst_dir $<
else if ($test_output_dir_defined == -1) then
	setenv tst_dir $test_default_output_dir
	echo $tst_dir
else
	setenv tst_dir $test_output_dir
	echo $tst_dir
endif
if ( "$tst_dir" == "" ) then
	setenv tst_dir $test_default_output_dir
endif

echo " "


#########################################################################
#
#	Get the test_remote_user
#
#########################################################################
if ($?test_remote_user == 0) then
	setenv test_remote_user ""
endif
if ("$test_remote_user" == "") then
	set test_remote_user_defined = 0
else if ("$test_remote_user" == "default") then
	set test_remote_user_defined = -1
else
	set test_remote_user_defined = 1
endif
set test_remote_default_user = $USER
setenv tst_remote_user $test_remote_default_user
if ($?test_replic == 1) then
	echo " "
	echo -n "Enter Remote User Name	"
	echo -n "[$test_remote_default_user]: -------------> "

	if ($test_remote_user_defined == 0) then
		setenv tst_remote_user $<
	else if ($test_remote_user_defined == -1) then
		setenv tst_remote_user $test_remote_default_user
		echo $tst_remote_user
	else
		setenv tst_remote_user $test_remote_user
		echo $tst_remote_user
	endif
	if ("$tst_remote_user" == "") then
		setenv tst_remote_user $test_remote_default_user
	endif
	echo " "
endif


#########################################################################
#
#	Get the test_remote_version
#
#########################################################################
if ($?test_remote_version == 0) then
	setenv test_remote_version ""
endif
if ("$test_remote_version" == "") then
	set test_remote_version_defined = 0
else if ("$test_remote_version" == "default") then
	set test_remote_version_defined = -1
else
	set test_remote_version_defined = 1
endif
set test_remote_default_version = $gtm_verno
setenv remote_ver $test_remote_default_version
if ($?test_replic == 1) then
	echo ""
	echo -n "Enter Remote Version	"
	echo -n "[$test_remote_default_version]: -------------> "
	if ($test_remote_version_defined == 0) then
		setenv remote_ver $<
	else if ($test_remote_version_defined == -1) then
		setenv remote_ver $test_remote_default_version
		echo $remote_ver
	else
		setenv remote_ver $test_remote_version
		echo $remote_ver
	endif
	if ("$remote_ver" == "") then
		setenv remote_ver $test_remote_default_version
	endif
	echo " "
endif


#########################################################################
#
#	Get the test_remote_image
#
#########################################################################
if ($?test_remote_image == 0) then
	setenv test_remote_image ""
endif
if ("$test_remote_image" == "") then
	set test_remote_image_defined = 0
else if ("$test_remote_image" == "default") then
	set test_remote_image_defined = -1
else
	set test_remote_image_defined = 1
endif
set test_remote_image_def = $image
setenv remote_image $test_remote_image_def
if ($?test_replic == 1) then
	echo " "
	echo -n "Enter Remote Image	"
	echo -n "b, d, or p [$test_remote_image_def]: ------------> "
	if ($test_remote_image_defined == 0) then
		setenv remote_image $<
	else if ($test_remote_image_defined == -1) then
		setenv remote_image $test_remote_image_def
		echo $remote_image
	else
		setenv remote_image $test_remote_image
		echo $remote_image
	endif
	if ( "$remote_image" == "" ) then
		setenv remote_image $test_remote_image_def
	endif
	echo " "
endif



#########################################################################
#
#	Get the test_remote_machine
#
#########################################################################
if ($?test_remote_machine == 0) then
	setenv test_remote_machine ""
endif
if ("$test_remote_machine" == "") then
	set test_remote_machine_defined = 0
else if ("$test_remote_machine" == "default") then
	set test_remote_machine_defined = -1
else
	set test_remote_machine_defined = 1
endif
set test_default_remote_machine = "$test_temp_hostname"
setenv tst_remote_host $test_default_remote_machine
if ($?test_replic == 1) then
	echo " "
	echo -n "Test remote machine name "
	echo -n "[$test_default_remote_machine]:"
	echo -n " ------------> "
	if ($test_remote_machine_defined == 0) then
		setenv tst_remote_host $<
	else if ($test_remote_machine_defined == -1) then
		setenv tst_remote_host $test_default_remote_machine
		echo $tst_remote_host
	else
		setenv tst_remote_host $test_remote_machine
		echo $tst_remote_host
	endif
	if ( "$tst_remote_host" == "" ) then
		setenv tst_remote_host $test_default_remote_machine
	endif
	echo " "
endif



###################################################
setenv tst_org_host $test_temp_hostname
setenv tst_now_primary $tst_org_host
setenv tst_now_secondary $tst_remote_host
unset test_temp_hostname
###################################################




#########################################################################
#
#	Get the test_remote_output_dir
#
#########################################################################
if ($?test_remote_output_dir == 0) then
	setenv test_remote_output_dir ""
endif
if ("$test_remote_output_dir" == "") then
	set test_remote_output_dir_defined = 0
else if ("$test_remote_output_dir" == "default") then
	set test_remote_output_dir_defined = -1
else
	set test_remote_output_dir_defined = 1
endif
set test_remote_default_output_dir = "$tst_dir""/remote"
setenv tst_remote_dir $test_remote_default_output_dir
if ($?test_replic == 1) then
	echo " "
	echo -n "Test remote output dir	"
	echo -n "[$test_remote_default_output_dir]:"
	echo -n " ------------> "
	if ($test_remote_output_dir_defined == 0) then
		setenv tst_remote_dir $<
	else if ($test_remote_output_dir_defined == -1) then
		setenv tst_remote_dir $test_remote_default_output_dir
		echo $tst_remote_dir
	else
		setenv tst_remote_dir $test_remote_output_dir
		echo $tst_remote_dir
	endif
	if ( "$tst_remote_dir" == "" ) then
		setenv tst_remote_dir $test_remote_default_output_dir
	endif
	echo " "
endif



#########################################################################
#
#	Get the test_source_dir
#
#########################################################################

if ("$test_source_dir" == "") then
	set test_source_dir_defined = 0
else if ("$test_source_dir" == "default") then
	set test_source_dir_defined = -1
else
	set test_source_dir_defined = 1
endif

echo " "

set test_default_source_dir = V990

echo -n "Test source dir	"
echo -n "[$test_default_source_dir]:"
echo -n " ------------> "
if ($test_source_dir_defined == 0) then
	setenv tst_src $<
else if ($test_source_dir_defined == -1) then
	setenv tst_src $test_default_source_dir
	echo $tst_src
else
	echo $test_source_dir
	setenv tst_src $test_source_dir
	echo $tst_src
endif
if( "$tst_src" == "" ) then
	setenv tst_src $test_default_source_dir
endif
echo " "

#########################################################################
#
#	Get the test_acc_meth
#
#########################################################################

if ("$test_acc_meth" == "") then
	set test_acc_meth_defined = 0
else if ("$test_acc_meth" == "default") then
	set test_acc_meth_defined = -1
else
	set test_acc_meth_defined = 1
endif

echo " "

set test_default_acc_meth = BG

echo -n "Access Method (MM or BG) [BG]:"
echo -n " ------------> "
if ($test_acc_meth_defined == 0) then
	setenv acc_meth $<
else if ($test_acc_meth_defined == -1) then
	setenv acc_meth $test_default_acc_meth
	echo $acc_meth
else
	setenv acc_meth $test_acc_meth
	echo $acc_meth
endif

if( "$acc_meth" == "" ) then
	setenv acc_meth $test_default_acc_meth
endif
echo " "

#########################################################################
#
#	Get the test_daemon
#
#########################################################################

if ("$test_daemon" == "") then
	set test_daemon_defined = 0
else if ("$test_daemon" == "default") then
	set test_daemon_defined = -1
else
	set test_daemon_defined = 1
endif

echo " "

set test_default_daemon = "N"
setenv havedaemon

if (("MM" == $acc_meth) || ("mm" == $acc_meth)) then
	setenv acc_meth MM
	echo "Using MM"
else
	setenv acc_meth BG
	echo "Using BG"
	echo " "
	echo -n "Daemon [$test_default_daemon]: "
	echo -n " ------------> "
	if ($test_daemon_defined == 0) then
		setenv havedaemon $<
	else if ($test_daemon_defined == -1) then
		setenv havedaemon $test_default_daemon
		echo $havedaemon
	else
		setenv havedaemon $test_daemon
		echo $havedaemon
	endif
endif

if ( "$havedaemon" == "") then
	setenv havedaemon $test_default_daemon
endif

echo " "

#########################################################################
#
#	Get the test_run_time
#
#########################################################################

if ("$test_run_time" == "") then
	set test_run_time_defined = 0
else if ("$test_run_time" == "default") then
	set test_run_time_defined = -1
else
	set test_run_time_defined = 1
endif

echo " "

set test_default_run_time = "now"

echo ""
echo -n 'Test Submit Time (passed to the "at" command if not "now") [now] : ------------> '

if ($test_run_time_defined == 0) then
	setenv test_run_time "$<"
else if ($test_run_time_defined == -1) then
	setenv test_run_time $test_default_run_time
	echo $test_run_time
else
	# setenv test_run_time $test_run_time not required since same variable
	echo $test_run_time
endif

if ("$test_run_time" == "") then
	setenv test_run_time $test_default_run_time
endif

echo ""

#########################################################################
#
#	Get the test_num_runs
#
#########################################################################

if ("$test_num_runs" == "") then
	set test_num_runs_defined = 0
else if ("$test_num_runs" == "default") then
	set test_num_runs_defined = -1
else
	set test_num_runs_defined = 1
endif

echo " "

set test_default_num_runs = "1"

echo -n "Enter Number of Test Runs  "
echo -n "[$test_default_num_runs]: -------------> "

if ($test_num_runs_defined == 0) then
	setenv num_runs $<
else if ($test_num_runs_defined == -1) then
	setenv num_runs $test_default_num_runs
	echo $num_runs
else
	setenv num_runs $test_num_runs
	echo $num_runs
endif

if ("$num_runs" == "") then
	setenv num_runs $test_default_num_runs
endif

setenv test_num_runs $num_runs

echo " "

#######################################################################################################
#
#	define the set of temporary files that you are going to use in the script.
#	also note that you remove these files here and at the end for safety.
#	therefore both the remove codes should be the same except for "submit_script".
#
#######################################################################################################

set TMP_FILE_PREFIX = "/tmp/__${USER}_test_suite_$$_"

set at_temp_file = ${TMP_FILE_PREFIX}_temp_at_file
set month_number_file = ${TMP_FILE_PREFIX}_month_number_file

set include_test_file = ${TMP_FILE_PREFIX}_include_test_file
set include_test_file_temp = ${TMP_FILE_PREFIX}_include_test_file_temp
set include_file = ${TMP_FILE_PREFIX}_include_file
set include_file_name = ${TMP_FILE_PREFIX}_include_file_name
set exclude_test_file = ${TMP_FILE_PREFIX}_exclude_test_file
set exclude_file = ${TMP_FILE_PREFIX}_exclude_file
set exclude_file_name = ${TMP_FILE_PREFIX}_exclude_file_name

rm -f $at_temp_file >& /dev/null
rm -f $month_number_file >& /dev/null

rm -f $include_test_file $include_file $exclude_test_file $exclude_file >& /dev/null
rm -f $include_test_file_temp >& /dev/null
rm -f $include_file_name $exclude_file_name >& /dev/null

#######################################################################################################
#
#      this portion is to get the time when the test is to be run
#      so that the test directory can be appropriately named
#
#######################################################################################################


cat > $month_number_file << MONTHS_EOF
Jan 01
Feb 02
Mar 03
Apr 04
May 05
Jun 06
Jul 07
Aug 08
Sep 09
Oct 10
Nov 11
Dec 12
MONTHS_EOF

#-------------------- sample "at command output" -------------------------------------------------
#
#	mars,sol,sinanju is peculiar it gives a warning message. so that has to be removed
#	sparky has a "commands will be executed using /usr/local/bin/tcsh" message so that has to be taken care of
#	hrothgar says "will be run at" instead of "at"
#	I don't care what p1 says.
#          job nars.889113850.a at Thu Mar  5 11:04:10 1998
#
#-----------------------------------------------------------------------------------------------

if ("$test_run_time" != $test_default_run_time) then	# if it is not "now"
			# because we want to know the desired time and it is easy to know it from the at command
	echo "ls >& /dev/null" | at $test_run_time >& $at_temp_file
	set at_command_output = `cat $at_temp_file | grep -v "arning" | grep -v executed | sed 's/will be run //g'`
	at -r $at_command_output[2]			# remove the at job after it has been used

	setenv test_run_month `cat $month_number_file | grep $at_command_output[5] | awk '{print $2}'`
	setenv test_run_date $at_command_output[6]
	if ($test_run_date < 10) then
		setenv test_run_date "0"$test_run_date
	endif
	setenv test_run_hminsec `echo $at_command_output[7] | sed 's/:/_/g'`
	setenv test_run_year $at_command_output[8]	# this is of the form 1998 what we want is 98 so...
	###setenv test_run_year `echo $test_run_year | sed 's/19//g'` # Did not work in hrothgar? Naryanan added following line
	setenv test_run_year `echo $test_run_year | sed 's/19//g' | sed 's/\.//g'`      # the final sed is for hrothgar's at command
else
	setenv test_run_month `date +%m`
	setenv test_run_date `date +%d`
	setenv test_run_hminsec `date +%H_%M_%S`
	setenv test_run_year `date +%y`
endif

#######################################################################################################
#
#      output the configuration settings assumed by the test script
#      to the terminal and also a file config.log in the test directory
#
#######################################################################################################

version $ver $image
setenv gtm_tst $gtm_test/$tst_src

# Set & create the name of the test directory.
set image_full = `basename $gtm_exe`
setenv gtm_tst_out tst_${ver}_${image_full}_${acc_meth}_${test_run_year}${test_run_month}${test_run_date}_${test_run_hminsec}
mkdir $tst_dir/$gtm_tst_out
echo "Created $tst_dir/$gtm_tst_out"
if ($?test_replic == 1) then
	if ($tst_org_host == $tst_remote_host) then
		mkdir $tst_remote_dir/$gtm_tst_out
		echo "Created $tst_remote_dir/$gtm_tst_out"
	else
		rsh $tst_remote_host -l $tst_remote_user mkdir $tst_remote_dir/$gtm_tst_out
		echo "Created $tst_remote_dir/$gtm_tst_out in $tst_remote_host"
	endif
endif


setenv submit_script ${TMP_FILE_PREFIX}_${gtm_tst_out}_submit_script	# to export to the run-time script so that
									# this can be deleted later by that script
rm -f $submit_script >& /dev/null

#########################################################################
#
#	Get the load_output_directory
#
#########################################################################

set test_default_load_dir = "$tst_dir/$gtm_tst_out/_load"

if ("$test_want_concurrency" == "yes") then
	if ("$test_load_dir" == "") then
		set test_load_dir_defined = 0
	else if ("$test_load_dir" == "default") then
		set test_load_dir_defined = -1
	else
		set test_load_dir_defined = 1
	endif

	echo " "

	echo ""
	echo -n 'System Load Output Directory ["$test_default_load_dir"] : ------------> '

	if ($test_load_dir_defined == 0) then
		setenv test_load_dir "$<"
	else if ($test_load_dir_defined == -1) then
		setenv test_load_dir $test_default_load_dir
		echo $test_load_dir
	else
		echo $test_load_dir
	endif

	if ("$test_load_dir" == "") then
		setenv test_load_dir $test_default_load_dir
	endif

	if ($test_load_dir == $test_default_load_dir  &&  !(-e $test_default_load_dir)) then
		mkdir -p $test_default_load_dir
	endif

	echo " "
endif

#######################################################################################################

echo "Test Version            :: $ver"
echo "Test Image              :: "`basename $gtm_dist`
echo "Test Mail List          :: $mailing_list"
echo "Test Output Directory   :: $tst_dir/$gtm_tst_out"
echo "Test Source Directory   :: $tst_src"
echo -n "Test Access Method      :: $acc_meth"
echo " [Daemon = $havedaemon] "
if ($?test_replic == 1) then
	echo "Test Remote Host	:: $tst_remote_host"
	echo "Test Remote Version	:: $remote_ver"
	echo "Test Remote Image	:: $remote_image"
	echo "Test Remote User	:: $tst_remote_user"
endif
echo " "

set confil = $tst_dir/$gtm_tst_out/config.log
echo " " > $confil
echo "OUTPUT CONFIGURATION" >> $confil
echo " " >> $confil
echo "PRODUCT:	GT.M" >> $confil
echo "VERSION:	`basename $gtm_ver`" >> $confil
echo "IMAGE:		`basename $gtm_dist`" >> $confil
echo "ACCESS METHOD:	$acc_meth" >> $confil
echo "TEST SOURCE:	$gtm_tst" >> $confil
echo "HOST:		`basename $HOST .sanchez.com`" >> $confil
echo "HOSTOS:		$HOSTOS" >> $confil
echo "USER:		$USER" >> $confil
if ($?test_replic == 1) then
	echo "REMOTE HOST	$tst_remote_host" >> $confil
	echo "REMOTE VERSION	$remote_ver" >> $confil
	echo "REMOTE IMAGE	$remote_image" >> $confil
	echo "REMOTE USER	$tst_remote_user" >> $confil
endif
echo " "	>> $confil

if ("$test_want_concurrency" == "yes"  &&  !(-e $test_default_load_dir/loadinp.m)) then
	cp $gtm_tst/com/{clear,loadinp,load,unload,getnear}.m $test_default_load_dir

	if (!(-e $test_load_dir/load.dat)) then
		pushd $test_load_dir
		setenv gtmgbldir load.gld
		if (!(-e load.gld)) then
			$gtm_exe/mumps -run GDE << GDEEOF
				ch -s DEFAULT -file=load.dat
				exit
GDEEOF
				# excuse the alignment of the EOFs, this is because of limitations in end-marker placement.
		endif
		$gtm_exe/mupip create
		$gtm_exe/mumps -direct <<GTMEOF
			d ^loadinp
			h
GTMEOF
		popd
	endif
endif

#######################################################################################################
#
#	Get the set of tests to be included and/or excluded
#	Get the env. variables : test_include and test_exclude
#
#######################################################################################################

if ("$test_include" == "") then
	set test_include_defined = 0
else if ("$test_include" == "default") then
	set test_include_defined = -1
else
	set test_include_defined = 1
endif

set test_default_include = "all"

if ("$test_exclude" == "") then
	set test_exclude_defined = 0
else if ("test_exclude" == "default") then
	set test_exclude_defined = -1
else
	set test_exclude_defined = 1
endif

set test_default_exclude = "none"

if ($?test_replic == 1) then
	# Include DSE later
	setenv common_tests "basic compil locks mpt mugj speed v230 v234 v320 mupip jnl tp resil tpresil overflow split_recov lost_trans switch_over src_fail_rs src_crash src_crash_fo rcvr_fail_rs rcvr_crash_rs upd_fail_rs dual_fail1 dual_fail2 dual_fail3 dual_fail4 dual_fail4a dual_fail5 dual_fail6  onlnbkup_src_a onlnbkup_src_b onlnbkup_rcvr burst_load mdivistp online zqgblmod"
else
	setenv common_tests "basic compil dse gde jnl locks mpt met mugj mupip read_only speed tcpip tp v230 v234 v320 online_bkup resil tpresil zsearch"
endif

if ( $acc_meth == "MM" ) then
	setenv common_tests_plus_acc "$common_tests mm"
else
	setenv common_tests_plus_acc "$common_tests"
endif

switch ($HOSTOS)
	case "AIX":
	case "HP-UX":
	case "OSF1":
	case "OS/390":
		if ($?test_replic == 1) then
			setenv full_tst_suite "$common_tests_plus_acc"
		else
			setenv full_tst_suite "$common_tests_plus_acc xcall"
		endif
		breaksw

	case "SunOS":
		setenv full_tst_suite "$common_tests_plus_acc"
		breaksw

	default:
		setenv full_tst_suite "$common_tests_plus_acc"
		breaksw
endsw
#
#

if ($test_include_defined == 0) then
	echo "Your Full Test Suite Consists of"
	echo ""
endif
set suite_no_echo = " 1"
set suite_no = 1
foreach suite ($full_tst_suite)
	if ($test_include_defined == 0) then
		echo "	$suite_no_echo) $suite"
	endif
	setenv suite_${suite_no} $suite
	set suite_no = `echo "$suite_no+1" | bc`
	if ($suite_no < 10) then
	    set suite_no_echo = " $suite_no"
	else
	    set suite_no_echo = $suite_no
	endif
end

setenv include_test ""
setenv exclude_test ""

echo ""
echo -n "Submit Tests (e.g. 1:4+10:16-13:14) [All] : ------------> "
if ($test_include_defined == 0) then
	setenv include_test "$<"
	set isalpha = `echo $include_test | sed 's/[0-9:+]//g' | sed 's/-//g'`
	if ("$isalpha" != "") then
		set test_include_defined = 1	# make it as if user set $test_include
	endif
else if ($test_include_defined == -1) then
	setenv include_test $test_default_include
	echo $include_test
else
	setenv include_test "$test_include"
	echo $include_test
endif

if ("$include_test" == "") then
	setenv include_test $test_default_include
endif

#######################################################################################################
#
#	Get the set of tests to be run in terms of ranges of their test numbers
#
#######################################################################################################

if ("$include_test" == $test_default_include) then
	echo "+ 1 $suite_no" >! $include_test_file
	touch $exclude_test_file
else if ($test_include_defined == 0) then	# if got from user input only
	echo $include_test | sed 's/-/\\
- /g' | sed 's/:/ /g' | sed 's/+/\\
+ /g' >! $include_test_file_temp
	cat $include_test_file_temp | grep "\-" >! $exclude_test_file
	cat $include_test_file_temp | grep -v "\-" | awk '{if ($1 != "+") printf "+ %s\n",$0; else print $0;}' >! $include_test_file
endif

#######################################################################################################
#
#	Get the list of numbers from the above range file
#
#######################################################################################################

if ("$include_test" == $test_default_include || $test_include_defined == 0) then
	cat $include_test_file | awk '{if ($3 == NULL) end = $2; else end = $3; for (i = $2; i <= end; i++) print i;}' >! $include_file
	cat $exclude_test_file | awk '{if ($3 == NULL) end = $2; else end = $3; for (i = $2; i <= end; i++) print i;}' >! $exclude_file
endif

#######################################################################################################
#
#	Map the list of numbers to the corresponding test names
#
#######################################################################################################

if ("$include_test" != $test_default_include && $test_include_defined != 0) then
	echo $include_test | sed 's/[, 	][, 	]*/\\
/g' >! $include_file_name
	touch $exclude_file_name	# empty file if you have specified a specific test to be included
else
	cat $include_file | awk '$1 < '$suite_no' && $1 > 0 {printf "echo $suite_%s\n", $1}' | $shell -f >! $include_file_name
	if ($test_exclude_defined != 0 && $test_exclude_defined != -1) then
		echo $test_exclude | sed 's/[, 	][, 	]*/\\
/g' >! $exclude_file_name
	else
		cat $exclude_file | awk '$1 < '$suite_no' && $1 > 0 {printf "echo $suite_%s\n", $1}' | $shell -f | sed 's/^/\^/g' | sed 's/$/\$/g' >! $exclude_file_name
	endif
endif

set exclude_file_name_list = `cat $exclude_file_name`
if ("$exclude_file_name_list" == "") then
	setenv tst_suite `cat $include_file_name`
else
	setenv tst_suite `egrep -f $exclude_file_name -v $include_file_name`
endif

echo ""
echo "Your Choice of Tests is as Follows "
echo " $tst_suite" | sed 's/ /\\
	/g'
echo ""

if ("$tst_suite" != "") then
	if ("$test_run_time" != $test_default_run_time) then
		env | sed -n '/^ver/,$p' | sed 's/=/ "/g' | sed 's/$/"/g' | grep -v '^suite_' | sed 's/^/setenv /g' >! $submit_script
		echo '$shell -f $gtm_tst/com/submit_test.csh &' >>! $submit_script
		echo 'rm -f $submit_script >& /dev/null' >>! $submit_script
		echo "$shell -f $submit_script >& /dev/null" | at $test_run_time
	else
		if ( $?test_no_background != 0) then
			nice -20 $shell -f $gtm_tst/com/submit_test.csh
		else
			nice -20 $shell -f $gtm_tst/com/submit_test.csh &
		endif
	endif
else
	echo "No Tests were submitted"
endif
echo ""

##################################################################################################
#
#       remove all temporary files. this portion should be the same as the
#       as the portion above where the temporary file names are defined.
#
##################################################################################################

rm -f $at_temp_file >& /dev/null
rm -f $month_number_file >& /dev/null

rm -f $include_test_file $include_file $exclude_test_file $exclude_file >& /dev/null
rm -f $include_test_file_temp >& /dev/null
rm -f $include_file_name $exclude_file_name >& /dev/null

#rm -f $submit_script >& /dev/null

