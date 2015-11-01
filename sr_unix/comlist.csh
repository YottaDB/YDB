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

#
#####################################################################################
#
#	comlist.csh - compile all sources, create object libraries, link executables.
#
#	arguments:
#		$1 -	assembler options
#		$2 -	C compiler options
#		$3 -	"gtm_bta" => build bta images ($gtm_vrt/bta)
#			"gtm_dbg" => build dbg images ($gtm_vrt/dbg)
#			"gtm_pro" => build pro images ($gtm_vrt/pro)
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

set comlist_status = 0

set dollar_sign = \$

set comlist_start_directory = `pwd`

switch ( $gtm_verno )


case "V990":
	#	V990 is designated "most recent on the main line of descent in CMS"
	#	and should be protected from inadvertent change.
	set comlist_chmod_protect = 1
	breaksw

case "V99*":
	#	Other V9.9 releases are provided for the "private" use of developers to
	#	modify as they see fit for test purposes and should allow modification.
	set comlist_chmod_protect = 0
	breaksw

default:
	#	Anything else should be a configured release (i.e., should correspond to
	#	a CMS release class) and should be protected against inadvertent change.
	set comlist_chmod_protect = 1
	breaksw

endsw


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


#	Change the permissions on the root directory for this version to allow
#	others in this group (i.e., developers) to create new subdirectories.  This
#	is usually done for running tests or creating readme.txt files, although
#	there may be other reasons.
chmod 775 $gtm_ver



#	Change the permissions on the configured directories for this version to
#	prevent inadvetent modification (creation / deletion of files) by developers
#	or allow modification as appropriate.
cd $gtm_ver
chmod $comlist_chmod_conf bta dbg pro inc pct src tools gtmsrc.csh

chmod 775 log



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

#	Remove old error log.
rm $gtm_log/error.`basename $gtm_exe`.log

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
	version $p4 b
	set p3 = $gtm_bta
	breaksw

case "gtm_dbg":
	setenv	comlist_gt_as	"`alias gt_as_dbg` $p1"
	setenv	comlist_gt_cc	"`alias gt_cc_dbg` $p2"
	version $p4 d
	set p3 = $gtm_dbg
	breaksw

case "gtm_pro":
	setenv	comlist_gt_as	"`alias gt_as_pro` $p1"
	setenv	comlist_gt_cc	"`alias gt_cc_pro` $p2"
	version $p4 p
	set p3 = $gtm_pro
	breaksw

default:
	echo "comlist-F-badp3, Third argument invalid, should be one of: gtm_bta, gtm_dbg, gtm_pro -- no action taken."
	set comlist_status = -3
	goto comlist.END
	breaksw

endsw


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

cp $gtm_tools/lowerc.sh lowerc
cp $gtm_tools/lowerc_cp.sh lowerc_cp
cp $gtm_tools/upperc.sh upperc
cp $gtm_tools/upperc_cp.sh upperc_cp

cp $gtm_tools/{lowerc,upperc}*.csh .

chmod +x {lowerc,upperc}*

cp $gtm_tools/*.gtc .
mv configure{.gtc,}

cp $gtm_tools/repl_sort_add_seq.awk repl_sort_add_seq.awk
cp $gtm_tools/repl_sort_rem_tran.awk repl_sort_rem_tran.awk

cp $gtm_inc/gtmxc_types.h .
cp $gtm_inc/gtm_descript.h .
#
# headers to support ASCII/EBCDIC independence
#
cp $gtm_inc/gtm_stdio.h .
cp $gtm_inc/gtm_stdlib.h .
cp $gtm_inc/gtm_string.h .
if ( "$HOSTOS" == "OS/390" ) then
	cp $gtm_inc/global_a.h .
	cp $gtm_inc/gtm_unistd.h .
	cp $gtm_inc/gtm_netdb.h .
	cp $gtm_inc/gtm_stat.h .
endif


if ( "$HOSTOS" == "SunOS" ) then
	# Support for RPC implementation of DAL's and ZCALL's.
	# gtm_descript.h is already copied.
	cp $gtm_inc/gtmidef.h .
endif

cp $gtm_pct/*.hlp .


#	If this is a test version not being built in $gtm_exe
#	(see value of $3), then make sure ./obj and ./map exist.
if ( ! -d ./obj ) then
	mkdir ./obj
endif
if ( ! -d ./map ) then
	mkdir ./map
endif
cd ./obj

# Remove anything that's not a library.
/bin/ls | egrep -v '\.a$' | xargs -n25 rm

set eol_anchor = '$'
set gi = ($gtm_inc)
set gs = ($gtm_src)

#############################################################
#
#  Generate the error message definition files.
#
#############################################################
if ( -x $gtm_root/$gtm_curpro/pro/mumps ) then
    set comlist_ZPARSE_error_count = 0
    pushd $gtm_src
    # gtm_startup_chk requires gtm_dist setup
    set real_gtm_dist = "$gtm_dist"
    setenv gtm_dist "$gtm_root/$gtm_curpro/pro"
    set old_gtmroutines = "$gtmroutines"
    setenv gtmroutines "$gtm_obj($gtm_pct)"
    foreach i ( *.msg )

	set j = `basename $i .msg`
	rm -f ${j}_ctl.c ${gtm_inc}${j}_ansi.h	# in case an old version is lying around

	# MSG.m converts a VMS-style error message MSG input file to a
	# file with information about the messages.
	# On unix, the C source file includes the message texts.
	$gtm_root/$gtm_curpro/pro/mumps -run msg $i Unix

	if ( ! -f ${j}_ctl.c ) then
		echo "comlist-E-MSGfail, MSG.m failed to produce output file ${j}_ctl.c" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
	if ( -f ${j}_ansi.h ) then
		mv -f ${j}_ansi.h $gtm_inc
	endif
    end
    setenv gtmroutines "$old_gtmroutines"
    unset old_gtmroutines
    setenv gtm_dist "$real_gtm_dist"
    unset real_gtm_dist

    popd
else
    echo "comlist-E-NoMUMPS, unable to regenerate merrors.c due to missing $gtm_curpro/pro/mumps" \
		    >> $gtm_log/error.`basename $gtm_exe`.log
endif

#############################################################
#
#  Generate omi_sx_play.c from omi_srvc_xct.c
#
#############################################################

if (-e $gs[1]/omi_sx_play.c) then
	chmod +w $gs[1]/omi_sx_play.c
endif
cp $gs[1]/omi_srvc_xct.c $gs[1]/omi_sx_play.c
chmod -w $gs[1]/omi_sx_play.c

#############################################################
#
#  Copy over gtcm configuration files to the distribution directory
#
#############################################################

foreach file (gtcm_slist.gtc gtcm_run.gtc gtcm_gcore)
	if ( -e $p3/$file ) then
		chmod +w $p3/$file
	endif
	cp $gtm_tools/$file $p3
	chmod a-wx $p3/$file
end

############################################## Compilations and Assemblies ###################################################

# C compilations first:


/bin/ls $gs[1] | egrep '\.c$'   | xargs -n25 $shell $gtm_tools/gt_cc.csh

# Special compilation for omi_sx_play.c
set comlist_gt_cc_bak = "$comlist_gt_cc"
setenv comlist_gt_cc "$comlist_gt_cc -DFILE_TCP"
$shell $gtm_tools/gt_cc.csh omi_sx_play.c
setenv comlist_gt_cc "$comlist_gt_cc_bak"

if ( $?gt_xargs_insert == 0 ) setenv gt_xargs_insert "-i"

# Assembly language assemblies next so they can supersede the C sources by overwriting the object files:

if ( $gt_as_inc_convert == "true" ) then
	# Convert assembly language include files to native dialect:
	/bin/ls $gi[1] | egrep "\$gt_as_inc_from_suffix$eol_anchor" | \
		xargs $gt_xargs_insert $shell $gtm_tools/gt_as_inc_cvt.csh "$gs[1]/{}"
endif

if ( "$HOSTOS" == "OS/390" ) then
    $shell $gtm_tools/gt_os390_maclib.csh
endif

if ( $gt_as_src_convert == "true" ) then
	# Convert assembly language sources to native dialect in this directory:
	/bin/ls $gs[1] | egrep "\$gt_as_src_from_suffix$eol_anchor" | \
		xargs $gt_xargs_insert $shell $gtm_tools/gt_as_src_cvt.csh "$gs[1]/{}"

	# Then assemble them:
	/bin/ls | egrep "\$gt_as_src_suffix$eol_anchor" | xargs $gt_xargs_insert $shell $gtm_tools/gt_as.csh {}
endif

if ( $?gt_as_use_prebuilt == 0 ) then
	# Finally assemble any sources originally in native dialect so they
	# can supersede any conflicting non-native dialect sources:
	/bin/ls $gs[1] | egrep "\$gt_as_src_suffix$eol_anchor" | xargs $gt_xargs_insert $shell $gtm_tools/gt_as.csh "$gs[1]/{}"
else
	cp -p $gtm_vrt/$gt_as_use_prebuilt/*.o .
endif

############################################## Compilations and Assemblies ###################################################


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
	case "gtmrpc":
		# Note: libgtmrpc.a must be built in $gtm_exe because it must also be shipped with the release.
		gt_ar $gt_ar_option_create $gtm_exe/lib$i.a `sed -f $gtm_tools/lib_list_ar.sed $gtm_tools/lib$i.list` >>& ar$i.log
		if ( $status != 0 ) then
			@ comlist_status = $comlist_status + 1
			echo "comlist-E-ar${i}error, Error creating lib$i.a archive (see ${dollar_sign}gtm_tools/ar$i.log)" \
				>> $gtm_log/error.`basename $gtm_exe`.log
		endif
		# retain_list.txt contains modules listed in *.list that also need to be
		# included in libmumps.a (eg. getmaxfds, gtm_mumps_call_xdr)
		rm -f `sed -f $gtm_tools/lib_list_ar.sed $gtm_tools/lib$i.list |egrep -v -f $gtm_tools/retain_list.txt`
		breaksw

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
		set exclude = "^gtm\.o|^gtm_main\.o|^gtm_svc\.o|^gtm_dal_svc\.o|^gtm_rpc_init\.o|^mumps_clitab\.o"
		set exclude = "$exclude|^lke\.o|^lke_cmd\.o|^dse\.o|^dse_cmd\.o"
		set exclude = "$exclude|^mupip\.o|^mupip_cmd\.o|^daemon\.o|^gtmsecshr\.o|^geteuid\.o|^dtgbldir\.o"
		set exclude = "$exclude|^semstat2\.o|^ftok\.o|^msg\.o|^gtcm_main\.o|^gtcm_play\.o|^gtcm_pkdisp\.o|^gtcm_shmclean\.o"
		set exclude = "$exclude|^omi_srvc_xct\.o|^omi_sx_play\.o"
		set exclude = "$exclude|^gtcm_gnp_server\.o|^gtcm_gnp_clitab\.o"
		/bin/ls | egrep '\.o$' | egrep -v "$exclude" | \
			xargs -n50 $shell $gtm_tools/gt_ar.csh $gt_ar_option_create lib$i.a >>& ar$i.log
		if ( $status != 0 ) then
			@ comlist_status = $comlist_status + 1
			echo "comlist-E-ar${i}error, Error creating lib$i.a archive (see ${dollar_sign}gtm_tools/ar$i.log)" \
				>> $gtm_log/error.`basename $gtm_exe`.log
		endif
		breaksw

	default:
		gt_ar $gt_ar_option_create lib$i.a `sed -f $gtm_tools/lib_list_ar.sed $gtm_tools/lib$i.list` >>& ar$i.log
		if ( $status != 0 ) then
			@ comlist_status = $comlist_status + 1
			echo "comlist-E-ar${i}error, Error creating lib$i.a archive (see ${dollar_sign}gtm_obj/ar$i.log)" \
				>> $gtm_log/error.`basename $gtm_exe`.log
		endif
		# retain_list.txt contains modules listed in *.list that also need to be
		# included in libmumps.a (eg. getmaxfds, gtm_mumps_call_xdr)
		rm -f `sed -f $gtm_tools/lib_list_ar.sed $gtm_tools/lib$i.list |egrep -v -f $gtm_tools/retain_list.txt`
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


#
# OS/390 clients need the ASCII/EBCDIC conversion library for xcall
#
if ( "$HOSTOS" == "OS/390" ) then
	cp $gtm_obj/libascii.a ${gtm_dist}
endif

/bin/ls | egrep '\.o$' | egrep -v "$exclude" | xargs -n25 rm -f

switch ( $3 )
case "gtm_bta":
	$shell $gtm_tools/buildbta.csh $p4
	@ comlist_status = $comlist_status + $status	# done before each breaksw instead of after endsw
	breaksw						# as $status seems to be get reset in between

case "gtm_dbg":
	$shell $gtm_tools/builddbg.csh $p4
	@ comlist_status = $comlist_status + $status
	breaksw

case "gtm_pro":
	$shell $gtm_tools/buildpro.csh $p4
	@ comlist_status = $comlist_status + $status
	breaksw
endsw

if ( ! -x $gtm_dist/mumps ) then
	echo "comlist-E-nomumps, ${dollar_sign}gtm_dist/mumps is not executable" >> $gtm_log/error.`basename $gtm_exe`.log
	echo "comlist-W-nomsgverify, unable to verify error message definition files" >> $gtm_log/error.`basename $gtm_exe`.log
	echo "comlist-W-noonlinehelp, unable to generate on-line help files" >> $gtm_log/error.`basename $gtm_exe`.log
	goto comlist.END
endif

cd $p3
cd ./obj

# Verify that $gtm_src/ttt.c has been generated from $gtm_tools/ttt.txt, $gtm_inc/opcode_def.h, and $gtm_inc/vxi.h
cp $gtm_inc/opcode_def.h $gtm_inc/vxi.h $gtm_tools/ttt.txt .
gtm <<GTM.in_tttgen
Set \$ZROUTINES=". $gtm_dist"
Do ^TTTGEN
ZContinue
Halt
GTM.in_tttgen
diff {$gtm_src/,}ttt.c >& ttt.dif
if ( $status != 0 ) then
	echo "comlist-W-tttoutofdate, ${dollar_sign}gtm_src/ttt.c was not generated from ${dollar_sign}gtm_tools/ttt.txt, " \
		"${dollar_sign}gtm_inc/opcode_def.h, and ${dollar_sign}gtm_inc/vxi.h" \
		>> $gtm_log/error.`basename $gtm_exe`.log
	echo "comlist-I-tttdiff, See differences in ${dollar_sign}gtm_obj/ttt.dif" \
		>> $gtm_log/error.`basename $gtm_exe`.log
	echo "comlist-I-tttgen, See ${dollar_sign}gtm_obj/ttt.c for C source file generated from ${dollar_sign}gtm_tools/ttt.txt" \
		>> $gtm_log/error.`basename $gtm_exe`.log
endif

set mupip_size = `ls -l $gtm_exe/mupip |awk '{print $5}'`
set gtmshr_size = `ls -l $gtm_exe/libgtmshr$gt_ld_shl_suffix |awk '{print $5}'`

if ( "$HOSTOS" != "SunOS" ) then
 	if ($mupip_size > $gtmshr_size) then
	echo "comlist-E-mupip_size, ${dollar_sign}gtm_dist/mupip is larger than ${dollar_sign}gtm_dist/libgtmshr$gt_ld_shl_suffix" >> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

cd $p3

# Create a default global directory.
setenv gtmgbldir ./mumps.gld
gde <<GDE.in1
exit
GDE.in1

# Create the GT.M help database file.
setenv gtmgbldir $gtm_dist/gtmhelp.gld
gde <<GDE.in_gtmhelp
Change -segment DEFAULT	-block=2048	-file=$gtm_dist/gtmhelp.dat
Change -region DEFAULT	-record=1020	-key=255
GDE.in_gtmhelp

mupip create

gtm <<GTM.in_gtmhelp
Do ^GTMHLPLD
$gtm_dist/mumps.hlp
Halt
GTM.in_gtmhelp


# Create the GDE help database file.
setenv gtmgbldir $gtm_dist/gdehelp.gld
gde <<GDE.in_gdehelp
Change -segment DEFAULT	-block=2048	-file=$gtm_dist/gdehelp.dat
Change -region DEFAULT	-record=1020	-key=255
GDE.in_gdehelp

mupip create

gtm <<GTM.in_gdehelp
Do ^GTMHLPLD
$gtm_dist/gde.hlp
GTM.in_gdehelp

chmod 775 *

# Create the dump file for ZHELP.
touch $gtm_dist/gtmhelp.dmp
chmod a+rw $gtm_dist/gtmhelp.dmp


comlist.END:

echo ""
echo ""
if ( -f $gtm_log/error.`basename $gtm_exe`.log ) then
	echo "Error summary:"
	echo ""
	cat $gtm_log/error.`basename $gtm_exe`.log
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

echo ""
echo "End of $0 `date`"

exit $comlist_status
