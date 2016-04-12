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

if ("OS/390" != `uname -s`) then
	setenv	gtm_gtmdev 	"/usr"		# List of directories containing subdirectories named gtm_topdir which
else
	setenv	gtm_gtmdev 	"/gtm"		# List of directories containing subdirectories named gtm_topdir which
endif
						# may contain GT.M releases.  "NULL" => subdirectory is home of library
set	gtm_gtmdev_lcl = ( $gtm_gtmdev )	# Note: this must be a shell variable in order to use subscripts
						# (see below).

setenv	gtm_topdir	library			# name of top subdirectory that may contain release subdirectories

setenv	gtm_root	$gtm_gtmdev_lcl[1]/$gtm_topdir	# location of development directory tree
setenv	gtm_com		$gtm_root/com		# location of GT.M csh scripts

source	$gtm_com/versions.csh			# establish the environment variables:
						#	gtm_curpro (current production release name)
						#	gtm_verno (current active release name)

set vernam=$gtm_gtmdev_lcl[1]/$gtm_topdir/$gtm_verno

#	Note that gtm_ver and gtm_vrt have identical values on Unix.
#	On VMS one is a "normal" logical value and the other is "concealed".
setenv	gtm_ver		$vernam
setenv	gtm_vrt		$vernam			# /translate=(concealed) on VMS

setenv	gtm_tools	$gtm_vrt/tools
setenv	gtm_inc		$gtm_vrt/inc

source	$gtm_tools/synch_env_version.csh	# set up dependent environment variables

setenv	gtm_exe		$gtm_dbg		# the active version (initially debug)
setenv	gtmsrc_last_exe	$gtm_exe		# initial value for use by gtmsrc.csh

#
# 	establish default gtmroutines
#
setenv	gtmroutines	". $gtm_exe"

