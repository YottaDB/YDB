#################################################################
#								#
#	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	#
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
#	gtm_env_sp.mk - environment variable values and aliases specific to Linux
#
##########################################################################################

# GNU assembler options
gt_as_assembler=as
# to avoid naming files with .S
# smw 1999/12/04 gt_as_options_common=-c -x assembler-with-cpp
gt_as_options_common=
gt_as_option_debug="--gstabs"
gt_as_option_DDEBUG=
gt_as_option_nooptimize=
gt_as_option_optimize=
gt_as_src_suffix=.s

# C compiler options
gt_cc_compiler=gcc

# Do not lookup the source directory before include directories specified by -I.
gt_cc_option_I=-I-
gt_cc_shl_fpic=-fPIC			# generate Position Independent Code.

#	gt_cc_options_common=-c -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64
#	gt_cc_options_common=-c -ansi -D_XOPEN_SOURCE=500 -D_BSD_SOURCE -D_POSIX_C_SOURCE=199506L -D_FILE_OFFSET_BITS=64
# 		FULLBLOCKWRITES to make all block IO read/write the entire block to stave off prereads (assumes blind writes supported)
# For gcc: _BSD_SOURCE for caddr_t, others
#	   _XOPEN_SOURCE=500 should probably define POSIX 199309 and/or
#		POSIX 199506 but doesnt so...
# Linux gcc optimizations cause problems so do without them for now.
gt_cc_options_common=$(gt_cc_shl_fpic) -c -ansi -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -fsigned-char -Wimplicit -Wmissing-prototypes
ifeq ($(strip $(patsubst 2.2.%, 2.2, $(shell uname -r))), 2.2)
gt_cc_options_common:=$(gt_cc_options_common) -DNeedInAddrPort
endif
gt_cc_option_nooptimize=
gt_cc_option_optimize=
# autodepend option
gt_cc_dep_option=-w
# -g	generate debugging information for dbx (no longer overrides -O)
gt_cc_option_debug=-g
gt_cc_shl_options=-c $(gt_cc_shl_fpic)


# Linker definitions:
gt_ld_linker=$(gt_cc_compiler)
gt_ld_options_common=-Wl,-M 		# to generate link map onto standard output
gt_ld_options_bta=-g
gt_ld_options_dbg=-g
gt_ld_options_pro=
ifeq ($(MACHTYPE), "s390")
gt_ld_syslibs=-lncurses -lm -ldl
else
gt_ld_syslibs=-lcurses -lm -ldl
endif
# -lrt for async I/O in mupip recover/rollback
gt_ld_aio_syslib=-lrt
gt_ld_sysrtns=
gt_ld_options_gtmshr=-Wl,--version-script,gtmshr_symbols.export
gt_ld_shl_linker=$(gt_ld_linker)
gt_ld_shl_options=-shared
gt_ld_shl_suffix=.so

# lint definition overrides
gt_lint_linter=
gt_lint_options_library=-x
gt_lint_options_common=

gt_cpus=$(shell grep -c process /proc/cpuinfo)
# used to build VPATH
gt_echoe=echo -e

#
# gas assembly - the preprocessor works
#
define gt-as
$(gt_as_assembler) $(gt_as_options) $< -o $@
endef
#
# gcc specific rule to get the depend file (CC -M)
#
define gt-dep
	@echo $*.o $*.d : '\' > $@; \
	echo $(notdir $(filter-out /usr/include% /usr/lib/% /usr/local/include% /usr/local/lib/%, $(filter %.c %.h,$(shell $(gt_cc_compiler) -M $(gt_cc_options) $(gt_cc_dep_option) $<)))) >> $@
endef
define gt-export
	@echo "VERSION {" >$@
	@echo "global:" >>$@
	@sed 's/\(.*\)/	\1;/g' $< >>$@
	@echo "local:" >>$@
	@echo "	*;" >>$@
	@echo "};" >>$@
endef
