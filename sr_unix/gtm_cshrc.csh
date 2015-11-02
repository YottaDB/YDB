#################################################################
#								#
#	Copyright 2001, 2011 Fidelity Information Services, Inc	#
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
#
#	gtm_cshrc.csh - csh/tcsh startup script
#
#	gtm_cshrc.csh sets up the GT.M development root directory, verifies the shell
#	script version consistency, and invokes other scripts to complete the
#	environment setup.
#
#####################################################################################


# Make sure the tcsh environment variables are set properly (even if not running tcsh):

if ( $?HOSTOS   == "0" )	setenv HOSTOS	`uname -s`	# operating system
if ( $?MACHTYPE == "0" )	setenv MACHTYPE	`uname -m`	# hardware type

# Be careful not to re-initialize all of the environment variables for each subshell
# (i.e., don't undo what the last invocation of the version command set up).

set	gtm_cshrc_first_time =	"false"

if ( $?gtm_environment_init == "0" ) then

	set	gtm_cshrc_first_time =	"true"		# for gtm_env.csh (see below)
	setenv	gtm_environment_init	"GT.M environment initialized at `date`"
	if ( $HOSTOS != "OS/390" ) then
		setenv	gtm_root		'/usr/library'
	else
		setenv	gtm_root		'/gtm/library'
	endif
	setenv	gtm_com			$gtm_root/com

	if ( ! -f $gtm_com/gtm_cshrc.csh )  then
		# This is highly unlikely because that's where this file lives and where it must be executed!
		echo "gtm_cshrc-E-nogtm_cshrc, There is no gtm_cshrc.csh in $gtm_com"	# I am not here!
	endif

	if ( -f $gtm_com/gtmdef.csh )  then
		source	$gtm_com/gtmdef.csh	# initialize non-version-specific GT.M environment variables
	else
		echo "gtm_cshrc-E-nogtmdef, There is no gtmdef.csh in $gtm_com"
	endif

	if (! -f $gtm_com/versions.csh )  then
		echo "gtm_cshrc-E-noversions, There is no versions.csh in $gtm_com"
	endif


	if (-d $gtm_root/V990) then
		set errmsg = "Development ($gtm_root/V990/tools) and installed ($gtm_com) versions of"
		if ( -f $gtm_com/gtm_cshrc.csh ) then	# this test should be unnecessary at this point
			diff {$gtm_com/,$gtm_root/V990/tools/}gtm_cshrc.csh >& /dev/null
			if ( $status != 0 ) then
				echo "gtm_cshrc-W-gtm_cshrc_mismatch, $errmsg gtm_cshrc.csh are different."
			endif
		endif

		if ( -f $gtm_com/gtmdef.csh ) then	# this test should be unnecessary at this point
			diff {$gtm_com/,$gtm_root/V990/tools/}gtmdef.csh >& /dev/null
			if ( $status != 0 ) then
				echo "gtm_cshrc-W-gtmdef_mismatch, $errmsg gtmdef.csh are different."
			endif
		endif

		if ( -f $gtm_com/versions.csh )  then
			diff {$gtm_com/,$gtm_root/V990/tools/}versions.csh >& /dev/null
			if ( $status != 0 ) then
				echo "gtm_cshrc-W-versions_mismatch, $errmsg versions.csh are different."
			endif
		endif
	endif
endif

if ( $gtm_cshrc_first_time == "true" ) then
	# If it's the first time, we need to initialize all of the environment variables.
	# Otherwise, we just need to re-specify the aliases.
	setenv	gtm_version_change	`date`
endif

if ( -f $gtm_tools/gtm_env.csh ) then
	source $gtm_tools/gtm_env.csh	# version-controlled definitions and aliases
endif

if ( $gtm_cshrc_first_time == "true" ) then
	unsetenv gtm_version_change
endif

unset	gtm_cshrc_first_time
