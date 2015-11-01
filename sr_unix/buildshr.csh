#################################################################
#								#
#	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	#
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

if ( $gt_ar_gtmrpc_name == "" ) then
	set gt_ld_gtmrpc_library_option = ""
else
	# Note: libgtmrpc.a is in $gtm_exe because it's part of the release.
	set gt_ld_gtmrpc_library_option = "-L$gtm_exe -l$gt_ar_gtmrpc_name"
endif


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
	set aix_loadmap_option = "-bloadmap:$gtm_map/mumps.loadmap"
endif

gt_ld $gt_ld_options $gt_ld_ci_options $aix_loadmap_option ${gt_ld_option_output}$3/mumps -L$gtm_obj $gtm_obj/{gtm,mumps_clitab}.o $gt_ld_sysrtns \
		-lmumps -lgnpclient -lcmisockettcp $gt_ld_gtmrpc_library_option $gt_ld_syslibs >& $gtm_map/mumps.map
if ( $status != 0  ||  ! -x $3/mumps ) then
	set buildshr_status = `expr $buildshr_status + 1`
	echo "buildshr-E-linkmumps, Failed to link mumps (see ${dollar_sign}gtm_map/mumps.map)" \
		>> $gtm_log/error.`basename $gtm_exe`.log
endif

# gtmrpc needs to be linked twice :-
# once before -lmumps since
#	gtm_svc needs to link 'restart' from libgtmrpc.a instead of libmumps.a
# once after -lmumps because
# 	getmaxfds which is defined in libgtmrpc.a but referenced only in libmumps.a.
#
if ( $gt_ar_gtmrpc_name != "" ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/gtmsvc.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtm_svc -L$gtm_obj $gtm_obj/{gtm_svc,mumps_clitab}.o $gt_ld_sysrtns \
		$gt_ld_gtmrpc_library_option -lmumps $gt_ld_gtmrpc_library_option  -lgnpclient -lcmisockettcp $gt_ld_syslibs >& $gtm_map/gtm_svc.map
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
