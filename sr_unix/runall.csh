#!/usr/local/bin/tcsh -f
#################################################################
#								#
#	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################


if ($?RUNALL_DEBUG != 0) then
	set verbose
	set echo
endif

echo ""

set listonly = 0
set compileonly = 0
set linkonly = 0
set helponly = 0

set temp=(`getopt nchl $argv:q`)
if ($? == 0) then
	eval set argv=\($temp:q\)
	while (1)
		switch ($1:q)
			case -n :
				set listonly = 1
				shift
				breaksw

			case -c :
				set compileonly = 1
				shift
				breaksw

			case -l :
				set linkonly = 1
				shift
				breaksw

			case -h :
				set helponly = 1
				shift
				breaksw

			case -- :
				shift
				break
		endsw
	end
else
	set helponly = 1
endif

if ($helponly) then
	echo "Usage : `basename $0` [-n|-c|-h] [file...]"
	echo " -n	List the out-of-date sources; don't compile, or build"
	echo " -c	Compile the out-of-date sources; don't build"
	echo " -h	Print this message"
	echo ""
	exit 1
endif

source $gtm_root/$gtm_verno/tools/gtm_cshrc.csh

\unalias unalias
unalias cd chown rm touch echo grep sed awk sort cat mv ls

if ($?RUNALL_VERSION == 0) then
	setenv RUNALL_VERSION
endif

if ($?RUNALL_IMAGE == 0) then
	setenv RUNALL_IMAGE
endif

if ("$RUNALL_VERSION" == "") then
	setenv RUNALL_VERSION $gtm_verno
endif

if ("$RUNALL_IMAGE" == "") then
	setenv RUNALL_IMAGE $gtm_exe:t
endif

if ($?RUNALL_EXTRA_CC_FLAGS == 0) then
	setenv RUNALL_EXTRA_CC_FLAGS ""
endif

if ($?RUNALL_EXTRA_AS_FLAGS == 0) then
	setenv RUNALL_EXTRA_AS_FLAGS ""
endif

if ($RUNALL_IMAGE == "d") then
	setenv RUNALL_IMAGE "dbg"
else if ($RUNALL_IMAGE == "p") then
	setenv RUNALL_IMAGE "pro"
else if ($RUNALL_IMAGE == "b") then
	setenv RUNALL_IMAGE "bta"
endif

cd $gtm_root/$RUNALL_VERSION/${RUNALL_IMAGE}/obj

echo ""
echo "Start of $gtm_tools/runall.csh"
echo ""
echo -n "   -->  "
date
echo ""
echo "Input Command Line"
echo ""
echo -n "   -->  "
echo "[ $0 "$argv" ]"
echo ""
echo "Environment Variables"
echo ""
echo "     RUNALL_VERSION         ---->  [ $RUNALL_VERSION ]"
echo "     RUNALL_IMAGE           ---->  [ $RUNALL_IMAGE ]"
echo "     RUNALL_EXTRA_CC_FLAGS  ---->  [ $RUNALL_EXTRA_CC_FLAGS ]"
echo "     RUNALL_EXTRA_AS_FLAGS  ---->  [ $RUNALL_EXTRA_AS_FLAGS ]"
echo ""

if (`uname` == "SunOS") then
	set path = (/usr/xpg4/bin $path)
endif

set user=`id -u -n`

version $RUNALL_VERSION $RUNALL_IMAGE

rm -f $gtm_log/error.$RUNALL_IMAGE.log >& /dev/null

set TMP_DIR_PREFIX = "/tmp/__${user}__runall"
set TMP_DIR = "${TMP_DIR_PREFIX}__`date +"%y%m%d_%H_%M_%S"`_$$"
rm -f ${TMP_DIR}_* >& /dev/null

onintr cleanup

if ($?RUNALL_BYPASS_VERSION_CHECK == 0) then
	set temp_ver=`echo $gtm_verno | sed 's/./& /g'`
	if ($temp_ver[2] != "9" || $temp_ver[3] == "9" && $temp_ver[4] == "0") then
		echo ""
		echo "-----------------------------------------------------------------------------------"
		echo "RUNALL-E-WRONGVERSION : Cannot Runall a Non-Developemental Version  ---->   $gtm_verno"
		echo "-----------------------------------------------------------------------------------"
		echo ""
		goto cleanup
	endif
endif

# ---- currently, dtgbldir isn't built into an executable. So a "dummy" executable is assigned to it.

cat - << LABEL >>! ${TMP_DIR}_exclude
dse dse
dse_cmd dse
dtgbldir dummy
mupip mupip
mupip_cmd mupip
lke lke
lke_cmd lke
geteuid geteuid
gtm mumps
gtm_svc gtm_svc
gtm_dal_svc gtm_svc
gtm_rpc_init gtm_svc
daemon gtm_dmna
gtmsecshr gtmsecshr
mumps_clitab mumps
gtm_main mumps
semstat2 semstat2
ftok ftok
gtcm_main gtcm_server
omi_srvc_xct gtcm_server
gtcm_play gtcm_play
omi_sx_play gtcm_play
gtcm_shmclean gtcm_shmclean
gtcm_pkdisp gtcm_pkdisp
gtcm_gnp_server gtcm_gnp_server
gtcm_gnp_clitab gtcm_gnp_server
LABEL

# find all libnames other than mumps
pushd $gtm_tools >& /dev/null
set comlist_liblist = `ls *.list | sed 's/.list//' | sed 's/^lib//'`
popd >& /dev/null

foreach value ($comlist_liblist)
	cat $gtm_tools/lib$value.list | awk '{print "lib'${value}'.a", $1, " "}' >>! ${TMP_DIR}_list
end

rm -f ${TMP_DIR}_main_misc >& /dev/null
touch ${TMP_DIR}_main_.misc

rm -f $gtm_inc/__temp_runall_* >& /dev/null
rm -f $gtm_src/__temp_runall_* >& /dev/null
rm -f ${TMP_DIR}_inc_files >& /dev/null
rm -f ${TMP_DIR}_src_files >& /dev/null

touch ${TMP_DIR}_inc_files
touch ${TMP_DIR}_src_files

if ($#argv) then
	while ($#argv > 0)
		set ext = $1:e
		if ($ext == "h"  ||  $ext == "si") then
			echo $1 >>&! ${TMP_DIR}_inc_files
		else if ("$ext" == "c" || "$ext" == "s" || "$ext" == "msg" || "$ext" == "") then
			echo $1 >>&! ${TMP_DIR}_src_files
		endif
		shift
	end
else
	echo "...... Searching for --------> Last Built Executable in $gtm_exe ...... "
	pushd $gtm_exe >& /dev/null
	set latest_exe = `ls -lart | grep "\-..x..x..x" | tail -1 | awk '{print $NF}'`
	popd >& /dev/null
	if ("$latest_exe" != "") then
		echo "...... Searching for --------> out-of-date INCLUDEs ...... "
		pushd $gtm_inc >& /dev/null
		ln -s $gtm_exe/${latest_exe} __temp_runall_${latest_exe}
		ls -1Lat | sed -n '1,/__temp_runall_'${latest_exe}'/p' | grep -E '(\.h$|\.si$)' >>&! ${TMP_DIR}_inc_files
		rm -f __temp_runall_${latest_exe} >& /dev/null
		popd >& /dev/null
		if (! -z ${TMP_DIR}_inc_files) then
			sort -u ${TMP_DIR}_inc_files >&! ${TMP_DIR}_inc_files_sorted
			mv ${TMP_DIR}_inc_files_sorted ${TMP_DIR}_inc_files
		endif
		echo "...... Searching for --------> out-of-date SOURCEs ...... "
		pushd $gtm_src >& /dev/null
		ln -s $gtm_exe/${latest_exe} __temp_runall_${latest_exe}
		ls -1Lat | sed -n '1,/__temp_runall_'${latest_exe}'/p' | grep -E '(\.c$|\.msg$|\.s$)' >>&! ${TMP_DIR}_src_files
		rm -f __temp_runall_${latest_exe} >& /dev/null
		popd >& /dev/null
	else
		echo ""
		echo "Unable to find the last modified executable in $gtm_exe. ....... Exiting"
		echo ""
		goto cleanup
	endif
endif

set backslash_quote

if (!(-z ${TMP_DIR}_inc_files)) then
	echo "...... Searching for --------> out-of-date Nested HEADER Files ...... "
	pushd $gtm_inc >& /dev/null
	rm -f ${TMP_DIR}_inc_file_temp >& /dev/null
	touch ${TMP_DIR}_inc_file_temp
	while (1)
		diff ${TMP_DIR}_inc_file_temp ${TMP_DIR}_inc_files | grep "^>" | sed 's/> //' > ${TMP_DIR}_inc_file_diff
		if (! -z ${TMP_DIR}_inc_file_diff) then
			cp -f ${TMP_DIR}_inc_files ${TMP_DIR}_inc_file_temp
			foreach value (`cat ${TMP_DIR}_inc_file_diff`)
				ls -1 | grep -E '(\.h$|\.si$)' | \
					xargs grep -l "^[ 	]*#[ 	]*include[ 	]*\"$value\"" >>&! ${TMP_DIR}_inc_files
			end
			sort -u ${TMP_DIR}_inc_files >&! ${TMP_DIR}_inc_files_sorted
			mv ${TMP_DIR}_inc_files_sorted ${TMP_DIR}_inc_files
		else
			break
		endif
	end
	popd >& /dev/null
endif

if (!(-z ${TMP_DIR}_inc_files)) then
	echo "...... Searching for --------> #INCLUDEing Source Files ...... "
	pushd $gtm_src >& /dev/null
	foreach value (`cat ${TMP_DIR}_inc_files`)
		ls -1 | grep -E '(\.c$|\.msg$|\.s$)' | \
			xargs grep -l "^[ 	]*#[ 	]*include[ 	]*\"$value\"" >>&! ${TMP_DIR}_src_files
	end
	popd >& /dev/null
endif

if (!(-z ${TMP_DIR}_src_files)) then
	sort -u ${TMP_DIR}_src_files >&! ${TMP_DIR}_src_files_sorted
	mv ${TMP_DIR}_src_files_sorted ${TMP_DIR}_src_files
else
	echo ""
	echo " -- Error: Nothing to Compile ............ Exiting"
	echo ""
	goto cleanup
endif

if ($listonly) then
	if ($?latest_exe) then
		echo ""
		echo "************** Last built executable ************** "
		echo ""
		echo $latest_exe
	endif
	echo ""
	echo "************** Out of date include files ********** "
	echo ""
	cat ${TMP_DIR}_inc_files
	echo ""
	echo "************** Out of date source files *********** "
	echo ""
	cat ${TMP_DIR}_src_files
	echo ""
	goto cleanup
endif

echo ""
echo "****************************** COMPILING *********************************"
echo ""


foreach file (`cat ${TMP_DIR}_src_files`)
	set file = $file:t
	set ext = $file:e
	if ("$ext" == "") then
		set ext = "c"
	endif
	set file = $file:r		# take the non-extension part for the obj file
	set objfile = ${file}.o

	alias runall_cc gt_cc_${RUNALL_IMAGE}
	alias gt_as $gt_as_assembler $gt_as_options_common $gt_as_option_I $RUNALL_EXTRA_AS_FLAGS
	alias runall_as gt_as_${RUNALL_IMAGE}

	if ($linkonly == 0) then
		if ($ext == "s") then
			echo "$gtm_src/$file.$ext   ---->  $gtm_obj/$file.o"
			runall_as $gtm_src/$file.s
		else if ($ext == "c") then
			echo "$gtm_src/$file.$ext   ---->  $gtm_obj/$file.o"
			runall_cc $RUNALL_EXTRA_CC_FLAGS $gtm_src/$file.c
			if ($file == "omi_srvc_xct") then
				chmod +w $gtm_src/omi_sx_play.c
				\cp $gtm_src/omi_srvc_xct.c $gtm_src/omi_sx_play.c
				chmod -w $gtm_src/omi_sx_play.c
				echo "$gtm_src/omi_sx_play.c   ---->  $gtm_obj/omi_sx_play.o"
				runall_cc -DFILE_TCP $RUNALL_EXTRA_CC_FLAGS $gtm_src/omi_sx_play.c
			endif
		else if ($ext == "msg") then
			echo "$gtm_src/$file.$ext   ---->  $gtm_obj/${file}_ctl.c  ---->  $gtm_obj/${file}_ctl.o"
			# gtm_startup_chk requires gtm_dist setup
			rm -f ${file}_ctl.c ${file}_ansi.h	# in case an old version is lying around
			set real_gtm_dist = "$gtm_dist"
			setenv gtm_dist "$gtm_root/$gtm_curpro/pro"
			setenv gtmroutines "$gtm_obj($gtm_pct)"
			$gtm_root/$gtm_curpro/pro/mumps -run msg $gtm_src/$file.msg Unix
			setenv gtm_dist "$real_gtm_dist"
			unset real_gtm_dist
			mv ${file}_ctl.c $gtm_src/${file}_ctl.c
			if ( -f ${file}_ansi.h ) then
				mv -f ${file}_ansi.h $gtm_inc
			endif
			runall_cc $RUNALL_EXTRA_CC_FLAGS $gtm_src/${file}_ctl.c
			set objfile = ${file}_ctl.o
		endif
		if ($status == 1) then
			goto cleanup
		endif
	endif
	set library=`grep "^$file " ${TMP_DIR}_exclude`
	if ("$library" == "") then
		set library=`grep " $file " ${TMP_DIR}_list | awk '{print $1}'`
		if ("$library" == "") then
			set library="libmumps.a"
		else
			set retain=`grep "^$file" $gtm_tools/retain_list.txt`
			if ("$retain" != "") then
				echo $objfile >> ${TMP_DIR}_lib_.libmumps.a
			endif
		endif
		echo $objfile >> ${TMP_DIR}_lib_.$library
	else
		if ("$library[1]" != "$library") then
			echo $library[2] >> ${TMP_DIR}_main_.misc
			if ($file == "omi_srvc_xct.c") then
				echo "gtcm_play" >> ${TMP_DIR}_main_.misc
			endif
		endif
	endif
end

if ($compileonly) then
	goto cleanup
endif

echo ""
echo "-------------------------------- ARCHIVING --------------------------------"
echo ""

(ls -1 ${TMP_DIR}_lib_.* > ${TMP_DIR}_Lib_list) >& /dev/null

foreach lib_ (`cat ${TMP_DIR}_Lib_list`)
	set libext = $lib_:e
	set library = $lib_:r
	set library = $library:e
	set library = $library.$libext
	echo "-->  into $gtm_obj/$library <--"
	gt_ar $gt_ar_option_update $gtm_obj/$library `cat $lib_`
	if ($status) then
		if ($?RUNALL_DEBUG != 0) env
		goto cleanup
	endif
	rm -f `cat $lib_`
	echo ""
end
echo ""

echo "----------------------------------------------------------"
echo "      BUILDing the executables for the $RUNALL_IMAGE version"
echo "----------------------------------------------------------"
echo

cat ${TMP_DIR}_Lib_list ${TMP_DIR}_main_.misc | sed 's/.*lib//g' | sed 's/\.a$//g' | sort -u >! ${TMP_DIR}_main_.final
set mumps_changed = `grep libmumps.a ${TMP_DIR}_Lib_list`

# On OS390, check if libascii has been rebuilt. If $gt_ar_ascii_name
# has the value "ascii", then the null definition in gtm_env.csh was
# overridden in gtm_env_sp.csh (OS390 only), so set ascii_changed
# accordingly. OS390 adds "-lascii" to $gt_ld_syslibs so it gets included
# when linking in buildaux.csh and buildshr.csh

if ("$gt_ar_ascii_name" == "ascii") then
	set ascii_changed = `grep libascii.a ${TMP_DIR}_Lib_list`
else
	set ascii_changed = ""
endif

if ("$mumps_changed" != "" || "$ascii_changed" != "") then
	$shell $gtm_tools/build${RUNALL_IMAGE}.csh $RUNALL_VERSION
else
	set build_routine = `cat ${TMP_DIR}_main_.final`
	$shell $gtm_tools/buildaux.csh $RUNALL_VERSION $RUNALL_IMAGE $gtm_root/$RUNALL_VERSION/$RUNALL_IMAGE $build_routine
endif

echo ""
if ( -f $gtm_log/error.${RUNALL_IMAGE}.log ) then
	echo "Error summary:"
	echo "--------------"
	echo ""
	cat $gtm_log/error.${RUNALL_IMAGE}.log
	echo ""
	echo " ---->  Leaving runall logfiles ${TMP_DIR}_* intact. Please remove after troubleshooting."
	echo ""
	goto done
endif

cleanup:
if ($?latest_exe) then
	rm -f $gtm_src/__temp_runall_${latest_exe} $gtm_inc/__temp_runall_${latest_exe} >& /dev/null
endif
if ($?TMP_DIR_PREFIX) then
	rm -f ${TMP_DIR_PREFIX}* >& /dev/null
endif

done:
echo ""
echo "End of $gtm_tools/runall.csh"
echo ""
echo -n "   --  "
date
echo ""

