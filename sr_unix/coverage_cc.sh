#!/bin/sh
#################################################################
#								#
# Copyright (c) 2021 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

##
# @file
#
# @brief Replace the standard GT.M compiler with a coverage enabled one
#
# Our purpose here in this script is to be a drop-in C compiler (for linux/gcc only)
# which can be the object of the gt_cc_compiler environment variable.
#
# If we are invoked, our only purpose in life is to add the "--coverage"
# and supporting flags to compiles, and the "-lgcov" flag to loads, *except* for certain
# exceptional files (forex: tmpCFile2.c from scantypedefs.m)
#
# Since all our gcc versions on hosts participating in a coverage run must be
# the same, and since we originally used some advanced coverage features, we standardize
# on gcc-10.2.0 for our underlying compiler here.  Since this is not the default on
# any of our hosts, we use a set-aside directory to make sure we get the correct version.
# If we can't find a compliant compiler, we just fall back to an unuseful, but
# harmless regular run.

##
# @brief Main driver for the compiler script
#
# We run through the command line here, deciding if we are doing a coverage compile or now.
# If we are, and the system supports it, we massage argv so as to add the needed flags to
# enable coverage mode in GCC and pull in libgcov at link time.
#
# @return We will generally exit with the exit status of the underlying compiler
#
main () {
	real_compiler="gcc"
	can_profile=1
	# We assume we are run from /usr/library/VXXX/dbg/obj
	#obj_dir=`pwd`
	#build=`echo $obj_dir | sed -e 's?/usr/library/??' -e 's?/.*$??'`

	# First, determine if we are compile or link invocation of the compiler
	# and if we are doing a DEBUG build. (We only add coverage flags to a debug
	# build)
	is_debug_build=0
	is_compile=0
	is_link=0
	is_stop_list=0

	# List of C files for which we do not want to turn on --coverage
	stop_list="tmpCFile.c tmpCFile2.c gtm_threadgbl_deftypes.c"

	# Peek at our command line arguments to guess what we are doing
	for arg
	do
		if [ $arg = "-DDEBUG" ]; then
			is_debug_build=1
		fi

		# We are a debug link if we see something like "-L/usr/library/V972/dbg/obj"
		case $arg in
			-L*dbg/obj)
				is_debug_build=1
				;;
		esac

		if [ $arg = "-std=c99" ]; then
			is_compile=1
		fi

		if [ $arg = "-lrt" ]; then
			is_link=1
		fi

		for stop_file in $stop_list
		do
			case $arg in
				$stop_file)
					is_stop_list=1
					;;
				*/${stop_file})
					is_stop_list=1
					;;
			esac
		done
	done

	# If we are not linux, not doing a debug build, or if we are on the stop list punt to the original compiler/options
	if [ \( $OSTYPE != "linux" \) -o \( $is_debug_build -eq 0 \) -o \( $is_stop_list -eq 1 \) ]; then
		exec $real_compiler $*
	fi

	# We are doing a debug build.  Can our default compiler profile at the pid-separated level?

	# Find the gcc version, gcc --version will return something like
	# gcc (Ubuntu 7.5.0-3ubuntu1~18.04) 7.5.0
	# and we always care only about the first line/last field
	#
	gcc_version=`$real_compiler --version | head -1 | sed -e 's/^.* //'`

	# This section used to look for gcc >= 10, but it turns out that the
	# profiling data is *very* version specific, so we must use gcc-10.2.0.
	# If that is not the system gcc (and it won't be on any current linux box of ours)
	# then we have to check to see if we have installed 10.2.0 on the side and
	# use it. If not, we just skip profiling and do a normal compile, que sera, sera.
	if [ $gcc_version != "10.2.0" ]; then
		can_profile=0
		if [ -x "/usr/local/cluster/gcc-10.2.0/bin/gcc" ]; then
			export PATH="/usr/local/cluster/gcc-10.2.0/bin:$PATH"
			real_compiler=gcc
			can_profile=1
		fi
	fi

	# Overlay ourselves and punt to a standard compile.
	if [ $can_profile -ne 1 ]; then
		exec $real_compiler $*
	fi

	# It is standard practice for automated (ie: not developer driven) test runs to be compiled as
	# 'library' and run as 'gtmtest'.  Therefore, if we are running as library, we want to adjust
	# things so gcda files land under gtmtest.
	runtime_user=$USER
	if [ $USER = "library" ]; then
		runtime_user="gtmtest"
	fi

	# If we get here, we are building DEBUG & can profile
	# GCOV_PREFIX_STRIP lets us move the GCDA files easily.
	# (Re)Creating the gcda output dir and its ACL here is not ideal,
	# but if we don't, it's possible some GCDA files will be created during
	# build with a non 666 mode and might be unwritable by different userids in
	# a later test.
	export GCOV_PREFIX_STRIP=5

	# We go ahead and create this directory here because later parts of
	# the build process run mumps code so we can't really leave it to the
	# test system.  I think the gcc runtime would create it, but would not get the ACL
	# created, which could leave us with unwritable .gcda files later.
	# We use a timestamp on the output dir so if the build changes, it doesn't write to the wrong version
	time_stamp=`ls -ld /usr/library/$gtm_verno | \
		gawk '{time=sprintf("%s_%02d_%s",toupper($6),$7,$8); gsub(":","",time); print time}'`
	profile_dir="/testarea1/${runtime_user}/coverage/${gtm_verno}_${time_stamp}/gcda"
	if [ ! -d $profile_dir ]; then
		mkdir -p $profile_dir
	fi
	chmod ugo+rx $profile_dir

	# we need ACLs because tests run as a number of different users.  If this fails, punt
	# to standard compile. (This is our last chance to punt because after this we start fiddling argv)
	# We trash stderr because there's no point in punting to standard compile if we are going to break
	# the build with an error message anyway.
	if setfacl -d -m other::rw -m user::rw -m group::rw $profile_dir 2> /dev/null; then
		:
	else
		exec $real_compiler $*
	fi

	# Patch argv to include coverage options.  Originally the gcda files were generated
	# in /usr/library/VXXX/dbg/obj, but that was blowing up /usr/library, so now they
	# are generated in /testarea1/$runtime_user/coverage/gcda.
	# The profile-prefix-path keeps the files from being renamed with weird '#' escapes.
	argv="$*"
	if [ \( $is_compile -eq 1 \) -a \( $is_debug_build -eq 1 \) -a \( $is_stop_list -eq 0 \) ]; then
		argv="-fprofile-prefix-path=/usr/library/${gtm_verno}/dbg/obj $argv"
		argv="-fprofile-dir=${profile_dir} $argv"
		argv="--coverage $argv"
	fi

	# If we are linking, we can always add -lgcov and this will be
	# harmless if none of the objects need those symbols
	if [ $is_link -eq 1 ]; then
		argv="$argv -lgcov"
	fi

	# Now that we have edited our command line, run it with the real compiler
	exec $real_compiler $argv
}

main $*
