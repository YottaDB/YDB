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
###############################################################################
#
#	snapshot.csh - populate specified version with downloaded source files
#
#	One argument:
#		the version without punctuation (e.g., use "V316" for "V3.1-6")
#
###############################################################################

set snapshot_verbose = $?verbose
set verbose
set snapshot_version = `$shell $gtm_tools/gtm_version_dirname.csh $1`
version $snapshot_version p

if ( $gtm_verno != $snapshot_version ) then
	echo "snapshot - error: unable to set version properly for version $1"
	goto snapshot.END
endif

foreach snapshot_component ( inc pct src tools )
	cd $gtm_ver
	if ( -d $snapshot_component ) then
		rm -rf $snapshot_component
	endif
	mkdir $snapshot_component
	chmod 775 $snapshot_component
	cd $snapshot_component
	set snapshot_fcnt = `/bin/ls | wc`
	if ( $snapshot_fcnt[1] != 0 ) then
		echo "snapshot-E-notempty, $snapshot_component directory is not empty"
		goto snapshot.END
	endif
	cd $gtm_root/$snapshot_component
	/bin/ls | xargs -i cp {} $gtm_ver/$snapshot_component
end



snapshot.END:
set
if ( $snapshot_verbose == "0" ) then
	unset verbose
	unset snapshot_verbose
else
	unset snapshot_verbose
endif
unset snapshot_component
unset snapshot_fcnt
unset snapshot_version
