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
###################################################################################################
#
#
#	gtm_version_dirname.csh - convert version name to corresponding development directory name.
#
#	argument - GT.M version name (e.g., "v314")
#
#	The net effect of gtm_version_dirname.csh is to write to stdout the name of the directory
#	corresponding to the GT.M version name passed as an argument.
#
#
###################################################################################################

echo "$1" | sed -f $gtm_tools/gtm_version_dirname.sed
