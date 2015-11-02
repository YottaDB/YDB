#################################################################
#								#
#	Copyright 2001, 2010 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
##################################################################################
#
#	gtm_env.csh - establish GT.M environment
#
#	This script defines reasonable default values for environment variables
#	and aliases generally compatible with as many Unix platforms as possible.
#
#	Each environment variable or alias used in any non-platform-specific Unix
#	shell script should have a default value defined in here.  In addition,
#	this script can contain definitions of other environment variables and
#	aliases considered to be generally useful.
#
#	To accomodate platform-specific peculiarities, this script also invokes
#	gtm_env_sp.csh (if one exists) to provide over-riding definitions for any
#	of the environment variables or aliases defined here.
#
#	Naming conventions are designed so an alphabetically-sorted list of
#	environment variables or aliases will cause related definitions to appear
#	together.  Common abbreviations:
#
#		gt	Greystone Technology
#		gt_ar	archiver (library maintenance)
#		gt_as	assembler
#		gt_cc	C compiler
#		gt_ld	linker
#		gt_lint	C de-lint utility
#		gtm	GT.M-specific (as opposed to GT.SQL-specific, for example)
#		shl	shared-library
#
##################################################################################


# Once the environment variables have been initialized, be careful not to
# re-initialize them for each subshell (i.e., don't undo any explicit changes that
# have been made since the last version change).

if ( $?gtm_version_change == "1" ) then

	# Generic archiver information:

	setenv	gt_ar_archiver		"ar"		# name of archiver utility

	setenv	gt_ar_gtmrpc_name	""		# set to "gtmrpc" to create libgtmrpc.a from libgtmrpc.list
	setenv	gt_ar_option_create	"qlv"		# quick, use local directory for temp files, verbose
	setenv	gt_ar_option_update	"rlv"		# replace, use local directory for temp files, verbose
	setenv	gt_ar_option_delete	"dlv"		# delete, use local directory for temp files, verbose
	setenv	gt_ar_use_ranlib	"no"		# don't run ranlib over final archive -- not necessary


	# Generic assembler information:

	setenv	gt_as_assembler		"as"		# name of assembler

	setenv	gt_as_inc_convert	"false"		# if true, convert non-native assembly language header files
							#   to native dialect
	setenv	gt_as_src_convert	"false"		# if true, convert non-native assembly language source files
							#   to native dialect
	setenv	gt_as_src_suffix	".s"		# filename suffix for assembly language source files

	setenv	gt_as_option_DDEBUG	"-DDEBUG"	# define DEBUG compilation-/assembly-time macro
	setenv	gt_as_option_I		""		# specify header (include) file directory
							#   (set by gtmsrc.csh during version command)
	setenv	gt_as_option_debug	""		# generate debugger information
	setenv	gt_as_option_nooptimize	""		# don't optimize generated code
	setenv	gt_as_option_optimize	""		# optimize generated code (if applicable)

	setenv	gt_as_options_common	""		# generally-required assembler option(s)


	# Generic C compiler information:

	setenv	gt_cc_compiler		"cc"		# name of C compiler
	setenv	gt_cpp_compiler		"cpp"		# name of C preprocessor

	setenv	gt_cc_option_DBTABLD	"-DNOLIBGTMSHR"	# define NOLIBGTMSHR macro to statically link mumps for bta image

	setenv	gt_cc_option_DDEBUG	"-DDEBUG"	# define DEBUG compilation-time macro

	setenv	gt_cc_option_I		"-I/usr/local/include"	# specify ICU header (include) file directory
	setenv	gt_cc_option_I		" $gt_cc_option_I -I/usr/include/libelf" # Libelf is under different directory in SUSE
								#   (set by gtmsrc.csh during version command)
	setenv	gt_cc_option_debug	"-g"		# generate debugger information
	setenv	gt_cc_option_nooptimize	""		# don't optimize generated code
	setenv	gt_cc_option_optimize	"-O"		# optimize generated code

	setenv	gt_cc_options_common	"-c"		# suppress link phase; force .o for each .c file even if only one
	setenv	gt_cpp_options_common	""		# Options for C preprocessor


	# Generic linker information:

	setenv	gt_ld_linker		"$gt_cc_compiler" # name of link editor; use cc instead of ld to ensure correct
							  #   startup routines and C runtime libraries

	setenv	gt_ld_option_output	"-o "		# option to specify where linker should write output
							#   (some linkers do not allow a space; some require it)

	setenv	gt_ld_options_common	""		# generally-required linker options

	setenv	gt_ld_options_bta	"$gt_ld_options_common"
	setenv	gt_ld_options_dbg	"$gt_ld_options_common"
	setenv	gt_ld_options_pro	"$gt_ld_options_common"
	setenv	gt_ld_options_gtmshr	""
# force the linker to retain gtmci.o & dependent modules even if not referenced.
	setenv gt_ld_ci_u_option	"-Wl,-u,gtm_ci"

	setenv gt_ld_extra_libs		""		# platform specific GT.M libraries

	setenv	gt_ld_syslibs		"-lcurses -lm"	# system libraries needed for link (in addition to defaults)
	setenv	gt_ld_sysrtns		""		# system routines needed for link (in addition to defaults)
	setenv	gt_ld_aio_syslib	""		# system libraries needed for async I/O routines

	# Linker options to create shared libraries from GT.M generated objects
	setenv	gt_ld_m_shl_linker	"ld"
	setenv	gt_ld_m_shl_options	""

	# Generic shared library information:

	# setenv	gt_cc_shl_options	""	# there is no good default for this; leave uninitialized here,
							#  forcing initialization in gtm_env_sp.csh


	setenv	gt_ld_shl_linker	"$gt_ld_linker"

	# setenv	gt_ld_shl_options	""	# there is no good default for this; leave uninitialized here,
							#  forcing initialization in gtm_env_sp.csh
	setenv	gt_ld_shl_suffix	".sl"


	# Generic lint information:

	setenv	gt_lint_linter		"lint"		# name of linter; parameterized to allow use of non-default

	setenv	gt_lint_option_output	"-o "		# option to specify where lint should write output library
							#   (some lints do not allow a space; some require it)

	setenv	gt_lint_options_common	""		# generally-required lint options
	setenv	gt_lint_options_library	""		# lint options specific to lint library generation
							#  (e.g., ignore undefined externals because libraries typically
							#   only #   contain a subset of sources for a program)

	setenv	gt_lint_options_bta	"$gt_cc_option_DDEBUG"
	setenv	gt_lint_options_dbg	"$gt_cc_option_DDEBUG"
	setenv	gt_lint_options_pro	""

	setenv	gt_lint_syslibs		""		# system libraries against which to check for compatibility


endif



# Alias definitions.  In some Unix implementations, alias definitions are not inherited
# by subshells, so they need to be re-initialized each time.  Aliases should not be
# redefined by users; they should be defined to depend on environment variables which
# can be changed by users.


# Generic archiver invocations:

alias	gt_ar			'$gt_ar_archiver'


# Generic assembler invocations:

alias	gt_as			'$gt_as_assembler $gt_as_options_common $gt_as_option_I'
alias	gt_as_bta		'gt_as $gt_as_option_debug $gt_as_option_nooptimize'
alias	gt_as_dbg		'gt_as $gt_as_option_DDEBUG $gt_as_option_debug $gt_as_option_nooptimize'
alias	gt_as_pro		'gt_as $gt_as_option_optimize'


# Generic C compiler invocations:

alias	gt_cpp			'$gt_cpp_compiler $gt_cpp_options_common $gt_cc_option_I'
alias	gt_cc			'$gt_cc_compiler $gt_cc_options_common $gt_cc_option_I'
alias	gt_cc_bta		'gt_cc $gt_cc_option_DBTABLD $gt_cc_option_debug $gt_cc_option_nooptimize'
alias	gt_cc_dbg		'gt_cc $gt_cc_option_DDEBUG $gt_cc_option_debug $gt_cc_option_nooptimize'
alias	gt_cc_pro		'gt_cc $gt_cc_option_optimize'


# Generic linker invocation:

alias	gt_ld			'$gt_ld_linker'
alias	gt_ld_shl		'$gt_ld_shl_linker'


# Generic lint invocation:

alias	gt_lint			'$gt_lint_linter'


# GT.M commands and utilities:

alias	dse	'$gtm_exe/dse'
alias	gde	'mumps -run GDE'
alias	gtm	'mumps -direct'
alias	lke	'$gtm_exe/lke'
alias	mumps	'$gtm_exe/mumps'
alias	mupip	'$gtm_exe/mupip'


# Note: on VMS, this is defined as the symbol VER*SION, allowing abbreviation by
# omitting trailing characters as long as the first three ("VER") are specified.

alias	version	'set setactive_parms=(\!*); source $gtm_tools/setactive.csh'

if ( $?prompt == "1" ) then
	# On Unix, we have to define all of the shorter forms as individual symbols, although
	# it's unlikely any forms other than "ver" or "version" are ever used.  Note we only
	# do this for login (interactive) processes; shell scripts should not need to abbreviate.
	alias	ver	version
	alias	vers	version
	alias	versi	version
	alias	versio	version
endif

# Platform-specific overrides, if any:

if ( -f $gtm_tools/gtm_env_sp.csh ) then
	source $gtm_tools/gtm_env_sp.csh
endif

# Allow platform specific gt_ld_ci related symbol changes
# force the linker to retain gtmci.o & dependent modules even if not referenced.
setenv gt_ld_ci_options "$gt_ld_ci_u_option $gt_ld_options_gtmshr"
