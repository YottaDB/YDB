#################################################################
#                                                               #
#       Copyright 2007, 2008 Fidelity Information Services, Inc #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################
# Most of the logic in this script is similar to its counter-part on sr_ia64
# Any changes or bugfixes in this files should be updated in its couterpart on sr_ia64


# This script is called from two places. If it is called from comlist.mk,it takes source directories as arguments.
# else it is called from comlist.csh with no arguments.
if ( $#argv != 0 ) then
	set builds=$buildtypes
	set numbuilds=$#builds
	if ( 1 != $numbuilds ) then
		echo "sr_x86_64/gen_xfer_desc.csh-E-2many: only one buildtype at a time allowed"
		exit 2
	endif
	cd $gtm_ver/$buildtypes/obj
	set lib_count=$#argv
	set ref_libs=""

	while ( $lib_count != 0 )
		set ref_libs="$ref_libs $argv[$lib_count]"
		@ lib_count--
	end

	set gtm_src_types = "c m64 s msg"
	set gtm_inc_types = "h max mac si"
	set xfer_dir=`pwd`

	if (-e src) then
		\rm -rf src
	endif

	if (-e inc ) then
		\rm -rf inc
	endif
# Create temporary directories called src and inc
	mkdir src inc

	pushd $gtm_ver

# Following "foreach" logic comes from cms_load.csh
	foreach ref_library ( $ref_libs )
		cd $ref_library
	        foreach dir (src inc)
	                foreach ftype (`set | grep "^gtm_${dir}_t" | sed 's/^gtm_'$dir'_types[  ][      ]*//g'`)
	                        set nfiles = `\ls -1 | grep "\.$ftype"'$' | wc -l | sed 's/^[ ]*//g'`
	                        if ($nfiles != 0) then
#creates the links for all specific  files in src and inc directory.
	                                \ls -1 | grep "\.$ftype"'$' | xargs -i ln -f -s "$PWD/{}" $xfer_dir/${dir}
	                        endif
	                end
	        end
		cd ..
	end

	popd
	setenv gtm_src `pwd`/src
	setenv gtm_inc `pwd`/inc
	setenv gt_cc_option_I "$gt_cc_option_I -I$gtm_inc"
	rm -rf $xfer_dir/xfer_desc.i
else
#	set xfer_desc.i path to $gtm_inc in normal build
	set xfer_dir=$gtm_inc

# If this is a non-developmental version and the current image is "dbg" and xfer_desc.i already exists, do not recreate xfer_desc.i.
# The assumption is that a "pro" build had already created xfer_desc.i so we should not change whatever it had relied upon.
# For development versions, we dont care so we unconditionally recreate this file.

	if (-e $xfer_dir/xfer_desc.i) then
		if ($gtm_verno !~ V9*) then
			echo "GENXFERDESC-I-EXIST : xfer_desc.i already exists for production version $gtm_verno. Not recreating."
			exit 0
		else
			echo "GENXFERDESC-I-EXIST : xfer_desc.i already exists for development version $gtm_verno. Recreating."
			chmod +w $xfer_dir/xfer_desc.i	# in case previous build had reset permissions to be read-only
			rm -f $xfer_dir/xfer_desc.i
		endif
	endif
endif

cd $gtm_src

cat << TEST >! temp_xyz_ia.c

#include "mdef.h"

#define XFER(a,b) MY_XF,b

#include "xfer.h"

TEST

$gt_cc_compiler $gt_cc_option_I -E temp_xyz_ia.c >! temp_xyz_ia.1
cat temp_xyz_ia.1 | grep MY_XF | cut -f2 -d"," >! temp_xyz_ia.2

echo "#include <stdio.h>" >! $xfer_dir/xfer_desc.i
echo "#define C 1" >> $xfer_dir/xfer_desc.i
echo "#define ASM 2" >> $xfer_dir/xfer_desc.i
echo "#define C_VAR_ARGS 3" >> $xfer_dir/xfer_desc.i

echo "char xfer_table_desc[] = {" >> $xfer_dir/xfer_desc.i

foreach name (`cat temp_xyz_ia.2`)
	set name2 = `grep "^$name" *.s`
	if (-r ${name}.s) then
		echo "ASM, /* $name */" >> $xfer_dir/xfer_desc.i
	else if (-r ${name}.c) then
		grep $name $gtm_inc/* | grep "\.\.\." >> /dev/null
		if ( $? == 0 ) then
			echo "C_VAR_ARGS, /* $name */" >> $xfer_dir/xfer_desc.i
		else
			echo "C, /* $name */" >> $xfer_dir/xfer_desc.i
		endif
	else if ("${name2}" != "") then
		echo "ASM, /* $name */" >> $xfer_dir/xfer_desc.i
	else if ("${name2}" == "") then
		echo "C_VAR_ARGS, /* $name */" >> $xfer_dir/xfer_desc.i
	endif
end
echo "0};" >> $xfer_dir/xfer_desc.i

\rm temp_xyz_ia.*

if ($#argv != 0) then
	cd $xfer_dir
	\rm -rf src
	\rm -rf inc
endif

exit 0

