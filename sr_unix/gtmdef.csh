#################################################################
#								#
<<<<<<< HEAD
# Copyright 2001, 2011 Fidelity Information Services, Inc	#
#								#
# Copyright (c) 2017 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2017 Stephen L Johnson. All rights reserved.	#
=======
# Copyright (c) 2001-2020 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
>>>>>>> f33a273c... GT.M V6.3-012
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
##############################################################
#
#
#	gtmdef.csh - set up initial GT.M environment variables
#
#	This shell script corresponds to the VMS script
#	gtmdef.com which defines all of the system globals for
#	the Greystone GT.M development environment.  I.e.,
#	each of the following 'setenv' commands is intended to
#	be roughly equivalent to the VMS 'define/system'
#	command.
#
#	To duplicate this effect as much as possible, this
#	script should be invoked as soon as possible after
#	shell startup (see gtm_cshrc.csh).
#
#
##############################################################

# The below are defined for backward compatibility. Can be removed once V63011 and prior versions are gone
setenv	gtm_gtmdev 	"/usr"
setenv	gtm_topdir	library
###############################

if (! $?gtm_root) setenv gtm_root /usr/library	# location of development directory tree
setenv	gtm_com		$gtm_root/com		# location of GT.M csh scripts

source	$gtm_com/versions.csh			# establish the environment variables:
						#	gtm_curpro (current production release name)
						#	gtm_verno (current active release name)

set vernam=$gtm_root/$gtm_verno

setenv	gtm_ver		$vernam
<<<<<<< HEAD
setenv	gtm_vrt		$vernam			# /translate=(concealed) on VMS

setenv	gtm_tools	$gtm_vrt/tools
setenv	gtm_inc		$gtm_vrt/inc

####### source	$gtm_tools/synch_env_version.csh	# set up dependent environment variables

=======
setenv	gtm_vrt		$gtm_ver		# Set only because prior versions need it. Should not be used post V63011
setenv	gtm_tools	$gtm_ver/tools
setenv	gtm_inc		$gtm_ver/inc
setenv	gtm_pro		$gtm_ver/pro		# production version
setenv	gtm_bta		$gtm_ver/bta		# beta test version
setenv	gtm_dbg		$gtm_ver/dbg		# debug version
setenv	gtm_tools	$gtm_ver/tools		# version-controlled GT.M csh scripts
>>>>>>> f33a273c... GT.M V6.3-012
setenv	gtm_exe		$gtm_dbg		# the active version (initially debug)
setenv	gtmsrc_last_exe	$gtm_exe		# Set only because prior versions need it. Should not be needed post V63011

setenv	gtmroutines	". $gtm_exe"

