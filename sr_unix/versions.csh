#################################################################
#								#
#	Copyright 2001, 2013 Fidelity Information Services, Inc	#
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
if (`uname -s` != "OS/390") then
	setenv	gtm_curpro	"V60001"
else
	setenv	gtm_curpro	"V53004A"	# until newer version built on z/OS
endif

#	gtm_verno is the current production version
setenv	gtm_verno	$gtm_curpro
