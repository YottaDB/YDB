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
	breaksw

case "[dD]*":
	set gt_ld_options = "$gt_ld_options_dbg"
	breaksw

case "[pP]*":
	set gt_ld_options = "$gt_ld_options_pro"
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

set aix_loadmap_option = ''
if ( $HOSTOS == "AIX") then
	set aix_loadmap_option = "-bcalls:$gtm_map/mumps.loadmap -bmap:$gtm_map/mumps.loadmap -bxref:$gtm_map/mumps.loadmap"
endif

gt_ld $gt_ld_options $gt_ld_ci_options $aix_loadmap_option ${gt_ld_option_output}$3/mumps -L$gtm_obj $gtm_obj/{gtm,mumps_clitab}.o $gt_ld_sysrtns \
		-lmumps -lgnpclient -lcmisockettcp $gt_ld_syslibs >& $gtm_map/mumps.map
if ( $status != 0  ||  ! -x $3/mumps ) then
	set buildshr_status = `expr $buildshr_status + 1`
	echo "buildshr-E-linkmumps, Failed to link mumps (see ${dollar_sign}gtm_map/mumps.map)" \
		>> $gtm_log/error.`basename $gtm_exe`.log
endif

# Note: gtm_svc should link with gtm_dal_svc.o before gtm_mumps_call_clnt.o(libgtmrpc.a) to
#       resolve conflicting symbols (gtm_init_1, gtm_halt_1 etc..) appropriately.
if ( $gt_ar_gtmrpc_name != "" ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/gtmsvc.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtm_svc -L$gtm_obj $gtm_obj/{gtm_svc,mumps_clitab,gtm_rpc_init,gtm_dal_svc}.o $gt_ld_sysrtns \
		-lmumps -lgnpclient -lcmisockettcp -L$gtm_exe -l$gt_ar_gtmrpc_name $gt_ld_syslibs >& $gtm_map/gtm_svc.map
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
