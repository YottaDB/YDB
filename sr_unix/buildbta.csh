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
##################################################################
#
#	buildbta.csh - Build bta images.
#
#	Argument:
#		$1 -	Version number or code (i.e., b, d, or p).
#
##################################################################

if ( $1 == "" ) then
	echo "buildbta-E-needp1, Usage: $shell buildbta.csh <version>"
	exit -1
endif

version $1 b
$shell $gtm_tools/buildbdp.csh $1 b $gtm_vrt/bta
exit $status
