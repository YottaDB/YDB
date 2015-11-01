#! /usr/local/bin/tcsh -fv
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

#######################################################################
#
# Run GTM procedure allrefs.m with parms
#
#######################################################################

# Set global directory to cross reference database

if ("$1" == "") then
	echo ""
	echo "$0 {variable | function | macro | ...}"
	echo ""
	exit 1
endif

setenv gtm_xrefs $gtm_ver/misc

setenv gtmgbldir $gtm_xrefs/mxref.gld
setenv gtmroutines "$gtm_xrefs($gtm_xrefs $gtm_dist)"
alias gtm $gtm_exe/mumps -direct

# Execute GTM command

gtm << GTM_EOF
d ^allrefs("$1")
h
GTM_EOF
