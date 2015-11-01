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
########################################################################
#
#
#	synch_env_version.csh - synchronize environment and GT.M version
#
#	This C shell script should be source'd any time the current
#	GT.M version changes.
#
#	On VMS, logicals may be defined transitively, e. g., GTM$DBG
#	can be defined once as GTM$VRT:[DBG] and whenever it's evaluated,
#	VMS will use the current value of GTM$VRT.
#
#	On Unix, this is not possible so, for example, we must remember
#	to update gtm_dbg to $gtm_vrt/dbg every time gtm_vrt changes.
#	Fortunately, versions are established and changed in only a
#	very few places and they can all source this shell script.
#
#
########################################################################

setenv	gtm_pro		$gtm_vrt/pro	# production version
setenv	gtm_bta		$gtm_vrt/bta	# beta test version
setenv	gtm_dbg		$gtm_vrt/dbg	# debug version

setenv	gtm_tools	$gtm_vrt/tools	# version-controlled GT.M csh scripts
