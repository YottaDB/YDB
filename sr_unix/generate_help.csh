#################################################################
#								#
#	Copyright 2014 Fidelity Infromation Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
# (re)Generate GT.M and Utility Help global directories and files on demand
#
# Parameters:
#   HLP file location (defaults to $gtm_pct)
#   Error log file (used to redirect output to error file in comlist.csh)


set hlpdir = $1
if ("" == "${hlpdir}") then
	if (0 == $?gtm_pct) then
		echo "HLP file location was not supplied and \$gtm_pct is not defined"
		exit -1
	endif
	set hlpdir = ${gtm_pct}
	if (! -e ${hlpdir}) then
		echo "HLP file location does not exist"
		exit -2
	endif
endif

set errout = ""
if ("" != "${2}") set errout = ">> $2"

# Need write permissions to $gtm_dist
if (! -w ${gtm_dist}) then
	set restorePerms = `filetest -P $gtm_dist`
	chmod ugo+w ${gtm_dist}
	if ($status) then
		echo "User does not have sufficient privileges to get write access to $gtm_dist, cannot update help"
		exit -3
	endif
endif

set script_stat = 0
foreach hlp (${hlpdir}/*.hlp)
	# Extract the HLP file name and fix-up the mumps to gtm
	set prefix=${hlp:t:r:s/mumps/gtm/}

	# If the HLP files are newer than the help database create a new one, otherwise skip it
	if ( `filetest -C ${hlp}` > `filetest -C $gtm_dist/${prefix}help.dat` ) then
		\rm -f  ${gtm_dist}/${prefix}help.gld ${gtm_dist}/${prefix}help.dat
	else
		continue
	endif

	# Either help info does not exist or needs to be regenerated

	# Define the global directory with the same prefix as the HLP file and
	# use ${gtm_dist} in the file name to ensure dynamic lookup of the DAT
	# for help information
	setenv gtmgbldir ${gtm_dist}/${prefix}help.gld
	${gtm_dist}/mumps -run GDE <<GDE_in_help
Change -segment DEFAULT	-block=2048	-file=\$gtm_dist/${prefix}help.dat
Change -region DEFAULT	-record=1020	-key=255
GDE_in_help

	if ($status) then
		@ script_stat++
		echo "genreatehelp-E-hlp, Error creating GLD for ${hlp}" $errout
		continue
	endif

	${gtm_dist}/mupip create

	if ($status) then
		@ script_stat++
		echo "genreatehelp-E-hlp, Error creating DAT for ${hlp}" $errout
		continue
	endif

	${gtm_dist}/mumps -direct <<GTM_in_gtmhelp
Do ^GTMHLPLD
${hlp}
Halt
GTM_in_gtmhelp

	if ($status) then
		@ script_stat++
		echo "genreatehelp-E-hlp, Error while processing ${hlp}" $errout
		continue
	endif
end

# Restore read-only status
if ($?restorePerms) then
	chmod ${restorePerms} ${gtm_dist}
endif

exit ${script_stat}

