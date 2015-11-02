#################################################################
#								#
#	Copyright 2001, 2010 Fidelity Information Services, Inc	#
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

set buildshr_status = 0

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
	echo "buildshr-I-usage, Usage: $shell buildshr.csh <version> <image type> <target directory>"
	exit $buildshr_status
endif

set buildshr_verbose = $?verbose
set verbose
set echo

set gt_ld_linklib_options = "-L$gtm_obj $gtm_obj/gtm_main.o -lmumps -lgnpclient -lcmisockettcp"
set nolibgtmshr = "no"	# by default build libgtmshr

if ($gt_image == "bta") then
	set nolibgtmshr = "yes"	# if bta build, build a static mumps executable
endif

if ("OS/390" == $HOSTOS) then
	set exp = "x"
else
	set exp = "export"
endif
$shell $gtm_tools/genexport.csh $gtm_tools/gtmshr_symbols.exp gtmshr_symbols.$exp

# The below is used to generate an export file that is specific to executables. Typically used to export
# some symbols from utility progs like mupip, dse, lke etc

$shell $gtm_tools/genexport.csh $gtm_tools/gtmexe_symbols.exp gtmexe_symbols.$exp

if ($nolibgtmshr == "no") then	# do not build libgtmshr.so for bta builds
	# Building libgtmshr.so shared library
	set aix_loadmap_option = ''
	set aix_binitfini_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = \
		"-bcalls:$gtm_map/libgtmshr.loadmap -bmap:$gtm_map/libgtmshr.loadmap -bxref:$gtm_map/libgtmshr.loadmap"
		# Delete old gtmshr since AIX linker fails to overwrite an already loaded shared library.
		rm -f $3/libgtmshr$gt_ld_shl_suffix
		# Define gtmci_cleanup as a termination routine for libgtmshr on AIX.
		set aix_binitfini_option = "-binitfini::gtmci_cleanup"
	endif

	gt_ld $gt_ld_options $gt_ld_shl_options $aix_binitfini_option $gt_ld_ci_options $aix_loadmap_option \
		${gt_ld_option_output}$3/libgtmshr$gt_ld_shl_suffix \
		${gt_ld_linklib_options} $gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/libgtmshr.map
	if ( $status != 0 ) then
		set buildshr_status = `expr $buildshr_status + 1`
		echo "buildshr-E-linkgtmshr, Failed to link gtmshr (see ${dollar_sign}gtm_map/libgtmshr.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( ($HOSTOS == "Linux") && (-e /usr/bin/chcon) ) then
		# Successful build -- for Linux builds use chcon to enable usage of executable (later SELinux platforms)
		# Note that this command only works on filesystems that support context info so because it may fail,
		# (and if it does, it is irrelevent) we merrily ignore the output. It either works or it doesn't.
		chcon -t texrel_shlib_t $3/libgtmshr$gt_ld_shl_suffix >& /dev/null
	endif
	if ($HOSTOS == "OS/390") then
		cp $gtm_obj/gtmshr_symbols.$exp $3/
	endif
	set gt_ld_linklib_options = "-L$gtm_obj"	# do not link in mumps whatever is already linked in libgtmshr.so
endif

# Building mumps executable
set aix_loadmap_option = ''
if ( $HOSTOS == "AIX") then
	set aix_loadmap_option = "-bcalls:$gtm_map/mumps.loadmap -bmap:$gtm_map/mumps.loadmap -bxref:$gtm_map/mumps.loadmap"
endif

gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/mumps ${gt_ld_linklib_options} $gtm_obj/gtm.o \
	$gt_ld_extra_libs $gt_ld_sysrtns $gt_ld_syslibs >& $gtm_map/mumps.map

if ( $status != 0  ||  ! -x $3/mumps ) then
	set buildshr_status = `expr $buildshr_status + 1`
	echo "buildshr-E-linkmumps, Failed to link mumps (see ${dollar_sign}gtm_map/mumps.map)" \
		>> $gtm_log/error.`basename $gtm_exe`.log
else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
        if ( "dbg" == $gt_image ) then
                chatr +dbg enable +as mpas $3/mumps
	else
	        chatr +as mpas $3/mumps
        endif
endif

# Note: gtm_svc should link with gtm_dal_svc.o before gtm_mumps_call_clnt.o(libgtmrpc.a) to
#       resolve conflicting symbols (gtm_init_1, gtm_halt_1 etc..) appropriately.
if ( $gt_ar_gtmrpc_name != "" ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/gtmsvc.loadmap"
	endif
	# export gtm_filename_to_id and dependent modules from gtm_svc.
	gt_ld $gt_ld_options $gt_ld_options_all_exe $aix_loadmap_option ${gt_ld_option_output}$3/gtm_svc \
		-L$gtm_obj $gtm_obj/{gtm_svc,gtm_rpc_init,gtm_dal_svc}.o $gt_ld_sysrtns -lmumps -lgnpclient \
		$gt_ld_extra_libs  -lcmisockettcp -L$gtm_exe -l$gt_ar_gtmrpc_name $gt_ld_syslibs >& $gtm_map/gtm_svc.map
	if ( $status != 0  ||  ! -x $3/gtm_svc ) then
		set buildshr_status = `expr $buildshr_status + 1`
		echo "buildshr-E-linkgtm_svc, Failed to link gtm_svc (see ${dollar_sign}gtm_map/gtm_svc.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

unset echo
if ( $buildshr_verbose == "0" ) then
	unset verbose
endif

exit $buildshr_status
