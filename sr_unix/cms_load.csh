#! /usr/local/bin/tcsh
#################################################################
#								#
#	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

if( $# != 2 ) then
	echo ""
	echo "	Usage: $0 <cms-directory> <release-directory>"
	echo ""
	exit 5
endif

set cms_dir = $1
set dst_dir = $2

set dst_top_dir = $dst_dir:h
set dst_ver = $dst_dir:t
set cms_ver = $cms_dir:t

set platform_name = `uname | sed 's/-/_/g' | tr '[A-Z]' '[a-z]'`

unalias cp chmod mv ls grep
alias ls 'echo "ls \!:* 2>/dev/null" | sh'	# we want to redirect only stderr to /dev/null; can't do that in tcsh.
alias cp 'cp -f \!:* >& /dev/null'		# some copies may be null copies. we dont want error messages coming out.
alias mv 'mv -f \!:* >& /dev/null'		# some moves may be null moves. we dont want error messages coming out.
alias chmod 'chmod \!:* >& /dev/null'		# some chmods may be null chmods. we dont want error messages coming out.
if ($platform_name == "sunos") then
	alias grep /usr/xpg4/bin/grep		# for -E option to work on sparky
endif

set build_types        = "pro dbg bta"
set build_dirs         = "map obj"
set dir_structure      = "inc pct src tools log $build_types"

############# Define mapping between file-types and directory-name ##################

set gtm_src_types = "c m64 s msg"
set gtm_inc_types = "h max mac si"
set gtm_pct_types = "mpt m hlp"
set gtm_tools_types = "gtc sed awk sh csh list txt exp"

#####################################################################################

if !(-e $cms_dir) then
	echo "$cms_dir doesn't exist. Exiting..."
	exit 1
endif

if !(-e $dst_top_dir) then
	echo "$dst_top_dir doesn't exist. Exiting..."
	exit 1
endif

cd $dst_top_dir
if (-e $dst_ver) then
	foreach image (pro bta dbg)
		if (-e $gtm_root/$dst_ver/$image/gtmsecshr) then
			$gtm_com/IGS $gtm_root/$dst_ver/$image/gtmsecshr 1	/* stop gtmsecshr in case it is running */
			$gtm_com/IGS $gtm_root/$dst_ver/$image/gtmsecshr 2	/* reset gtmsecshr to be suid and root owned */
		endif
	end
	# Verify if anybody is using this version before deleting
	if ($platform_name == "linux") then
		set psopt = "-ef --width 300"		# to get more screen output have a 300 column screen
	else
		set psopt = "-ef"
	endif
	/bin/ps $psopt | grep "$dst_top_dir/$dst_ver/" | grep -vE "grep|$0" >& /dev/null
	if ($status == 0) then
		# This check does not cover all cases of usage. There is still a window where new processes might start.
		# But, this is better than not checking at all.
		echo "Following processes are still using $dst_ver; not deleting $dst_top_dir/$dst_ver"
		/bin/ps $psopt | grep "$dst_top_dir/$dst_ver/" | grep -vE "grep|$0"
		exit 1
	endif
	if ($dst_ver =~ V3* || $dst_ver =~ V4* || $dst_ver == "V990") then
		set move_args = "compulsory"
	endif
	if ($?move_args)  then
		set save_ver = `ls -ld ${gtm_root}/$dst_ver | awk '{if (length($7)==1) $7="0"_$7; time=$6"_"$7"_"$8; print toupper(time)}' | sed 's/://g'`
		echo "Renaming ${gtm_root}/${dst_ver} to $gtm_root}/${dst_ver}_${save_ver}"
		mv ${gtm_root}/$dst_ver ${gtm_root}/${dst_ver}_${save_ver}
	else
		echo "Deleting existing $dst_dir directory structure"
		rm -rf $dst_ver
		if ($status != 0) then
			exit $status
		endif
	endif
endif

############## Create $dst_dir and subdirectories ##################

echo "Creating -------> $dst_dir Directory Structure ..."
mkdir $dst_ver
if ($status != 0) then
	exit $status
endif
cd $dst_ver
set gtm_ver = `pwd`
if ($status != 0) then
	exit $status
endif
mkdir $dir_structure {`echo $build_types | sed 's/ /,/g'`}/{`echo $build_dirs | sed 's/ /,/g'`}
if ($status != 0) then
	exit $status
endif

cd $gtm_ver
cp $cms_dir/*/gtmsrc.csh .

############### Define platform-specific libraries ##################################

set gtm_s_aix   = "sr_port sr_port_cm sr_unix sr_unix_cm sr_unix_gnp sr_rs6000 sr_aix"
set gtm_s_osf1  = "sr_port sr_port_cm sr_unix sr_unix_cm sr_unix_gnp sr_alpha sr_dux"
set gtm_s_hp_ux = "sr_port sr_port_cm sr_unix sr_unix_cm sr_unix_gnp sr_hppa sr_hpux"
set gtm_s_linux = "sr_port sr_port_cm sr_unix sr_unix_cm sr_unix_gnp sr_i386 sr_linux"
set gtm_s_sunos = "sr_port sr_port_cm sr_unix sr_unix_cm sr_unix_gnp sr_sparc sr_sun"
set gtm_s_os390 = "sr_port sr_port_cm sr_unix sr_unix_cm sr_unix_gnp sr_s390 sr_os390"
set gtm_s_l390 = "sr_port sr_port_cm sr_unix sr_unix_cm sr_unix_gnp sr_l390 sr_linux"

set platform_library = "$platform_name"
if ( "s390" == $MACHTYPE && "linux" == $platform_library ) then
	set platform_library = "l390"
endif
if ( "z/OS" == $MACHTYPE ) then
	set platform_library = "os390"
endif

########### Copy sources from platform-specific directories into appropriate version-subdirectories ############

cd $cms_dir
set ref_libs = `set | grep "^gtm_s_${platform_library}[ 	]" | sed 's/^gtm_s_'${platform_library}'[ 	][ 	]*//g'`
foreach ref_library ( $ref_libs )
    if ( -d $ref_library ) then
	cd $ref_library
	foreach dir (src inc pct tools)
		foreach ftype (`set | grep "^gtm_${dir}_types[ 	]" | sed 's/^gtm_'$dir'_types[ 	][ 	]*//g'`)
			set nfiles=`ls -1 *.$ftype | wc -l | sed 's/^[ ]*//g'`
			if ($nfiles != 0) then
				echo "Copying $nfiles files of type .$ftype from $ref_library to ${gtm_ver}/${dir}"
				cp *.$ftype $gtm_ver/${dir}
			endif
			set nfiles=`ls -1 *.${ftype}nix | wc -l | sed 's/^[ ]*//g'`
			if ($nfiles != 0) then
				echo "Restoring $nfiles NIXed files of type .$ftype in directory ${gtm_ver}/${dir}"
				ls -1 *.${ftype}nix | awk '{printf "cp -f %s %s/%s\n", $1, '\"${gtm_ver}/${dir}\"', $1}' | sed 's/nix$//g' | sh
			endif
		end
	end
	cd ..
    else
	echo "Skipping missing library $ref_library"
    endif
end
cp sr_unix_cm/gtcm_gcore $gtm_ver/tools
cp sr_unix_cm/makefile* $gtm_ver/tools

######################## Rename .mpt files to _*.m files #######################

echo "Renaming .mpt files to _*.m in $gtm_ver/pct"
cd $gtm_ver/pct
ls -1 *.mpt | awk '{printf "mv %s _%s\n", $1, $1}' | sed 's/mpt$/m/g' | sh

######################## Edit release_name.h ####################################

if ($cms_ver != $dst_ver) then
	echo "Modifying release_name.h"
	$cms_tools/edrelnam.csh $dst_ver # Do we care if this fails?
endif

############## Set appropriate permissions on the files. For comments see $gtm_tools/comlist.csh ##############

set gtm_verno = $gtm_ver:t
switch ($gtm_verno)
	case "V990":
		set chmod_protect = 1
		breaksw
	case "V9*":
		set chmod_protect = 0
		breaksw
	default:
		set chmod_protect = 1
		breaksw
endsw
if ($chmod_protect == 1) then
        set chmod_conf = 755
        set chmod_src = 444
else
        set chmod_conf = 775
        set chmod_src = 664
endif
chmod 775 $gtm_ver
cd $gtm_ver
chmod $chmod_conf bta dbg pro inc pct src tools gtmsrc.csh
chmod 775 log
cd $gtm_ver/inc
chmod $chmod_src *
cd $gtm_ver/pct
chmod $chmod_src *
cd $gtm_ver/src
/bin/ls | xargs -n25 chmod $chmod_src
cd $gtm_ver/tools
chmod $chmod_src *
if ($chmod_protect} == 1 ) then
        chmod 555 *sh
else
        chmod 775 *sh
endif
echo ""
echo "Done"
echo ""
exit 0
