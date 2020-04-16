#################################################################
#								#
# Copyright (c) 2001-2020 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
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
#		gtmroutines	- pathname for GT.M to lookup M sources and object files
#
#		gt_as_option_I	- assembler option(s) specifying the location(s) of the
#				  assembly language header files (usually *.si)
#
#		gt_cc_option_I	- C compiler option(s) specifying the location(s) of the C
#				  header files (*.h)
#
###########################################################################################
#
# This file is copied to $gtm_root/<version>/ only because V63011 and prior versions source it.
# Copying to $gtm_root/<version>/ can be avoided if there are no V63011 and prior versions.

# These env vars are set in setactive.csh. They are duplicated here only because V63011 and prior versions need it.
# These lines can be removed once there are no V63011 and prior versions.
setenv gtm_inc	"$gtm_ver/inc"
setenv gtm_pct	"$gtm_ver/pct"
setenv gtm_src	"$gtm_ver/src"
setenv gtm_tools	"$gtm_ver/tools"
####################################

if !($?gtmroutines) then
	setenv gtmroutines ""
endif
# Check for utf8 mode two ways, 1) by gtm_chset and 2) by active routines
set utf = ""
if ($?gtm_chset) then
	if ("UTF-8" == "$gtm_chset") set utf="/utf8"
else if ("utf8" == "$gtm_exe:t") then
	set utf="/utf8"
endif

# The only place where gtmsrc.csh is sourced is setactive.csh. The current setactive.csh simply sets $gtmroutines to "."
# But this "rebuild" of gtmroutines is necessary when sourcing setactive.csh of versions V63011 and earlier, which do
# not set gtmroutines to ".". This section can be removed once there are no V63011 and prior versions.
set rtns = ($gtmroutines:x)
if (0 < $#rtns) then
	@ rtncnt = $#rtns
	# Strip off "$gtm_exe/plugin/o($gtm_exe/plugin/r)" if present; assumption, it's at the end
	if ("$rtns[$rtncnt]" =~ "*/plugin/o*(*/plugin/r)") @ rtncnt--
	# Strip off "$gtm_exe"; assumption, it's next to last or the last
	if ($?gtmsrc_last_exe) then
		if ("${rtns[$rtncnt]:s;/utf8;;:s;*;;}" == "${gtmsrc_last_exe:s;/utf8;;:s;*;;}") @ rtncnt--
	endif
	setenv gtmroutines "$rtns[-$rtncnt]"
	unset rtncnt
else
	setenv gtmroutines "."
endif
if (-d $gtm_exe/plugin/o && -d $gtm_exe/plugin/r) then
	setenv gtmroutines "$gtmroutines $gtm_exe$utf $gtm_exe/plugin/o$utf($gtm_exe/plugin/r)"
else
	setenv gtmroutines "$gtmroutines $gtm_exe$utf"
endif
setenv gtmsrc_last_exe	$gtm_exe
unset rtns utf8

setenv	gtm_version_change	`date`
source $gtm_tools/gtm_env.csh
unsetenv gtm_version_change

if !($?gt_as_option_I) then
	setenv	gt_as_option_I	"-I$gtm_inc"
else
	setenv	gt_as_option_I	"$gt_as_option_I -I$gtm_inc"
endif
if !($?gt_cc_option_I) then
	setenv	gt_cc_option_I	"-I$gtm_inc"
else
	setenv	gt_cc_option_I	"$gt_cc_option_I -I$gtm_inc"
endif
