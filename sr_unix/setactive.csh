#################################################################
#								#
#	Copyright 2001, 2009 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
#######################################################################################################################
#
#	setactive.csh - set active GT.M version (aliased to "version")
#
#	Because setactive must be source'd, it cannot take any arguments (in csh).  We get around this by requiring the
#	shell variable setactive_parms to be set to a (possibly empty) list:
#
#	setactive_parms[1] - version
#			"A" or "a" => use (current) active version (i. e., don't change the version)
#			"D" or "d" => use current development (volatile) version
#			"P" or "p" => use current production (fully tested) version
#			omitted - if argument 2 specified, this is the same as "A" or "a"
#
#	setactive_parms[2] - type of binaries
#			"B" or "b" => use bta (beta) binaries (optimized with asserts but without debugger information)
#			"D" or "d" => use dbg (debug) binaries (unoptimized with asserts and debugger information)
#			"P" or "p" => use pro (production) binaries (optimized without asserts or debugger information)
#
#	If both arguments are omitted:
#			In an interactive shell, setactive will prompt for each argument.
#			In a non-interactive shell, setactive will not change anything (no-op).
#
#######################################################################################################################


set setactive_save_verbose = $?verbose
set exit_status = 0
unset verbose

set setactive_p1 = ""
set setactive_p2 = ""

if ( $#setactive_parms >= 1 ) then
	set setactive_p1 = $setactive_parms[1]
endif
if ( $#setactive_parms >= 2 ) then
	set setactive_p2 = $setactive_parms[2]
endif

set setactive_setenv = "/tmp/setactive.${USER}_$$.setenv"
if (-e $setactive_setenv) \rm -f $setactive_setenv


if ( $?prompt == "1" ) then
	set setactive_interact = 1
	if ($?gtm_ver_noecho == 0) echo ""
else
	set setactive_interact = 0
endif

$shell -f $gtm_tools/setactive1.csh "$setactive_p1" "$setactive_p2" $setactive_interact $setactive_setenv
set setactive_status = $status
if ($setactive_status) @ exit_status++

# This is needed to ensure the current values of these environment variables don't persist past the
# invocation of 'setactive_setenv' and 'gtmsrc.csh' in case the version to which we are changing
# does not contain appropriate code to reset them (prior to V3.2).  This comment and the code following
# it can (and probably should) be removed once we remove all V3.1 versions from the system.
unsetenv	gtm_inc_list
unsetenv	gtm_pct_list
unsetenv	gtm_src_list
unsetenv	gtm_tools_list
unsetenv	gtm_log
unsetenv	gtm_map
unsetenv	gtm_obj

if ( $setactive_status == 0 ) then
	source $setactive_setenv
	if ($status) @ exit_status++
	source $gtm_ver/gtmsrc.csh
	if ($status) @ exit_status++
endif


FINI:
# Clean up local shell variables and files:
if ( -f $setactive_setenv ) then
	\rm $setactive_setenv
endif

unset setactive_interact
unset setactive_parms
unset setactive_p1
unset setactive_p2
unset setactive_setenv
unset setactive_status

if ( "$setactive_save_verbose" == "1" ) then
	unset setactive_save_verbose
	set verbose
else
	unset setactive_save_verbose
	unset verbose
endif

exit $exit_status
