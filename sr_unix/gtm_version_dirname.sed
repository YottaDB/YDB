#################################################################
#								#
#	Copyright 2002, 2003 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
#
##############################################################################################################################
#
#
# 	gtm_version_dirname.sed - sed script to convert a version name to the name of the corresponding development directory.
#
#	This script is intended to be used in-line by gtm_version_dirname.csh
#	which does some pre-formatting by converting the version name to
#	lower-case.
#
#
##############################################################################################################################

#	All characters should be lower-case on entry; convert leading "v"
#	and remove embedded decimal points (.) and minus signs (-).
s/^v/V/
s/\.//g
s/-//g

#	V9.9-BL99* Base-line release:		V99bl99* => V99BL99*
/^V[0-9][0-9][bB][lL][0-9][0-9]*$/s/[bB][lL]/BL/
/^V[0-9][0-9]BL[0-9][0-9]*$/q

#	V9.9-FT99* Field-test release:		V99ft99* => V99FT99*
/^V[0-9][0-9][fF][tT][0-9][0-9]*$/s/[fF][tT]/FT/
/^V[0-9][0-9]FT[0-9][0-9]*$/q

#	V9.9-FT99*x Field-test incremental release:		V99ft99*x => V99FT99*x	x == any lower-case alphabetic character
/^V[0-9][0-9][fF][tT][0-9][0-9]*[A-Za-z]$/s/[fF][tT]/FT/
/^V[0-9][0-9]FT[0-9][0-9]*[A-Za-z]*$/q

#	V9.9-99* Relatively major release:	V9999*   => V9999*	9* == any (possibly empty) sequence of numeric digits
/^V[0-9][0-9][0-9][0-9]*$/q

#	V9.9-99*x Incremental release:		V9999*x  => V9999*x	x == any lower-case alphabetic character
/^V[0-9][0-9][0-9][0-9]*[A-Za-z]$/q
