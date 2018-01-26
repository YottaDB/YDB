#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2001-2015 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
###########################################################
#
#	buildshr.csh - Build GT.M mumps executable.
#
#	Arguments:
#		$1 -	version number or code
#		$2 -	image type (b[ta], d[bg], or p[ro])
#		$3 -	target directory
#
###########################################################

echo ""
echo "############# Linking MUMPS ###########"
echo ""
set buildshr_status = 0

source $gtm_tools/gtm_env.csh

set dollar_sign = \$
set mach_type = `uname -m`
set platform_name = `uname | sed 's/-//g' | tr '[A-Z]' '[a-z]'`

if ( $1 == "" ) then
	set buildshr_status = `expr $buildshr_status + 1`
endif

if ( $2 == "" ) then
	set buildshr_status = `expr $buildshr_status + 1`
endif

if ( $3 == "" ) then
	set buildshr_status = `expr $buildshr_status + 1`
endif


switch ($2)
case "[bB]*":
	set gt_ld_options = "$gt_ld_options_bta"
	set gt_image = "bta"
	breaksw

case "[dD]*":
	set gt_ld_options = "$gt_ld_options_dbg"
	set gt_image = "dbg"
	breaksw

case "[pP]*":
	set gt_ld_options = "$gt_ld_options_pro"
	set gt_image = "pro"
	breaksw

default:
	set buildshr_status = `expr $buildshr_status + 1`
	breaksw

endsw

version $1 $2
if ( $buildshr_status != 0 ) then
	echo "buildshr-I-usage, Usage: buildshr.csh <version> <image type> <target directory>"
	exit $buildshr_status
endif

set gt_ld_linklib_options = "-L$gtm_obj $gtm_obj/gtm_main.o -lmumps -lgnpclient -lcmisockettcp"
set nolibyottadb = "no"	# by default build libyottadb

if ($gt_image == "bta") then
	set nolibyottadb = "yes"	# if bta build, build a static mumps executable
endif

if ("OS/390" == $HOSTOS) then
	set exp = "x"
else
	set exp = "export"
endif
$shell $gtm_tools/genexport.csh $gtm_tools/yottadb_symbols.exp yottadb_symbols.$exp

# The below is used to generate an export file that is specific to executables. Typically used to export
# some symbols from utility progs like mupip, dse, lke etc

$shell $gtm_tools/genexport.csh $gtm_tools/ydbexe_symbols.exp ydbexe_symbols.$exp

if ($nolibyottadb == "no") then	# do not build libyottadb.so for bta builds
	# Building libyottadb.so shared library
	set aix_loadmap_option = ''
	set aix_binitfini_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = \
		"-bcalls:$gtm_map/libyottadb.loadmap -bmap:$gtm_map/libyottadb.loadmap -bxref:$gtm_map/libyottadb.loadmap"
		# Delete old yottadb since AIX linker fails to overwrite an already loaded shared library.
		rm -f $3/libyottadb$gt_ld_shl_suffix
		# Define gtmci_cleanup as a termination routine for libyottadb on AIX.
		set aix_binitfini_option = "-binitfini::gtmci_cleanup"
	endif

	set echo
	gt_ld $gt_ld_options $gt_ld_shl_options $aix_binitfini_option $gt_ld_ci_options $aix_loadmap_option \
		${gt_ld_option_output}$3/libyottadb$gt_ld_shl_suffix \
		${gt_ld_linklib_options} $gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/libyottadb.map
	@ exit_status = $status
	unset echo
	if ( $exit_status != 0 ) then
		@ buildshr_status++
		echo "buildshr-E-linkyottadb, Failed to link yottadb (see ${dollar_sign}gtm_map/libyottadb.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( ($HOSTOS == "Linux") && (-e /usr/bin/chcon) ) then
		# Successful build -- for Linux builds use chcon to enable usage of executable (later SELinux platforms)
		# Note that this command only works on filesystems that support context info so because it may fail,
		# (and if it does, it is irrelevent) we merrily ignore the output. It either works or it doesn't.
		chcon -t texrel_shlib_t $3/libyottadb$gt_ld_shl_suffix >& /dev/null
	endif
	if ($HOSTOS == "OS/390") then
		cp $gtm_obj/yottadb_symbols.$exp $3/
	endif
	set gt_ld_linklib_options = "-L$gtm_obj"	# do not link in mumps whatever is already linked in libyottadb.so
endif

# Building mumps executable
set aix_loadmap_option = ''
if ( $HOSTOS == "AIX") then
	set aix_loadmap_option = "-bcalls:$gtm_map/mumps.loadmap -bmap:$gtm_map/mumps.loadmap -bxref:$gtm_map/mumps.loadmap"
endif

set echo
gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/mumps ${gt_ld_linklib_options} $gtm_obj/gtm.o \
	$gt_ld_extra_libs $gt_ld_sysrtns $gt_ld_syslibs >& $gtm_map/mumps.map
@ exit_status = $status
unset echo
if ( $exit_status != 0  ||  ! -x $3/mumps ) then
	@ buildshr_status++
	echo "buildshr-E-linkmumps, Failed to link mumps (see ${dollar_sign}gtm_map/mumps.map)" \
		>> $gtm_log/error.`basename $gtm_exe`.log
else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
        if ( "dbg" == $gt_image ) then
                chatr +dbg enable +as mpas $3/mumps
	else
	        chatr +as mpas $3/mumps
        endif
endif

exit $buildshr_status
