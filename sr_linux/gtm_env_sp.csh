#################################################################
#								#
#	Copyright 2001, 2008 Fidelity Information Services, Inc	#
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
# Cygwin adds the Windows version e.g. uname = CYGWIN_NT-5.1
set platform_only = `echo $platform_name | sed 's/_.*//'`

#### To determine whether the build type is 64 or 32. On x86_64 linux box, use the header file "x86_64.h" in the inc directory.
if ( "ia64" == $mach_type || ( "x86_64" == $mach_type && -e $gtm_inc/x86_64.h) ) then
	setenv linux_build_type 64
else
	setenv linux_build_type 32
endif

if ( $?gtm_version_change == "1" ) then

	# Archiver definitions:
	# GNU ar q equals r, S prevents generating the symbol table but
	#			requires ranlib before linking

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

	setenv	gt_ar_option_create	"qSv"		# quick, verbose
	setenv	gt_ar_option_update	"rv"		# replace, verbose
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
	    if ( "cygwin" == $platform_only ) then
	        setenv gt_as_options_common	"--defsym cygwin=1"
	    	setenv gt_as_option_debug	"--gdwarf-2"
	    else
        	setenv gt_as_option_debug      "--gstabs"
	    endif
        endif

	# to avoid naming files with .S
	# smw 1999/12/04 setenv gt_as_options_common	"-c -x assembler-with-cpp"
	setenv gt_as_option_DDEBUG	""

	# C definitions:

	# generate position independent code
	if ( "cygwin" == $platform_only ) then
		setenv gt_cc_shl_fpic	""
	else
		setenv 	gt_cc_shl_fpic		"-fPIC"
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
        setenv  gt_cc_options_common    "$gt_cc_options_common -D_XOPEN_SOURCE=600 -fsigned-char "

        if ( "ia64" != $mach_type ) then
		set tmpgtmval = `echo $gt_cc_compiler | grep icc`
		if ($status) then
        		setenv gt_cc_options_common "$gt_cc_options_common $gt_cc_shl_fpic"
		else
			setenv gt_cc_options_common "$gt_cc_options_common $gt_cc_shl_fpic -Wimplicit"
		endif
		setenv gt_cc_options_common "$gt_cc_options_common -Wmissing-prototypes -D_LARGEFILE64_SOURCE"
        endif

	if ( "cygwin" == $platform_only ) then
		setenv gt_cc_options_common "$gt_cc_options_common -DNO_SEM_TIME -DNO_SEM_GETPID"
	endif

	# 64 bit ICU headers of 64 bit linux are in /emul/ia64-linux/usr/include/
	if ( "x86_64" == $mach_type ) then
		if ( "32" == $linux_build_type ) then
			if (-d /emul/ia32-linux/usr/include/) setenv gt_cc_option_I  "-I/emul/ia32-linux/usr/include/"
		endif
	endif

	if ( "linux" == $platform_name ) then
		set lversion=`uname -r`
        	set ltemp_ver=`echo $lversion | sed 's/./& /g'`
        	if ($ltemp_ver[3] == "2"  && $ltemp_ver[1] == "2") then
            		setenv gt_cc_options_common "$gt_cc_options_common -DNeedInAddrPort"
        	endif
	endif

	# -fno-defer-pop to prevent problems with assembly/generated code with optimization
	# -fno-strict-aliasing since we don't comply with the rules
	# -ffloat-store for consistent results avoiding rounding differences
        if ( "ia64" != $mach_type ) then
		setenv	gt_cc_option_optimize	"-O2 -fno-defer-pop -fno-strict-aliasing -ffloat-store"
		if ( 64 != $linux_build_type ) then
			setenv  gt_cc_option_optimize "$gt_cc_option_optimize -march=i686"
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
	setenv 	gt_ld_options_gtmshr	"-Wl,--version-script,gtmshr_symbols.export"

        if ( "ia64" == $mach_type ) then
		# Added -lelf
        	setenv	gt_ld_syslibs		" -lrt -lelf -lncurses -lm -ldl"
        endif

        if ( "ia64" != $mach_type && "linux" == $platform_name ) then
		if ( "x86_64" == $mach_type && 64 == $linux_build_type ) then
	        	setenv  gt_ld_syslibs           " -lrt -lelf -lncurses -lm -ldl"
		else
			setenv  gt_ld_syslibs           " -lrt -lncurses -lm -ldl"
		endif
        endif

	if ( "cygwin" == $platform_only ) then
		setenv  gt_ld_syslibs           "-lncurses -lm -lcrypt"
	endif

	# -lrt for async I/O in mupip recover/rollback
	if ( "cygwin" == $platform_only ) then
		setenv gt_ld_aio_syslib         ""
	else
		setenv gt_ld_aio_syslib         "-lrt"
	endif

	# Shared library definition overrides:
	setenv	gt_cc_shl_options	"-c $gt_cc_shl_fpic"

	setenv	gt_ld_shl_linker	"cc"
	setenv	gt_ld_shl_options	"-shared"

	# If we are trying to force a 32 bit build on a 64 bit x86 machine, then we need to explicitly specify a 32 bit
	# over-ride option.
        if ( "x86_64" == $mach_type && "32" == $linux_build_type ) then
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
		if !($?gtm_pbo_db) then
			if !(-e $gtm_log/pbo) then
				mkdir -p $gtm_log/pbo
			endif
			setenv  gtm_pbo_db	"$gtm_log/pbo"
		else if !(-e $gtm_pbo_db) then
			if !(-e $gtm_log/pbo) then
				mkdir -p $gtm_log/pbo
			endif
			setenv  gtm_pbo_db	"$gtm_log/pbo"
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
if ( "ia64" != $mach_type ) then
	if ( "64" == $linux_build_type) then
		setenv assembler_op_arch_size "--64"
	else
		setenv assembler_op_arch_size "--32"
	endif
	alias	gt_as_bta \
		'gt_as $gt_as_option_debug $gt_as_option_nooptimize $assembler_op_arch_size -o `basename \!:1 .s`.o \!:1'
		# Do not split alias definitions across multiple lines as tcsh will then use the last partial line
		# as the definition. So it is ok even if the definition goes more than 132 columns (coding standard).
	alias	gt_as_dbg	\
		'gt_as $gt_as_option_DDEBUG $gt_as_option_debug $gt_as_option_nooptimize $assembler_op_arch_size -o `basename \!:1 .s`.o \!:1'
	alias   gt_as_pro	'gt_as $gt_as_option_optimize $assembler_op_arch_size -o `basename \!:1 .s`.o \!:1'
endif
