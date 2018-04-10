#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2001-2018 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	Get rid of debug options producing huge amounts of output if set
unsetenv gtmdbglvl

# Unconditionally switch to C locale. UTF-8 locales have shown issues in various stages of runall.
unsetenv LC_ALL
setenv LC_CTYPE C
setenv LC_COLLATE C

if ($?RUNALL_DEBUG != 0) then
	set verbose
	set echo
endif

echo ""

set runall_status = 0
set listonly = 0
set compileonly = 0
set linkonly = 0
set helponly = 0
set mumps_changed = ""
set gtmplatformlib_changed = ""

while (0 < $#)
	switch ($1:q)
		case -n :
			set listonly = 1
			breaksw
		case -c :
			set compileonly = 1
			breaksw
		case -l :
			set linkonly = 1
			breaksw
		case -h :
			set helponly = 1
			breaksw
		default :
			break
	endsw
	shift
end

if ($helponly) then
	echo "Usage : `basename $0` [-n|-c|-h] [file...]"
	echo " -n	List the out-of-date sources; don't compile, or build"
	echo " -c	Compile the out-of-date sources; don't build"
	echo " -l	Link the out-of-date sources; don't compile"
	echo " -h	Print this message"
	echo ""
	exit 1
endif

source $gtm_root/$gtm_verno/tools/gtm_cshrc.csh

\unalias unalias >& /dev/null  # TCSH does not let you alias unalias
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

version $RUNALL_VERSION $RUNALL_IMAGE

if ($?RUNALL_BYPASS_VERSION_CHECK == 0) then
	if ($gtm_verno =~ V[4-8]* || $gtm_verno == "V990" ) then
		echo ""
		echo "-----------------------------------------------------------------------------------"
		echo "RUNALL-E-WRONGVERSION : Cannot Runall a Non-Developemental Version  ---->   $gtm_verno"
		echo "-----------------------------------------------------------------------------------"
		echo ""
		@ runall_status = 1	# to signal that the runall failed to do its job
		goto cleanup
	endif
endif

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
echo "     gtmroutines            ---->  [ $gtmroutines ]"
echo ""

if (`uname` == "SunOS") then
	set path = (/usr/xpg4/bin $path)
endif

set user=`id -u -n`


rm -f $gtm_log/error.$RUNALL_IMAGE.log >& /dev/null

set TMP_DIR_PREFIX = "/tmp/__${user}__runall"
setenv TMP_DIR "${TMP_DIR_PREFIX}__`date +"%y%m%d_%H_%M_%S"`_$$" # needed by runall_cc_many.csh/runall_cc_one.csh hence "setenv"
rm -f ${TMP_DIR}_* >& /dev/null

onintr cleanup

set platform_name = `uname | sed 's/-//g' | sed 's,/,,' | tr '[A-Z]' '[a-z]'`
set mach_type = `uname -m`

cat - << LABEL >>! ${TMP_DIR}_exclude
dse dse
dse_cmd dse
mupip mupip
mupip_cmd mupip
lke lke
lke_cmd lke
geteuid geteuid
gtm mumps
gtmsecshr gtmsecshr
gtmsecshr_wrapper gtmsecshr
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
dbcertify dbcertify
dbcertify_cmd dbcertify
gtmcrypt_dbk_ref gtmcrypt
gtmcrypt_pk_ref gtmcrypt
gtmcrypt_sym_ref gtmcrypt
gtmcrypt_ref gtmcrypt
gtmcrypt_util gtmcrypt
maskpass gtmcrypt
gtm_tls_impl gtmcrypt
LABEL

# this first set of excluded modules are from the list above of modules that get built as plugins. Only plugins
# should be metioned in this list.
set exclude_compile_list = (gtmcrypt_dbk_ref gtmcrypt_sym_ref gtmcrypt_pk_ref gtmcrypt_ref maskpass gtm_tls_impl)
set exclude_compile_list = ($exclude_compile_list gtmcrypt_util)
# modules that should never be built or compiled are in this list. They are used in other capacities (e.g. used to
# generate other routines) but are NOT part of the GTM runtime. Other scripts compile and use these routines.
set exclude_build_list = "gtm_threadgbl_deftypes"
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
rm -f ${TMP_DIR}_latest_exe
touch ${TMP_DIR}_inc_files
touch ${TMP_DIR}_src_files

	echo "Modifying release_name.h"
	$cms_tools/edrelnam.csh $RUNALL_VERSION

# For all builds, gtm_threadgbl_deftypes.h needs to be generated unless we are bypassing.
# This only updates gtm_threadgbl_deftypes.h in $gtm_inc if generation shows it changed.
# Note shortened variable to RUNALL_BYPASS_GEN_THREADGBL due to failure on Tru64 (too long var name).
if ($?RUNALL_BYPASS_GEN_THREADGBL == 0) then
	tcsh $gtm_tools/gen_gtm_threadgbl_deftypes.csh
	if (0 != $status) @ runall_status = $status
	if (0 != $runall_status) then
		echo "Failed to build gtm_threadgbl_deftypes.h - aborting build"
		exit $runall_status
	endif
	# Setup link from $gtm_obj to the proper assembler include file
	if (! -e ${gtm_obj}/gtm_threadgbl_deftypes_asm.si) then
	    set asmtgbltype = $gtm_exe:t
	    \ln -s ${gtm_inc}/gtm_threadgbl_deftypes_asm_${asmtgbltype}.si ${gtm_obj}/gtm_threadgbl_deftypes_asm.si
	endif
endif

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
	# Look down the $gtm_exe recursively so that the search for recent build executable is inclusive of $gtm_exe/plugin/ and
	# $gtm_exe/plugin/gtmcrypt. Also the new shell invocation is needed to redirect the stderr that will arise due to the
	# gtmsecshr privileges for which (find .) will issue 'permission denied'.
	# Look for files with execute permissions (-111) and ignore help database (! -name "*.dat")
	(ls -lart `find . -type f -perm -111 ! -name "*.dat"` | tail -n 1 | awk '{print $NF}' > ${TMP_DIR}_latest_exe) >&! /dev/null
	set latest_exe_with_rel_path = `cat ${TMP_DIR}_latest_exe`
	set latest_exe = `basename $latest_exe_with_rel_path`
	rm -f ${TMP_DIR}_latest_exe
	popd >& /dev/null
	if ("$latest_exe" != "") then
		echo "...... Searching for --------> out-of-date INCLUDEs ...... "
		pushd $gtm_inc >& /dev/null
		ln -s $gtm_exe/${latest_exe_with_rel_path} __temp_runall_${latest_exe}
		ls -1Lat | sed -n '1,/__temp_runall_'${latest_exe}'/p' | grep -E '(\.h$|\.si$)' >>&! ${TMP_DIR}_inc_files
		rm -f __temp_runall_${latest_exe} >& /dev/null
		popd >& /dev/null
		echo "...... Searching for --------> out-of-date SOURCEs ...... "
		pushd $gtm_src >& /dev/null
		ln -s $gtm_exe/${latest_exe_with_rel_path} __temp_runall_${latest_exe}
		ls -1Lat | sed -n '1,/__temp_runall_'${latest_exe}'/p' | grep -E '(\.c$|\.msg$|\.s$)' >>&! ${TMP_DIR}_src_files
		rm -f __temp_runall_${latest_exe} >& /dev/null
		popd >& /dev/null
		echo "...... Searching for --------> out-of-date PCT ROUTINEs ...... "
		pushd $gtm_pct >& /dev/null
		ln -s $gtm_exe/${latest_exe_with_rel_path} __temp_runall_${latest_exe}
		ls -1Lat | sed -n '1,/__temp_runall_'${latest_exe}'/p' | grep -E '\.(m|hlp)$' >>&! ${TMP_DIR}_pct_files
		rm -f __temp_runall_${latest_exe} >& /dev/null
		popd >& /dev/null
	else
		echo ""
		echo "Unable to find the last modified executable in $gtm_exe. ....... Exiting"
		echo ""
		@ runall_status = 1	# to signal that the runall failed to do its job
		goto cleanup
	endif
endif

###### if *.msg files got included, then also include *_ansi.h files for recompiling #######
if (!(-z ${TMP_DIR}_src_files)) then
	foreach file (`cat ${TMP_DIR}_src_files`)
		set ext = $file:e
		if ("$ext" == "msg") then
			set file=$file:t:r
			echo ${file}_ansi.h >>&! ${TMP_DIR}_inc_files
		endif
	end
endif

if (! -z ${TMP_DIR}_inc_files) then
	sort -u ${TMP_DIR}_inc_files >&! ${TMP_DIR}_inc_files_sorted
	mv ${TMP_DIR}_inc_files_sorted ${TMP_DIR}_inc_files
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

# For ia64 & x86_64, the file - xfer_desc.i - needs to be generated.
if ($?RUNALL_BYPASS_GEN_XFER_DESC == 0) then
	if ( "ia64" == $mach_type || "x86_64" == $mach_type ) then
		pushd $gtm_src
		tcsh $gtm_tools/gen_xfer_desc.csh
		popd
	endif
endif

#  Generate ttt.c
if (!(-e $gtm_src/ttt.c) || ((-M $gtm_tools/ttt.txt) > (-M $gtm_src/ttt.c))) then
	if (-x $gtm_root/$gtm_curpro/pro/mumps) then
		pushd $gtm_src >& /dev/null
		/usr/local/bin/tcsh $gtm_tools/gen_ttt.csh
		echo ttt.c >>&! ${TMP_DIR}_src_files
		popd >& /dev/null
	else
		echo "RUNALL-E-NoMUMPS, unable to regenerate ttt.c due to missing $gtm_curpro/pro/mumps" \
			>> $gtm_log/error.$RUNALL_IMAGE.log
	endif
endif

# Run the list of files we are supposed to compile and delete the ones we never want to build
if (-e ${TMP_DIR}_src_files) then
    rm -f ${TMP_DIR}_src_files_tmp
    foreach value ($exclude_build_list)
	sed "/$value/d" ${TMP_DIR}_src_files > ${TMP_DIR}_src_files_tmp
	mv ${TMP_DIR}_src_files_tmp ${TMP_DIR}_src_files
    end
endif

if (!(-z ${TMP_DIR}_src_files)) then
	sort -u ${TMP_DIR}_src_files >&! ${TMP_DIR}_src_files_sorted
	mv ${TMP_DIR}_src_files_sorted ${TMP_DIR}_src_files
else if (-z ${TMP_DIR}_pct_files) then
	echo ""
	echo " -- Nothing to Compile ............ Exiting"
	echo ""
	@ runall_status = 0	# runall's job is done even though there is nothing to compile
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
	echo "************** Out of date pct files *********** "
	echo ""
	cat ${TMP_DIR}_pct_files
	echo ""
	@ runall_status = 0
	goto cleanup
endif

# Irrespective of the gtm_chset value from the user environment, all
# M objects generated in $gtm_dist (GDE*.o, _*.o, ttt.o) must be
# compiled with gtm_chset="M".
unsetenv gtm_chset

echo ""
echo "****************************** COMPILING *********************************"
echo ""

set exclude_compile_list_modified = "FALSE"
if (! -z ${TMP_DIR}_src_files) then
	set filelist = `cat ${TMP_DIR}_src_files`
	set newfilelist = ""
	foreach file ( $filelist )
		set newfile = $file:t
		# Do not compile plugin files if they are modified. Compilation and subsequent build will happen in
		# buildaux.csh.
		set skip_file_compile = 0
		set tmplist = ($exclude_compile_list)
		set -f tmplist = ($tmplist $newfile:r)
		if ($#tmplist == $#exclude_compile_list) then
			# $newfile is part of $exclude_compile_list
			set exclude_compile_list_modified = "TRUE"
		else
			set newfilelist = "$newfilelist $file"
		endif
	end
	echo $newfilelist | xargs -n25 $gtm_tools/runall_cc_many.csh $linkonly
	@ runall_status = $status
	if ($compileonly || (0 != $runall_status)) then
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
		set dstlib = "$gtm_obj/$library"
		# remove pre-existing object files from object library to ensure an older working version of the module does
		# not get used in case the current version of the module did not compile and failed to produce an object file.
		gt_ar $gt_ar_option_delete $dstlib `cat $lib_` >& /dev/null
		echo "-->  into $dstlib <--"
		gt_ar $gt_ar_option_update $dstlib `cat $lib_`
		if (0 != $status) @ runall_status = $status
		if (("ia64" == $mach_type) && ("hpux" == $platform_name)) then
			ranlib $dstlib
			if (0 != $status) @ runall_status = $status
		endif
		if (0 != $status) @ runall_status = $status
		cat $lib_ | xargs rm -f
		echo ""
	end
	if (0 != $runall_status) then
		goto cleanup
	endif

	echo ""

	echo "----------------------------------------------------------"
	echo "      BUILDing the executables for the $RUNALL_IMAGE version"
	echo "----------------------------------------------------------"
	echo

	# did libmumps.a change
	set mumps_changed = `grep libmumps.a ${TMP_DIR}_Lib_list`

	# on z/OS only, did libgtmzos.a change
	set gtmplatformlib_changed = `grep libgtmzos.a ${TMP_DIR}_Lib_list`

	cat ${TMP_DIR}_Lib_list ${TMP_DIR}_main_.misc | sed 's/.*lib//g' | sed 's/\.a$//g' | sort -u >! ${TMP_DIR}_main_.final
endif # if (! -z ${TMP_DIR}_src_files) then

# if either libmumps.a or a platform specific support library changes rebuild everything
if ("$mumps_changed" != "" || "$gtmplatformlib_changed" != "") then
	$shell $gtm_tools/build${RUNALL_IMAGE}.csh $RUNALL_VERSION
	if (0 != $status) @ runall_status = $status
else
	# If the plugin files are modified, include them in the final list of build routines.
	if ("TRUE" == "$exclude_compile_list_modified") then
		echo "gtmcrypt" >>! ${TMP_DIR}_build_routine.final
	endif
	if (-e ${TMP_DIR}_main_.final) then
		cat ${TMP_DIR}_main_.final >>! ${TMP_DIR}_build_routine.final
	endif
	# If we have a non-zero pct file list then include gde as one of the final build routines
	if (-e ${TMP_DIR}_pct_files && ! -z ${TMP_DIR}_pct_files) then
		echo "gde" >>! ${TMP_DIR}_build_routine.final
	endif
	set build_routine = `cat ${TMP_DIR}_build_routine.final`
	$gtm_tools/buildaux.csh $RUNALL_VERSION $RUNALL_IMAGE $gtm_root/$RUNALL_VERSION/$RUNALL_IMAGE $build_routine
	if (0 != $status) @ runall_status = $status
endif

# Create the GT.M/GDE/MUPIP/DSE/LKE help databases
$gtm_tools/generate_help.csh $gtm_pct $gtm_log/error.${RUNALL_IMAGE}.log
if ($status) then
	@ runall_status++
	echo "runall-E-hlp, Error generating hlp databases"
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

# Generate Special Debug Files (z/OS specific at the moment)
if ( -e $gtm_tools/gtm_dbgld.csh ) then
	$gtm_tools/gtm_dbgld.csh $RUNALL_IMAGE
	if (0 != $status) @ runall_status = $status
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

exit $runall_status
