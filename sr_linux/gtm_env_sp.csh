#################################################################
#								#
# Copyright (c) 2001-2017 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
#
##########################################################################################
#
#	gtm_env_sp.csh - environment variable values and aliases specific to Linux
#
##########################################################################################
#


# Once the environment variables have been initialized, be careful not to
# re-initialize them for each subshell (i.e., don't undo any explicit changes that
# have been made since the last version change).

set platform_name = `uname | sed 's/-//g' | tr '[A-Z]' '[a-z]'`
set mach_type = `uname -m`

### Sanitize platform_name and match_type
# Cygwin adds the Windows version e.g. uname = CYGWIN_NT-5.1
set platform_only = `echo $platform_name | sed 's/_.*//'`

# sanitize i386 thru i686 to one option, do not use this to set build optimizations!
if ( "linux" == $platform_name ) then
	set mach_type = `uname -m | sed 's/i[3456]86/ia32/' `
endif

# default to 64bit builds when object mode is not set
if (!($?OBJECT_MODE)) then
	setenv OBJECT_MODE 64
endif

### 64bit vs 32bit builds
# The only 32 bit targets are cygwin, ia32 and x86_64 with specific settings
if ( ( "ia32" == $mach_type ) ||  ( "cywgin" == $platform_only ) ) then
	setenv gt_build_type 32
# build 32 bit on x86_64 when $gtm_inc/x86_64.h does not exist with comlist.csh OR when OBJECT_MODE is set to 32 with comlist.mk
else if ( "x86_64" == $mach_type && ((! -e $gtm_inc/x86_64.h && "inc" == "${gtm_inc:t}") || "32" == $OBJECT_MODE)) then
	setenv gt_build_type 32
else
	setenv gt_build_type 64
	setenv gt_ld_m_shl_options "-shared"
endif

if ( $?gtm_version_change == "1" ) then

	# Compiler selections:
	#

	if !( $?gtm_linux_compiler ) then
	 setenv gtm_linux_compiler gcc
	endif

        if ( "ia64" == $mach_type ) then
	 if ( "gcc" != $gtm_linux_compiler ) then
	  if (-r /usr/bin/icc) then
        	setenv  gt_cc_compiler          "icc -i-static"            # name of C compiler
	  endif
         endif
	endif

	# Archiver definitions:
	# GNU ar q equals r, S prevents generating the symbol table but
	#			requires ranlib before linking

	setenv	gt_ar_option_create	"qSv"		# quick, verbose
	setenv	gt_ar_option_update	"rv"		# replace, verbose
	setenv	gt_ar_option_delete	"dv"		# delete, verbose
	setenv	gt_ar_use_ranlib	"yes"

	# Assembler definitions:

	# From before conversion from MASM or get MASM running under DOSEMU or such
	# setenv	gt_as_use_prebuilt	"yes"

	# GNU as

        if ( "ia64" == $mach_type ) then
		if ( "icc" == $gtm_linux_compiler ) then
			setenv gt_cpp_compiler		"icc"		# name of C preprocessor
			setenv gt_cpp_options_common	"-x assembler-with-cpp"
			setenv gt_as_assembler		"icc"
			setenv gt_as_options_common	"-c -i-static"
			setenv gt_as_option_optimize	"$gt_as_option_optimize -O3"
		else
			setenv gt_as_assembler		"gcc -c"
		endif
        endif

        if ( "ia64" != $mach_type ) then
            setenv gt_as_assembler          "as"
	    if ("s390x" == $mach_type) then
	        setenv gt_as_options_common	"-march=z9-109"
		setenv gt_as_option_debug	"--gdwarf-2"
	    else if ( "cygwin" == $platform_only ) then
	        setenv gt_as_options_common	"--defsym cygwin=1"
	    	setenv gt_as_option_debug	"--gdwarf-2"
	    else
        	setenv gt_as_option_debug      "--gstabs"
	    endif
        endif

	# to avoid naming files with .S
	# smw 1999/12/04 setenv gt_as_options_common	"-c -x assembler-with-cpp"
	setenv gt_as_option_DDEBUG	"-defsym DEBUG=1"

	# C definitions:

	# generate position independent code
	setenv 	gt_cc_shl_fpic		"-fPIC"
	if ( "cygwin" == $platform_only ) then
		setenv gt_cc_shl_fpic	""
	endif

        #	setenv	gt_cc_options_common 	"-c -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64"
	# For gcc: _BSD_SOURCE for caddr_t, others
	#	   _XOPEN_SOURCE=500 should probably define POSIX 199309 and/or
	#		POSIX 199506 but doesnt so...
	#	   -fsigned-char for Linux390 but shouldn't hurt x86

	#	setenv	gt_cc_options_common	"-c -ansi -D_XOPEN_SOURCE=500 -D_BSD_SOURCE -D_POSIX_C_SOURCE=199506L
	#	setenv	gt_cc_options_common	"$gt_cc_options_common -D_FILE_OFFSET_BITS=64 -DFULLBLOCKWRITES -fsigned-char"
	#	_GNU_SOURCE includes _XOPEN_SOURCE=400, _BSD_SOURCE, and _POSIX_C_SOURCE-199506L among others
	#       Need _XOPEN_SOURCE=600 for posix_memalign() interface (replaces obsolete memalign)

	if ( "cygwin" == $platform_only ) then
#	on Cygwin, -ansi defines __STRICT_ANSI__ which suppresses many prototypes
		setenv gt_cc_options_common     "-c "
	else
		setenv gt_cc_options_common     "-c -ansi "
	endif

        setenv  gt_cc_options_common    "$gt_cc_options_common -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 "
        setenv  gt_cc_options_common    "$gt_cc_options_common -D_XOPEN_SOURCE=600 -fsigned-char -Wreturn-type -Wpointer-sign "

        if ( "ia64" != $mach_type ) then
		if ("$gt_cc_compiler" =~ *icc*) then
        		setenv gt_cc_options_common "$gt_cc_options_common $gt_cc_shl_fpic"
		else
			setenv gt_cc_options_common "$gt_cc_options_common $gt_cc_shl_fpic -Wimplicit"
		endif
		setenv gt_cc_options_common "$gt_cc_options_common -Wmissing-prototypes -D_LARGEFILE64_SOURCE"
        endif

	if ( "cygwin" == $platform_only ) then
		setenv gt_cc_options_common "$gt_cc_options_common -DNO_SEM_TIME -DNO_SEM_GETPID"
	endif

	# 32 bit ICU headers of 32 bit linux are in /emul/ia32-linux/usr/include/
	if ( "32" == $gt_build_type ) then
		if (-d /emul/ia32-linux/usr/include/) setenv gt_cc_option_I  "-I/emul/ia32-linux/usr/include/"
	endif

	if ( "linux" == $platform_name ) then
		set lversion=`uname -r`
        	set ltemp_ver=`echo $lversion | sed 's/./& /g'`
        	if ($ltemp_ver[3] == "2"  && $ltemp_ver[1] == "2") then
            		setenv gt_cc_options_common "$gt_cc_options_common -DNeedInAddrPort"
        	endif
		if ( "x86_64" == $mach_type ) then
			# Add flags for warnings that we want and don't want.
			set desired_warnings = ( -Wall )
			# The following warnings would be desirable, but together can result in megabytes of warning messages. We
			# should look into how hard they would be to clean up. It is possible that some header changes could
			# reduce a large number of these.
			#set desired_warnings = ( $desired_warnings -Wconversion -Wsign-compare )
			#
			# We should also look into how hard these would be to restore. Some of the warnings come from generated
			# code and macro use, making them harder to deal with.
			set undesired_warnings = ( -Wunused-result -Wparentheses -Wunused-value -Wunused-variable )
			set undesired_warnings = ( $undesired_warnings -Wmaybe-uninitialized -Wchar-subscripts )
			set undesired_warnings = ( $undesired_warnings -Wunused-but-set-variable )
			if ( { (cc --version |& grep -q -E '4.4.7|4.7.2|4.6.3|SUSE') } ) then
				set undesired_warnings = ( $undesired_warnings -Wuninitialized )
			endif
			# If our compiler happens not to support --help=warnings, the following will produce junk, but it is
			# unlikely that the junk will match the warning options of interest, so it shouldn't be a problem.
			set -f supported_warnings = `cc --help=warnings | & awk '$1 ~ /^-[^=]*$/ {print $1}'`
			foreach w ($desired_warnings)
				# Add the warning to a copy of the supported list, discarding duplicates.
				set -f tmp_warnings = ($supported_warnings $w)
				if ("$supported_warnings" == "$tmp_warnings") then
					# Adding the desired warning didn't change the list, so it is supported.
					setenv gt_cc_options_common "$gt_cc_options_common $w"
				endif
			end
			foreach w ($undesired_warnings)
				# Add the warning to a copy of the supported list, discarding duplicates.
				set -f tmp_warnings = ($supported_warnings $w)
				if ("$supported_warnings" == "$tmp_warnings") then
					# Adding the desired warning didn't change the list, so it is supported.
					setenv gt_cc_options_common "$gt_cc_options_common -Wno-${w:s/-W//}"
				endif
			end
		endif
	endif

	# -fno-defer-pop to prevent problems with assembly/generated code with optimization
	# -fno-strict-aliasing since we don't comply with the rules
	# -ffloat-store for consistent results avoiding rounding differences
	# -fno-omit-frame-pointer so %rbp always gets set up (required by caller_id()). Default changed in gcc 4.6.
	if ( "ia64" != $mach_type ) then
		setenv	gt_cc_option_optimize	"-O2 -fno-defer-pop -fno-strict-aliasing -ffloat-store -fno-omit-frame-pointer"
		if ( "32" == $gt_build_type ) then
			# applies to 32bit x86_64, ia32 and cygwin
			# Compile 32-bit x86 GT.M using 586 instruction set rather than 686 as the new Intel Quark
			# low power system-on-a-chip uses the 586 instruction set rather than the 686 instruction set
			setenv  gt_cc_option_optimize "$gt_cc_option_optimize -march=i586"
		endif
	endif
	# -g	generate debugging information for dbx (no longer overrides -O)
	setenv	gt_cc_option_debug	"-g"
	if ( "cygwin" == $platform_only ) then
		setenv gt_cc_option_debug "$gt_cc_option_debug -gdwarf-2 -fno-inline -fno-inline-functions"
	endif

	# Linker definitions:
	setenv	gt_ld_linker		"$gt_cc_compiler" # redefine to use new C compiler definition

	# -M		generate link map onto standard output
	setenv	gt_ld_options_common	"-Wl,-M"
	setenv 	gt_ld_options_gtmshr	"-Wl,-u,accumulate -Wl,-u,is_big_endian -Wl,-u,to_ulong"
	setenv 	gt_ld_options_gtmshr	"$gt_ld_options_gtmshr -Wl,-u,gtm_filename_to_id -Wl,--version-script,gtmshr_symbols.export"
	setenv 	gt_ld_options_all_exe	"-rdynamic -Wl,-u,gtm_filename_to_id -Wl,-u,gtm_zstatus"
	setenv	gt_ld_options_all_exe	"$gt_ld_options_all_exe -Wl,--version-script,gtmexe_symbols.export"

  	# optimize for all 64bit platforms
 	#
 	# -lrt doesn't work to pull in semaphores with GCC 4.6, so use -lpthread.
 	# Add -lc in front of -lpthread to avoid linking in thread-safe versions
 	# of libc routines from libpthread.
        setenv	gt_ld_syslibs		" -lelf -lncurses -lm -ldl -lc -lpthread -lrt"
	if ( 32 == $gt_build_type ) then
		# 32bit x86_64 and ia32 - decided at the beginning of the file
		setenv  gt_ld_syslibs           " -lncurses -lm -ldl -lc -lpthread -lrt"
	endif
	if ( "cygwin" == $platform_only ) then
		setenv  gt_ld_syslibs           "-lncurses -lm -lcrypt"
	endif

	# -lrt for async I/O in mupip recover/rollback
	setenv gt_ld_aio_syslib         "-lrt"
	if ( "cygwin" == $platform_only ) then
		setenv gt_ld_aio_syslib         ""
	endif

	# Shared library definition overrides:
	setenv	gt_cc_shl_options	"-c $gt_cc_shl_fpic"

	setenv	gt_ld_shl_linker	"cc"
	setenv	gt_ld_shl_options	"-shared"

	# If we are trying to force a 32 bit build on a 64 bit x86 machine, then we need to explicitly specify a 32 bit
	# over-ride option.
        if ( "x86_64" == $mach_type && "32" == $gt_build_type ) then
		setenv  gt_cc_options_common 	"$gt_cc_options_common -m32"
		setenv  gt_ld_options_gtmshr	"$gt_ld_options_gtmshr -m32"
                setenv  gt_cc_shl_options	"$gt_cc_shl_options -m32"
                setenv  gt_ld_shl_options	"$gt_ld_shl_options -m32"
                setenv  gt_ld_options_common	"$gt_ld_options_common -m32"
        endif

	# need to re-define these in terms of new gt_ld_options_common:
	setenv	gt_ld_options_bta	"$gt_ld_options_common"
	setenv	gt_ld_options_dbg	"$gt_ld_options_common"
	setenv	gt_ld_options_pro	"$gt_ld_options_common"

	setenv	gt_ld_shl_suffix	".so"

	# lint definition overrides
	# setenv	gt_lint_linter		""

	setenv	gt_lint_options_library	"-x"
	setenv	gt_lint_options_common	""

endif

# PBO(Profile based optimization) settings for Linux-IA64, intel compiler.
set gtm_build_image = `basename $gtm_exe`
if ( "ia64" == $mach_type  && "pro" == $gtm_build_image && "icc" == $gtm_linux_compiler) then
	setenv gt_cc_option_optimize	"-O3"
	if ($?gtm_pbo_option) then
		@ need_mkdir = 0
		if !($?gtm_pbo_db) then
			@ need_mkdir = 1
			setenv  gtm_pbo_db	"$gtm_log/pbo"
		else if !(-e $gtm_pbo_db) then
			@ need_mkdir = 1
		endif
		if (1 == $need_mkdir) then
			mkdir -p $gtm_pbo_db
			chmod 775 $gtm_pbo_db		# Others need ability to write to the pbo files
		endif
		if ($gtm_pbo_option == "collect") then
			setenv 	gt_cc_option_optimize	"-prof-gen -prof-dir $gtm_pbo_db"
			setenv  gt_ld_options_pro	"$gt_ld_options_pro -prof-gen -prof-dir $gtm_pbo_db"
			setenv gt_as_option_optimize	"$gt_cc_option_optimize"
		else if ($gtm_pbo_option == "use") then
			setenv  gt_cc_option_optimize   "-O3 -prof-use -prof-dir $gtm_pbo_db"
			setenv  gt_ld_options_pro       "$gt_ld_options_pro -prof-dir $gtm_pbo_db"
		endif
	endif
endif

# Assembler definitions:
# Note: we need to specify the assembler output file name or it will write it to the source directory.
if ( "ia64" == $mach_type ) then
	alias	gt_as_bta	'gt_as $gt_as_option_debug $gt_as_option_nooptimize'
	alias	gt_as_dbg	'gt_as $gt_as_option_DDEBUG $gt_as_option_debug $gt_as_option_nooptimize'
	alias	gt_as_pro	'gt_as $gt_as_option_optimize '
endif

if ( "s390x" == $mach_type) then
	setenv gt_asm_arch_size "-m64"
else if ( "64" == $gt_build_type) then
	setenv gt_asm_arch_size "--64"
else
	setenv gt_asm_arch_size "--32"
endif

if ( "ia64" != $mach_type ) then
	alias	gt_as_bta \
		'gt_as $gt_as_option_debug $gt_as_option_nooptimize $gt_asm_arch_size -o `basename \!:1 .s`.o \!:1'
		# If the value of the alias variable extends the 132 character limit, use a temporary variable
		# (like gtm_as_dbg_var below) and use that in the alias value to stick to the the coding standard.
	set gt_as_dbg_var = "$gt_as_option_DDEBUG $gt_as_option_debug $gt_as_option_nooptimize $gt_asm_arch_size"
	alias	gt_as_dbg	'gt_as $gt_as_dbg_var -o `basename \!:1 .s`.o \!:1'
	alias   gt_as_pro	'gt_as $gt_as_option_optimize $gt_asm_arch_size -o `basename \!:1 .s`.o \!:1'
endif
