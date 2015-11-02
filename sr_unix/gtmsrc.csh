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
###########################################################################################
#
#	gtmsrc.csh - release-specific definitions
#
#	gtmsrc.csh is an auxiliary shell script invoked by setactive.csh to define release-
#	specific environment variables and aliases for this release.  (The original version
#	of this file just defined release-specific source directories, hence the name.)
#	For most releases, this is straightforward, but incremental releases require the
#	specification of the release(s) upon which the current increment is based.
#
#	This script defines the following environment variables:
#
#		gtm_inc		- pathname of source directory containing C and assembly
#				  language header files (*.h and, usually, *.si)
#
#		gtm_pct		- pathname of source directory containing all of the GT.M
#				  sources for GDE and the percent utilities and the
#				  sources for the GT.M runtime help facility
#
#		gtm_src		- pathname of source directory containing the C and
#				  assembly language sources
#
#		gtm_tools	- pathname of source directory containing everything else
#				  (shell scripts and their corresponding input files, awk
#				  and sed programs, installation scripts, etc.) used in
#				  building or maintaining GT.M
#
#		gtm_{inc,pct,src,tools}_list - directory pathname or list of incremental
#				  directory pathnames
#
#			If this is a full release, each of these lists will contain only
#			one entry.
#
#			If this is an incremental release, each list will consist of
#			several directory pathnames:
#				the first will be the directory corresponding to this
#					version,
#				the second will be the directory corresponding to the
#					version on which this incremental version is based,
#				the third will be the directory corresponding to the
#					version on which the second version was based,
#				etc.
#
#		gt_as_option_I	- assembler option(s) specifying the location(s) of the
#				  assembly language header files (usually *.si)
#
#		gt_cc_option_I	- C compiler option(s) specifying the location(s) of the C
#				  header files (*.h)
#
###########################################################################################
#


setenv gtm_inc_list	"$gtm_vrt/inc"
setenv gtm_pct_list	"$gtm_vrt/pct"
setenv gtm_src_list	"$gtm_vrt/src"
setenv gtm_tools_list	"$gtm_vrt/tools"

# These shell variables are no longer used (they're now environment variables).  However, some older
# versions of gtmsrc.csh define them as shell variables and when both a shell variables and an
# environment variable of the same name exist, the shell variable value supersedes that of the
# environment variable.  Therefore, until all V3.1 versions are removed from the system, we need the
# following "unset" commands (remove after upgrading past V3.1.).
unset gtm_inc_list
unset gtm_pct_list
unset gtm_src_list
unset gtm_tools_list

if ( $?gtmsrc_last_exe == "1" ) then
	# Change path and gtmroutines to reflect new gtm_exe.
	set	path =			`echo $path        | sed -e "s|$gtmsrc_last_exe|$gtm_exe|"`

	# (Note: the use of an intermediate gtmroutines shell variable is necessary for csh; tcsh doesn't require it.)
	set	gtmsrc_gtmroutines =	`echo $gtmroutines | sed -e "s|$gtmsrc_last_exe|$gtm_exe|"`
	setenv	gtmroutines		"$gtmsrc_gtmroutines"
endif
setenv gtmsrc_last_exe	$gtm_exe

#	Copy to shell variables to make indexed selection possible.

set gtmsrc_inc_list   = ($gtm_inc_list)
set gtmsrc_pct_list   = ($gtm_pct_list)
set gtmsrc_src_list   = ($gtm_src_list)
set gtmsrc_tools_list = ($gtm_tools_list)

#	Set environment variables for this release only (doesn't include any of the
#	releases on which this is based -- even if this is an incremental release).
setenv gtm_inc		$gtmsrc_inc_list[1]
setenv gtm_pct		$gtmsrc_pct_list[1]
setenv gtm_src		$gtmsrc_src_list[1]
setenv gtm_tools	$gtmsrc_tools_list[1]

unsetenv gt_as_option_I
unsetenv gt_cc_option_I

setenv	gtm_version_change	`date`
source $gtm_tools/gtm_env.csh
unsetenv gtm_version_change

if (! $?gt_as_option_I) then
	setenv	gt_as_option_I	""
endif
foreach i ($gtm_inc_list)
	setenv	gt_as_option_I	"$gt_as_option_I -I$i"
end

if (! $?gt_cc_option_I) then
	setenv	gt_cc_option_I	""
endif
foreach i ($gtm_inc_list)
	setenv	gt_cc_option_I	"$gt_cc_option_I -I$i"
end

#	Clean up local shell variables.
unset gtmsrc_gtmroutines
unset gtmsrc_inc_list
unset gtmsrc_pct_list
unset gtmsrc_src_list
unset gtmsrc_tools_list
