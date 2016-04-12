#!/usr/local/bin/tcsh
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


if ($# != 2) then
	echo ""
	echo "	Usage: $0 <c-struct-name> <c-src-filename>"
	echo ""
	exit
endif

###############################################################################################################
#
# This block of demarcated code initializes the environment variables and aliases used by the offset program.
# If this program is used anywhere outside the Greystone development environment, care should be taken in doing the following.
#
# (a) This utility offset.csh needs the shell "tcsh" to exist in /usr/local/bin (a soft link would suffice too).
# (b) This utility also needs an accompanying awk script "offset.awk" to do primitive parsing of the C file.
# (c) The environment variable "gtm_src" should be set to point to the absolute pathname containing the C-source-file.
# (d) The environment variable "gtm_tools" should be set to point to the absolute pathname containing the "offset.awk" program.
# (e) The alias "gt_cc" should be set to the command for the C-compiler with appropriate flags (including #defines).
# (f) The alias "gt_ld" should be set to the command for the C-linker (cc or ld would do) with appropriate flags.
# (g) The environment variable "user" should be set to the appropriate user-name. Usually this is taken care of by the shell.
#
# Note that this utility creates temporary files for its processing in the form /tmp__${user}_offset_*
# It removes these files only in the case the offset determination was successful.
# Therefore care should be taken to avoid unintended proliferation of these temporary files in the system.
#
# Once the above changes have been done in place here, comment out the greystone-environment-specific-initialization below.
#
switch ($gtm_exe:t)
	case "[bB]*":
		alias gtcc "`alias gt_cc_bta`"
		set gt_ld_options = "$gt_ld_options_bta"
		breaksw
	case "[dD]*":
		alias gtcc "`alias gt_cc_dbg`"
		set gt_ld_options = "$gt_ld_options_dbg"
		breaksw
	case "[pP]*":
		alias gtcc "`alias gt_cc_pro`"
		set gt_ld_options = "$gt_ld_options_pro"
		breaksw
	default:
		echo "Environment Variable gtm_exe should point to either 'pro' or 'bta' or 'dbg' only. Exiting..."
		exit -1
		breaksw
endsw

alias gt_ld $gt_ld_linker $gt_ld_options -L$gtm_obj $gt_ld_extra_libs $gt_ld_sysrtns $gt_ld_syslibs

###############################################################################################################

set c_struct = $1
set srcfile  = $2

set TMPFILE = /tmp/__${user}_offset_

if !(-e $gtm_src/$srcfile) then
	echo "OFFSET-E-INVALIDSRCFILE : $gtm_src/$srcfile doesn't exist. Please give a valid c-source-file-name. Exiting..."
	exit -1
endif

if !(-r $gtm_src/$srcfile) then
	echo "OFFSET-E-SRCRDERR : Can't read $gtm_src/$srcfile for compiling. Please ensure read permissions before reissuing the command. Exiting..."
	exit -1
endif

if ($srcfile:e != "c") then
	echo "OFFSET-E-INVALIDCFILE : $srcfile doesn't have a .c extension. Please give a valid c-source-file-name. Exiting..."
	exit -1
endif

(gtcc -E $gtm_src/$srcfile > ${TMPFILE}_$srcfile:r.lis) >& /dev/null
awk -v c_struct=${c_struct} -f $gtm_tools/offset.awk ${TMPFILE}_$srcfile:r.lis > ${TMPFILE}_$srcfile
gtcc ${TMPFILE}_$srcfile -o ${TMPFILE}_$srcfile.o >& /dev/null

if ($status != 0) then
	echo "OFFSET-E-SRCSTRUCTMISMATCH : Very likely that the c-structure isn't used in the c-source-file-name. Please give a valid input."
	echo "	If you feel that the input is valid, please contact the author for assistance in debugging the offset utility (See ${TMPFILE}_$srcfile:r* for details)"
	echo "	Exiting..."
	echo "-----------------------------------------------------------------------------------------"
	echo "###### Below is the output from the compiler ########"
	echo "-----------------------------------------------------------------------------------------"
	gtcc ${TMPFILE}_$srcfile -o ${TMPFILE}_$srcfile.o
	exit -1
endif

gt_ld ${TMPFILE}_$srcfile.o -o ${TMPFILE}_$srcfile:r.out >& /dev/null

${TMPFILE}_$srcfile:r.out
rm -f ${TMPFILE}_$srcfile:r.lis
rm -f ${TMPFILE}_$srcfile
rm -f ${TMPFILE}_$srcfile.o
rm -f ${TMPFILE}_$srcfile:r.out
