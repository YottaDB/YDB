#################################################################
#								#
# Copyright (c) 2001-2023 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
#####################################################################################
#
#	comlist.csh - compile all sources, create object libraries, link executables.
#			Should be kept in sync with similar code in mkutf8dir.csh
#
#	arguments:
#		$1 -	assembler options
#		$2 -	C compiler options
#		$3 -	"gtm_bta" => build bta images ($gtm_ver/bta)
#			"gtm_dbg" => build dbg images ($gtm_ver/dbg)
#			"gtm_pro" => build pro images ($gtm_ver/pro)
#		$4 -	version number (without punctuation) or code letter:
#			e.g., "V123" => version "V1.2-3" or:
#				"a" => current active (in current process) release
#				"d" => current development release
#				"p" => current production release
#
#####################################################################################

echo "Start of $0 `date`"
echo ""

echo "Built on $HOST"
echo ""

echo "arguments: '$1' '$2' '$3' '$4'"
echo ""

source $gtm_tools/gtm_env.csh
if (! $?gtm_curpro) then
	# The tools and test system relies on $gtm_curpro defined in the environment for greater flexibility
	# Some day we need to eliminate versions.csh and expect $gtm_curpro to be defined in the environment.
	# setenv gtm_curpro `source $gtm_tools/versions.csh ; echo $gtm_curpro`
	echo 'COMLIST-E-CURPRO: gtm_curpro not defined. comlist expects gtm_curpro to be defined and exist in $gtm_root'
	exit 1
endif
echo "-------------------------"
echo "locale output at start"
echo "-------------------------"
locale
echo ""

echo "Setting locale and LANG to C unconditionally"
unsetenv gtm_chset
setenv LC_ALL C
setenv LANG C

echo "-------------------------"
echo "locale output after reset"
echo "-------------------------"
locale
echo ""

echo "--------------------------------------------"
echo "Value at start : ydb_dist=$ydb_dist"
echo "Value at start : gtmroutines=""$gtmroutines"""
echo "--------------------------------------------"
echo ""

echo "Setting ydb_dist and gtmroutines to non-UTF8 unconditionally"
setenv ydb_dist $gtm_exe
setenv gtmroutines ". $gtm_exe"
echo ""

echo "--------------------------------------------"
echo "Value after reset : ydb_dist=$ydb_dist"
echo "Value after reset : gtmroutines=""$gtmroutines"""
echo "--------------------------------------------"
echo ""

# Unset gtm_custom_errors to prevent some errors, especially %YDB-E-FTOKERR if $gtm_repl_instance is set in the environment
unsetenv gtm_custom_errors

# Remove old temporary files and logs.
set errorlog = "$gtm_log/error.`basename $gtm_exe`.log"
rm -f $errorlog $gtm_log/gt_cc_*.{csh,out} $gtm_log/buildaux_*.{out,err,csh} $gtm_log/tttgen_*.log

@ comlist_status = 0

set dollar_sign = \$
alias error_and_exit 'echo \!:1 ; echo \!:1 >> $errorlog ; @ comlist_status++ ; goto comlist.END'

unalias ls rm

set comlist_start_directory = `pwd`

# Verify arguments:
set p1 = "$1"
set p2 = "$2"

setenv	gt_as_options	"$p1"
setenv	gt_cc_options	"$p2"

# Default to building the development version.
if ( $4 == "" ) then
	set p4 = "d"
else
	set p4 = $4
endif

# Define image type-specific information.
# Use environment variables for the assembler and C compiler because aliases aren't inherited.

set p3 = $3
switch ( $p3 )
case "gtm_bta":
	setenv	comlist_gt_as	"`alias gt_as_bta` $p1"
	setenv	comlist_gt_cc	"`alias gt_cc_bta` $p2"
	set setactive_parms = ( $p4 b ) ; source $gtm_tools/setactive.csh
	@ comlist_status = $status
	set p3 = $gtm_bta
	set asmtgbltype = "pro"
	breaksw

case "gtm_dbg":
	setenv	comlist_gt_as	"`alias gt_as_dbg` $p1"
	setenv	comlist_gt_cc	"`alias gt_cc_dbg` $p2"
	set setactive_parms = ( $p4 d ) ; source $gtm_tools/setactive.csh
	@ comlist_status = $status
	set p3 = $gtm_dbg
	set asmtgbltype = "dbg"
	breaksw

case "gtm_pro":
	setenv	comlist_gt_as	"`alias gt_as_pro` $p1"
	setenv	comlist_gt_cc	"`alias gt_cc_pro` $p2"
	set setactive_parms = ( $p4 p ) ; source $gtm_tools/setactive.csh
	@ comlist_status = $status
	set p3 = $gtm_pro
	set asmtgbltype = "pro"
	breaksw

default:
	error_and_exit "COMLIST-E-BADP3 : Third argument invalid, should be one of: gtm_bta, gtm_dbg, gtm_pro -- no action taken."
	breaksw

endsw

if (0 != $comlist_status) then
	error_and_exit "COMLIST-E-VERSION : version command failed -- aborting build"
endif

switch ( $gtm_verno )

case "V990":
case "V999*":
	#	V990/V999 is designated "most recent on the main line"
	#	and should be protected from inadvertent change.
	set comlist_chmod_protect = 1
	breaksw

case "V9*":
	#	Other V9 releases are provided for the "private" use of developers to
	#	modify as they see fit for test purposes and should allow modification.
	set comlist_chmod_protect = 0
	breaksw

default:
	#	Anything else should be a release version
	#	and should be protected against inadvertent change.
	set comlist_chmod_protect = 1
	breaksw

endsw

set mach_type = `uname -m`

if ( $comlist_chmod_protect == 1 ) then
	#	Change the permissions on all of the source files to prevent inadvertent
	#	modification by developers.
	set comlist_chmod_conf = 755
	set comlist_chmod_src = 444

else
	#	Change the permissions on all of the source files to allow modification by developers.
	set comlist_chmod_conf = 775
	set comlist_chmod_src = 664

endif


if !(-o $gtm_ver) then
	error_and_exit "COMLIST-E-NOTOWNER : $USER is not owner of $gtm_ver cannot coninue"
endif

#	Change the permissions on the root directory for this version to allow
#	others in this group (i.e., developers) to create new subdirectories.  This
#	is usually done for running tests or creating readme.txt files, although
#	there may be other reasons.
chmod 775 $gtm_ver



#	Change the permissions on the configured directories for this version to
#	prevent inadvetent modification (creation / deletion of files) by developers
#	or allow modification as appropriate.
cd $gtm_ver
chmod $comlist_chmod_conf inc pct src tools gtmsrc.csh

chmod 775 log
if !(-w $gtm_exe) then
	echo "COMLIST-E-PERMISSON : There is no write permission to $gtm_exe. Exiting"
	exit
endif


#	Change permisions on all source files to prevent inadvertent modification of
#	source files corresponding to configured releases and allowing modification
#	for development and test releases.
cd $gtm_inc
chmod $comlist_chmod_src *


cd $gtm_pct
chmod $comlist_chmod_src *


#	In $gtm_src, use xargs because this directory has so many files that the
#	command line expansion overflows the command length limits on some Unix
#	implementations.
cd $gtm_src
/bin/ls | xargs -n25 chmod $comlist_chmod_src

cd $gtm_tools
chmod $comlist_chmod_src *

if ( $comlist_chmod_protect == 1 ) then
	chmod 555 *sh	# make the shell scripts executable, but protect against inadvertent modification

else
	chmod 775 *sh	# make the shell scripts exectuable and allow modification

endif



echo ""
echo "Assembler-related aliases:"
echo ""
alias | grep gt_as | sort
echo ""
echo ""

echo "Assembler-related environment variables:"
echo ""
env | grep gt_as | sort
echo ""
echo ""


echo "C compiler-related aliases:"
echo ""
alias | grep gt_cc | sort
echo ""
echo ""

echo "C compiler-related environment variables:"
echo ""
env | grep gt_cc | sort
echo ""
echo ""

echo "Linker-related aliases:"
echo ""
alias | grep gt_ld | sort
echo ""
echo ""

echo "Linker-related environment variables:"
echo ""
env | grep gt_ld | sort
echo ""
echo ""

echo "Archiver-related aliases:"
echo ""
alias | grep gt_ar | sort
echo ""
echo ""

echo "Archiver-related environment variables:"
echo ""
env | grep gt_ar | sort
echo ""
echo ""


set p5 = $5

cd $p3

#	Clean slate.
chmod +w *		# this allows the rm to work without prompting for an override
rm *

cp $gtm_tools/lowerc_cp.sh lowerc_cp
cp $gtm_tools/ydbinstall.sh ydbinstall

chmod +x {lowerc,ydbinstall}*

cp $gtm_tools/*.xc .
cp $gtm_tools/*.gtc .
# Fix the copyright year in configure
set yyyy = `date +%Y`
sed 's/Copyright 2009-YYYY/Copyright 2009-'$yyyy'/' configure.gtc >&! configure
rm -f configure.gtc

cp $gtm_inc/gtm_common_defs.h .
cp $gtm_inc/gtmxc_types.h .
cp $gtm_inc/gtm_descript.h .
cp $gtm_inc/gtm_sizeof.h .
#
# headers to support ASCII/EBCDIC independence
#
cp $gtm_inc/main_pragma.h .
cp $gtm_inc/gtm_limits.h .
cp $gtm_inc/gtm_stdio.h .
cp $gtm_inc/gtm_stdlib.h .
cp $gtm_inc/gtm_string.h .
cp $gtm_inc/gtm_strings.h .
if ( "$HOSTOS" == "OS/390disable" ) then
	cp $gtm_inc/global_a.h .
	cp $gtm_inc/gtm_unistd.h .
	cp $gtm_inc/gtm_netdb.h .
	cp $gtm_inc/gtm_stat.h .
endif

#	If this is a test version not being built in $gtm_exe
#	(see value of $3), then make sure ./obj and ./map exist.
if ( ! -d ./obj ) then
	mkdir ./obj
endif
if ( ! -d ./map ) then
	mkdir ./map
endif
cd ./obj

# Remove anything that's not a library.  We pass -r on Linux to suppress invoking rm with zero args (the xargs default on AIX)
set xargs_xtra=""
if ("Linux" == $HOSTOS) then
	set xargs_xtra="-r"
endif
find . -type f -name '*.a' -prune -o -type f -print | sort | xargs $xargs_xtra -n25 rm

set gi = ($gtm_inc)
set gs = ($gtm_src)

# Irrespective of the gtm_chset value from the user environment, all
# M objects generated in $ydb_dist (GDE*.o, _*.o, ttt.o) must be
# compiled with gtm_chset="M".
unsetenv gtm_chset

#############################################################
#
#  Generate the error message definition files and also ttt.c
#
#############################################################
if ( -x $gtm_root/$gtm_curpro/pro/mumps ) then
    set comlist_ZPARSE_error_count = 0
    pushd $gtm_src
    # ydb_chk_dist requires ydb_dist setup
    set real_ydb_dist = "$ydb_dist"
    setenv ydb_dist "$gtm_root/$gtm_curpro/pro"
    set old_gtmroutines = "$gtmroutines"
    setenv gtmroutines "$gtm_obj($gtm_pct)"
    set msgstatus = 0
    foreach i ( *.msg )

	set j = `basename $i .msg`
	rm -f ${j}_ctl.c ${gtm_inc}${j}_ansi.h	# in case an old version is lying around

	# MSG.m converts a VMS-style error message MSG input file to a
	# file with information about the messages.
	# On unix, the C source file includes the message texts.
	$gtm_root/$gtm_curpro/pro/mumps -run msg $i
	if (0 != $status) then
		echo "COMLIST-E-MSG : 'mumps -run msg $i' failed" >> $errorlog
		@ msgstatus++
	endif
	if ( ! -f ${j}_ctl.c ) then
		echo "COMLIST-E-MSG : MSG.m failed to produce output file ${j}_ctl.c" >> $errorlog
		@ msgstatus++
	endif
	if ( -f ${j}_ansi.h ) then
		mv -f ${j}_ansi.h $gtm_inc
	endif
    end

    #  Generate ttt.c
    $shell -f $gtm_tools/gen_ttt.csh
    @ msgstatus = $msgstatus + $status

    setenv gtmroutines "$old_gtmroutines"
    unset old_gtmroutines
    setenv ydb_dist "$real_ydb_dist"
    unset real_ydb_dist
    popd
    if ($msgstatus) then
    	error_and_exit "COMLIST-E-MSG : Error running msg.m or gen_ttt.csh"
    endif
else
    error_and_exit "COMLIST-E-NO_MUMPS : unable to regenerate merrors.c and ttt.c due to missing $gtm_curpro/pro/mumps"
endif

#############################################################
#
#  Generate omi_sx_play.c from omi_srvc_xct.c
#
#############################################################

if (-e $gs[1]/omi_sx_play.c) then
	chmod a+w $gs[1]/omi_sx_play.c
endif
cp $gs[1]/omi_srvc_xct.c $gs[1]/omi_sx_play.c
chmod a-w $gs[1]/omi_sx_play.c

#############################################################
#
#  Copy over gtcm configuration files to the distribution directory
#
#############################################################

foreach file (gtcm_slist.gtc gtcm_run.gtc)
	if ( -e $p3/$file ) then
		chmod +w $p3/$file
	endif
	cp $gtm_tools/$file $p3
	chmod a-wx $p3/$file
end

############################################## Compilations and Assemblies ###################################################

# C compilations first:

# For ia64 & x86_64, the file - xfer_desc.i - needs to be generated.
if ( "ia64" == $mach_type || "x86_64" == $mach_type ) then
        pushd $gtm_src
        $shell -f $gtm_tools/gen_xfer_desc.csh
        popd
endif

# For all systems, the file gtm_threadgbl_deftypes.h needs to be generated (no -f as needs startup file)
$shell -f $gtm_tools/gen_gtm_threadgbl_deftypes.csh
if (0 != $status) then
    echo "Failed to generate gtm_threadgbl_deftypes.h -- aborting build"
    exit 1
endif

# Setup link from $gtm_obj to the proper assembler include file if this is for a version that doesn't already
# have a gtm_threadgbl_deftypes_asm.si in its $gtm_inc directory in order to include the proper variant.
if ((! -e ${gtm_inc}/gtm_threadgbl_deftypes_asm.si) && (! -e ${gtm_obj}/gtm_threadgbl_deftypes_asm.si)) then
    \ln -s ${gtm_inc}/gtm_threadgbl_deftypes_asm_${asmtgbltype}.si ${gtm_obj}/gtm_threadgbl_deftypes_asm.si
endif

echo ""
echo "Start of C Compilation"	# Do not change this string. $gtm_tools/buildwarn.awk relies on this to detect warnings.
echo ""

# List of files that should be excluded from the compilation as they are dealt with separately
set TMP_DIR_PREFIX = "/tmp/__${user}__comlist"
set TMP_DIR = "${TMP_DIR_PREFIX}__`date +"%y%m%d_%H_%M_%S"`_$$"
\mkdir -p ${TMP_DIR}
cat << EOF >&! ${TMP_DIR}/exclude.lst
omi_sx_play.c
gtm_threadgbl_deftypes.c
gtmcrypt_pk_ref.c
gtmcrypt_dbk_ref.c
gtmcrypt_sym_ref.c
gtmcrypt_ref.c
gtmcrypt_util.c
maskpass.c
gtm_tls_impl.c
EOF
find $gs[1] -name '*.c' | grep -v -f ${TMP_DIR}/exclude.lst | sort | xargs -n25 $gtm_tools/gt_cc.csh
\rm -rf ${TMP_DIR}

set nonomatch ; set compileout=($gtm_log/gt_cc_local_*.out) ; unset nonomatch
if ("$gtm_log/gt_cc_local_*.out" != "$compileout") then
	grep -qE 'COMLIST-E-' $gtm_log/gt_cc_local_*.out
	if (0 == $status) then
		error_and_exit "COMLIST-E-COMPILE : Compiling one or more source files failed -- aborting build"
	endif
endif

# Special compilation for omi_sx_play.c
set comlist_gt_cc_bak = "$comlist_gt_cc"
setenv comlist_gt_cc "$comlist_gt_cc -DFILE_TCP"
$gtm_tools/gt_cc.csh $gtm_src/omi_sx_play.c
setenv comlist_gt_cc "$comlist_gt_cc_bak"

echo ""
echo "End of C Compilation"	# Do not change this string. $gtm_tools/buildwarn.awk relies on this to detect warnings.
echo ""

if ( $?gt_xargs_insert == 0 ) setenv gt_xargs_insert "-i"

# Assembly language assemblies next so they can supersede the C sources by overwriting the object files:

echo "Start of Assembly"	# Do not change this string. $gtm_tools/buildwarn.awk relies on this to detect warnings.
# AS - 2010/07/12
# No longer valid. Probably applied to sr_dux, but gt_as_inc_cvt.csh does
# not exist in the current revision or in CVS history
#if ( $gt_as_inc_convert == "true" ) then
#	# Convert assembly language include files to native dialect:
#	foreach cvt (${gi[1]}/*${gt_as_inc_from_suffix})
#		$shell $gtm_tools/gt_as_inc_cvt.csh $cvt
#	end
#endif

if ( "$HOSTOS" == "OS/390" ) then
    $shell -f $gtm_tools/gt_os390_maclib.csh
endif

# AS - 2010/07/12 this applies to sr_dux only
if ( $gt_as_src_convert == "true" ) then
	# Convert assembly language sources to native dialect in this directory:
	foreach cvt (${gs[1]}/*${gt_as_src_from_suffix})
		$shell -f $gtm_tools/gt_as_src_cvt.csh $cvt
	end
endif

if ( $?gt_as_use_prebuilt == 0 ) then
	# Finally assemble any sources originally in native dialect so they
	# can supersede any conflicting non-native dialect sources:
	@ asm_batch_size=25
	@ asm_batch_tail = ${asm_batch_size} + 1
	set asmlist=(`echo ${gs[1]}/*${gt_as_src_suffix}`)
	while ($#asmlist)
		if (${#asmlist} > ${asm_batch_size}) then
			set asmsublist=(${asmlist[1-${asm_batch_size}]})
			set asmlist=(${asmlist[${asm_batch_tail}-]})
		else
			set asmsublist=(${asmlist})
			set asmlist=()
		endif
		$shell -f $gtm_tools/gt_as.csh ${asmsublist}
	end
else
	cp -p $gtm_ver/$gt_as_use_prebuilt/*.o .
endif

if ( $HOSTOS =~ "CYGWIN*" ) then
	echo "Prefixing _ to .o in $gtm_exe to match Cygwin/Windows naming rules"
	foreach x (${gs[1]}/*.s)
		objcopy --prefix-symbols="_" $gtm_exe/obj/$x:r:t.o
	end
endif

echo "End of Assembly"	# Do not change this string. $gtm_tools/buildwarn.awk relies on this to detect warnings.
echo ""

############################################## Archiving object files ###################################################


pushd $gtm_tools >& /dev/null
set comlist_liblist = `ls *.list | sed 's/.list//' | sed 's/^lib//'`
set comlist_liblist = "$comlist_liblist mumps"
popd >& /dev/null

foreach i ( $comlist_liblist )
	if ( -f lib$i.a ) then
		mv lib$i.a lib$i.a.bak
	endif
	if ( -f ar$i.log ) then
		mv ar$i.log ar$i.log.bak
	endif

	echo "Start of $i archive creation: `date`"
	echo "archiver (ar) options: $gt_ar_option_create" > ar$i.log
	echo "" >> ar$i.log

	switch ( $i )
	case "mumps":
		# (Almost) everything else goes into libmumps.a, but the list is too long for a single command line so use xargs.
		# This case must be executed last in the switch statement (because it picks up "everything else") and, hence,
		# must appear last in the for statement.

		#--------------------------------------------------------------------------------------
		# While defining exclude files, please append "^" at the beginning of each file name to prevent
		# 	other files from being excluded
		#--------------------------------------------------------------------------------------

		# Exclude files that define the same externals
		# (e.g., "main" and the VMS CLI [command line interpreter] emulator arrays):
		set exclude = "^gtm\.o|^gtm_main\.o|^lke\.o|^lke_cmd\.o|^dse\.o|^dse_cmd\.o"
		set exclude = "$exclude|^mupip\.o|^mupip_cmd\.o|^gtmsecshr\.o|^gtmsecshr_wrapper\.o"
		set exclude = "$exclude|^msg\.o|^gtcm_main\.o|^gtcm_play\.o|^gtcm_pkdisp\.o|^gtcm_shmclean\.o"
		set exclude = "$exclude|^omi_srvc_xct\.o|^omi_sx_play\.o"
		set exclude = "$exclude|^gtcm_gnp_server\.o"
		set exclude = "$exclude|^dummy_gtmci\.o"
		/bin/ls | egrep '\.o$' | egrep -v "$exclude" | \
			xargs -n50 $shell -f $gtm_tools/gt_ar.csh $gt_ar_option_create lib$i.a >>& ar$i.log
		if ( $status ) then
			@ comlist_status++
			echo "COMLIST-E-ARCHIVE : Error creating lib$i.a archive (see ${dollar_sign}gtm_obj/ar$i.log)" \
			>> $errorlog
		endif
		breaksw

	default:
		gt_ar $gt_ar_option_create lib$i.a `sed -f $gtm_tools/lib_list_ar.sed $gtm_tools/lib$i.list` >>& ar$i.log
		if ( $status ) then
			@ comlist_status++
			echo "COMLIST-E-ARCHIVE : Error creating lib$i.a archive (see ${dollar_sign}gtm_obj/ar$i.log)" \
			>> $errorlog
		endif
		rm -f `sed -f $gtm_tools/lib_list_ar.sed $gtm_tools/lib$i.list`
		breaksw

	endsw

	if ( $gt_ar_use_ranlib == "yes" ) then
		ranlib lib$i.a
	endif

	if ( -f lib$i.a.bak ) then
		rm lib$i.a.bak
	endif
	if ( -f ar$i.log.bak) then
		rm ar$i.log.bak
	endif

	echo "" >> ar$i.log
	echo "End of $i archive creation: `date`"
end

if (0 != $comlist_status) then
	error_and_exit "COMLIST-E-ARCHIVE : One of the archive creations above failed -- aborting build"
endif

/bin/ls | egrep '\.o$' | egrep -v "$exclude" | xargs -n25 rm -f

switch ( $3 )
case "gtm_bta":
	set bldtype = "Bta"
	set buildscript = "$gtm_tools/buildbta.csh"
	set buildargs = "$p4"
	breaksw

case "gtm_dbg":
	set bldtype = "Dbg"
	set buildscript = "$gtm_tools/builddbg.csh"
	set buildargs = "$p4"
	breaksw

case "gtm_pro":
	set bldtype = "Pro"
	set buildscript = "$gtm_tools/buildpro.csh"
	set buildargs = "$p4"
	breaksw
endsw

$shell -f $buildscript $buildargs
if ($status) @ comlist_status++
if (0 != $comlist_status) then
	error_and_exit "COMLIST-E-BUILD : $buildscript invocation above failed -- aborting build"
endif

if ( ! -x $ydb_dist/mumps ) then
	error_and_exit "COMLIST-E-NOMUMPS : ${dollar_sign}ydb_dist/mumps is not executable"
endif

set mupip_size = `ls -l $gtm_exe/mupip |awk '{print $5}'`
set yottadb_size = `ls -l $gtm_exe/libyottadb$gt_ld_shl_suffix |awk '{print $5}'`

cd $p3

# Generate Special Debug Files (z/OS specific at the moment)
if ( -e $gtm_tools/gtm_dbgld.csh ) then
	$gtm_tools/gtm_dbgld.csh `echo $3 | sed 's/gtm_//'`
endif

# Build GTMDefinedTypesInit.m file but where to run the generation script from depends on whether our current working
# directory has a version of it or not.
rm -f obj/gengtmdeftypes.log*
rm -f GTMDefinedTypesInit.m
echo "Generating GTMDefinedTypesInit.m"
$gtm_tools/gengtmdeftypes.csh >& obj/gengtmdeftypes.log
@ savestatus = $status
if ((0 != $savestatus) || (! -e GTMDefinedTypesInit.m)) then
	set errmsg = "COMLIST-E-GTMDEFINEDTYPES : gengtmdeftypes.csh failed to create GTMDefinedTypesInit.m "
	set errmsg = "$errmsg - see log in $gtm_obj/gengtmdeftypes.log"
	@ comlist_status++
	echo $errmsg >> $errorlog
endif
if (-e struct_padding_warn.out) then
	mv struct_padding_warn.out $gtm_log/struct_padding_warn.${bldtype:al}.out
endif

if (-e GTMDefinedTypesInit.m) then
	# Need a different name for each build type as they can be different
	cp -f GTMDefinedTypesInit.m $gtm_pct/GTMDefinedTypesInit${bldtype}.m
	setenv LC_CTYPE C
	setenv gtm_chset M
	./mumps GTMDefinedTypesInit.m
	if ($status) then
		set errmsg = "COMLIST-E-GTMDEFINEDTYPES : Failed to compile generated $gtm_exe/GTMDefinedTypes.m"
		@ comlist_status++
		echo "${errmsg}" >> $errorlog
	endif
	# If we have a utf8 dir (created by buildaux.csh called from buildbdp.csh above), add a link to it for
	# GTMDefinedTypesInit.m and compile it in UTF8 mode
	source $gtm_tools/set_library_path.csh
	source $gtm_tools/check_utf8_support.csh
	if (-e $gtm_dist/utf8 && ("TRUE" == "$is_utf8_support")) then
		set icuver =  `setenv gtm_dist $PWD ; $gtm_tools/is_icu_symbol_rename.csh`
		if ("" != "$icuver") setenv ydb_icu_version "$icuver"
		if (! -e $gtm_dist/utf8/GTMDefinedTypesInit.m) then
		    ln -s $gtm_dist/GTMDefinedTypesInit.m $gtm_dist/utf8/GTMDefinedTypesInit.m
		endif
		pushd utf8
		# Switch to UTF8 mode
		if ( "OS/390" == $HOSTOS ) setenv gtm_chset_locale $utflocale      # LC_CTYPE not picked up right
		setenv LC_CTYPE $utflocale
		unsetenv LC_ALL
		setenv gtm_chset UTF-8  # switch to "UTF-8" mode
		# mumps executable not yet linked to utf8 dir so access it in parent directory
		../mumps GTMDefinedTypesInit.m
		if ($status) then
			set errmsg = "COMLIST-E-GTMDEFINEDTYPES : Failed to compile generated $gtm_exe/utf8/GTMDefinedTypes.m"
			@ comlist_status++
			echo "${errmsg}" >> $errorlog
		endif
		popd
		setenv LC_CTYPE C
		unsetenv gtm_chset      # switch back to "M" mode
		if ( "OS/390" == $HOSTOS ) unsetenv gtm_chset_locale
	endif
endif

if (0 != $comlist_status) then
	error_and_exit "COMLIST-E-GTMDEFINEDTYPES : Generating GTMDefinedTypesInit.m failed -- aborting build"
endif

echo "Generating gtmpcat field build"
set old_gtmroutines = "$gtmroutines"
setenv gtmroutines "$gtm_obj($gtm_tools)"
pushd $gtm_tools
chmod +w .
rm -f gtmpcat*On*.m gtm_threadgbl_undefs.h
$gtm_exe/mumps -r gtmpcatfldbld gtmpcat_field_def.txt ${gtm_verno}
if ($status) then
	error_and_exit "COMLIST-E-GTMPCAT : Failed to generate gtmpcat field build -- aborting build"
endif
ls -l gtmpcat*
popd
setenv gtmroutines "$old_gtmroutines"
rm $gtm_obj/gtmpcatfldbld.o

# Create the GT.M/GDE/MUPIP/DSE/LKE help databases
$gtm_tools/generate_help.csh $gtm_pct $errorlog
if ($status) then
	@ comlist_status++
	echo "COMLIST-E-HLP : Error generating hlp databases" >> $errorlog
endif

chmod 775 *	# do not check $status here because we know it will be 1 since "gtmsecshr" permissions cannot be changed.

# Create a mirror image (using soft links) of $ydb_dist under $ydb_dist/utf8 if it exists.
if (-e $gtm_exe/utf8) then	# would have been created by buildaux.csh while building GDE
	pushd $gtm_exe
	foreach file (*)
		# Skip utf8 directory
		if (-d $file && "utf8" == $file) then
			continue
		endif
		# Skip soft linking .o files
		set extension = $file:e
		if ($extension == "o") then
			continue
		endif
		# Soft link everything else
		if (-e utf8/$file) then
			rm -rf utf8/$file
			if ($status) then
				@ comlist_status++
				echo "COMLIST-E-RM : Error deleting utf8/$file" >> $errorlog
			endif
		endif
		ln -s ../$file utf8/$file
		if ($status) then
			@ comlist_status++
			echo "COMLIST-E-LN : Error linking $file" >> $errorlog
		endif
	end
	popd
endif
# To check the length of path of files even insdie gtmsecshr directory, relax permissions first
$gtm_com/IGS $ydb_dist/gtmsecshr UNHIDE
set distfiles_log = "dist_files.`basename $gtm_exe`.log"
find $ydb_dist -type f >&! $gtm_log/$distfiles_log
if ($?scan_image) then
	tar cvf $ydb_dist/veracode-${gtm_verno}-${HOST:ar}.tar dse gtmsec* gtcm* libgtmshr.so lke mumps mupip
	tar rvf $ydb_dist/veracode-${gtm_verno}-${HOST:ar}.tar plugin/libgtm* plugin/gtmcrypt/maskpass
	tar rvf $ydb_dist/veracode-${gtm_verno}-${HOST:ar}.tar /usr/lib64/lib{config,gpgme,gpg-error,crypt,ssl,icu*,z,elf}.so*
	gzip    $ydb_dist/veracode-${gtm_verno}-${HOST:ar}.tar
endif
$gtm_com/IGS $ydb_dist/gtmsecshr CHOWN
awk 'BEGIN {dlen=length(ENVIRON["ydb_dist"]);stat=0} {if ((length($0)-dlen)>50) {stat=1}} END {exit stat}' $gtm_log/$distfiles_log
if ($status) then
	@ comlist_status++
	echo "COMLIST-E-PATHLENGTH : The longest path beyond \$ydb_dist exceeds 50 bytes" >> $errorlog
	awk 'BEGIN {dlen=length(ENVIRON["ydb_dist"]);stat=0} \
		{if ((length($0)-dlen)>50) {print $0,"- length :",length($0)-dlen ; stat=1}} \
		END {exit stat}' $gtm_log/$distfiles_log >>&! $errorlog
endif

if ( $comlist_chmod_protect == 1 ) then
	# If it is release build, protect it from inadvertent modification/rebuild etc
	chmod -R a-w $gtm_inc $gtm_pct $gtm_src $gtm_tools $gtm_ver/gtmsrc.csh $gtm_exe $gtm_log
	chmod ug+w $gtm_ver
	chmod ug+w $gtm_log
endif
comlist.END:

echo ""
echo ""
if ( -f $errorlog ) then
	echo "Error summary:"
	echo ""
	cat $errorlog
else
	echo "No errors were detected by comlist.csh"
endif
echo ""

# Return to starting directory:
cd $comlist_start_directory

# Clean up environment variables:
unsetenv comlist_gt_as
unsetenv comlist_gt_cc

# Clean up local shell variables:
unset comlist_chmod_conf
unset comlist_chmod_protect
unset comlist_chmod_src
unset comlist_liblist
unset comlist_start_directory
unset comlist_ZPARSE_error_count
unset p1
unset p2
unset p3
unset p4

echo "############# OS and other library versions at the time of build #############"
uname -a
if ("AIX" == "$HOSTOS") then
	oslevel -s
	$gt_cc_compiler -qversion
else if ("Linux" == $HOSTOS) then
	if (-X lsb_release) then
		lsb_release -ds | awk '{gsub(/"/,"") ; print}'
	else if (-e /etc/os-release) then
		awk -F= '/^PRETTY_NAME=/ {gsub(/"/,"",$2) ; print $2}' /etc/os-release
	endif
	$gt_cc_compiler --version |& head -1
endif
# gpg and related versions"
if (-X gpg2) then
	gpg2 --version |& grep -E '^gpg |^libgcrypt '
else if (-X gpg) then
	gpg --version |& grep -E '^gpg |^libgcrypt '
endif
if (-X gpg-agent) then
	gpg-agent --version |& grep -E '^gpg-agent '
endif
if (-X openssl) then
	openssl version
endif
echo "##############################################################################"
echo ""
echo "Exit status (should be 0) is : $comlist_status"
echo ""
echo "End of $0 `date`"

exit $comlist_status
