#################################################################
#								#
#	Copyright 2001, 2007 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
############################################
#
#
#	versions - set default GT.M versions
#
#
############################################

#	gtm_curpro is the current production version
if (`uname -m` == "ia64") then
	# On Itanium (Linux or HPUX) it is V53FT00 (the first clean build on this platform)
	setenv	gtm_curpro	"V53FT00"
else
	setenv	gtm_curpro	"V51000" # V52000,A,B cannot be gtm_curpro due to $gtm_inc_list issue in gtm_env.csh/gtmsrc.csh
endif

#	gtm_verno is the current production version
setenv	gtm_verno	$gtm_curpro
