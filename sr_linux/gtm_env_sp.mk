#################################################################
#                                                               #
#       Copyright 2001, 2008 Fidelity Information Services, Inc #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

#
##########################################################################################
#
#       gtm_env_sp.mk - environment variable values and aliases specific to Linux
#
##########################################################################################
gt_machine_type=$(shell uname -m)
linux_build_type=32

ifeq ($(gt_machine_type),ia64)
linux_build_type=64
endif

ifeq ($(gt_machine_type),x86_64)
ifeq ($(OBJECT_MODE),64)
linux_build_type=64
else
linux_build_type=32
endif
endif

# GNU assembler options
ifeq ($(gt_machine_type), ia64)
gt_as_assembler=gcc -c
else
gt_as_assembler=as
gt_as_option_debug=--gstabs
endif
# to avoid naming files with .S
# smw 1999/12/04 gt_as_options_common=-c -x assembler-with-cpp
ifeq ($(linux_build_type),64)
gt_as_options_common=
else
gt_as_options_common= --32
endif
gt_as_option_DDEBUG=
gt_as_option_nooptimize=
gt_as_option_optimize=
gt_as_src_suffix=.s

# C compiler options
gt_cc_compiler=gcc

# Do not lookup the source directory before include directories specified by -I.
gt_cc_option_I=-I-
gt_cc_shl_fpic=-fPIC                    # generate Position Independent Code.

gt_cpp_compiler=cpp
gt_cpp_options_common=
#       gt_cc_options_common=-c -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64
#       gt_cc_options_common=-c -ansi -D_XOPEN_SOURCE=500 -D_BSD_SOURCE -D_POSIX_C_SOURCE=199506L -D_FILE_OFFSET_BITS=64
#               FULLBLOCKWRITES to make all block IO read/write the entire block to stave off prereads (assumes blind writes supported)
# For gcc: _BSD_SOURCE for caddr_t, others
#          _XOPEN_SOURCE=500 should probably define POSIX 199309 and/or
#               POSIX 199506 but doesnt so...
gt_cc_options_common=$(gt_cc_shl_fpic) -c -ansi -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -fsigned-char -Wimplicit -Wmissing-prototypes
ifeq ($(strip $(patsubst 2.2.%, 2.2, $(shell uname -r))), 2.2)
gt_cc_options_common:=$(gt_cc_options_common) -DNeedInAddrPort
endif
gt_cc_option_nooptimize=
# -fno-defer-pop to prevent problems with assembly/generated code with optimization
# -fno-strict-aliasing since we don't comply with the rules
# -ffloat-store for consistent results avoiding rounding differences
gt_cc_option_optimize=-O2 -fno-defer-pop -fno-strict-aliasing -ffloat-store
ifeq ($(linux_build_type),32)
gt_cc_option_optimize:=$(gt_cc_option_optimize) -march=i686
endif
# autodepend option
gt_cc_dep_option=-w
# -g    generate debugging information for dbx (no longer overrides -O)
gt_cc_option_debug=-g
gt_cc_shl_options=-c $(gt_cc_shl_fpic)


# Linker definitions:
gt_ld_linker=$(gt_cc_compiler)
gt_ld_options_common=-Wl,-M             # to generate link map onto standard output
gt_ld_options_bta=-g
gt_ld_options_dbg=-g
gt_ld_options_pro=
# -lrt for async I/O in mupip recover/rollback
ifeq ($(linux_build_type), 64)
gt_ld_syslibs=-lrt -lelf -lncurses -lm -ldl
else
gt_ld_syslibs=-lrt -lncurses -lm -ldl
endif
gt_ld_aio_syslib=
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

ifeq ($(linux_build_type), 32)
gt_cc_options_common:=$(gt_cc_options_common) -m32
gt_ld_options_gtmshr:=$(gt_ld_options_gtmshr) -m32
gt_cc_shl_options:=$(gt_cc_shl_options) -m32
gt_ld_shl_options:=$(gt_ld_shl_options) -m32
gt_ld_options_common:=$(gt_ld_options_common) -m32
endif
#
# gas assembly - the preprocessor works
#
define gt-as
$(gt_as_assembler) $(gt_as_options) $< -o $@
endef
define gt_cpp
$(gt_cpp_compiler) -E $(gt_cpp_options_common) $(gt_cc_option_I) $< > $<_cpp.s
endef
define gt-as_cpp
$(gt_as_assembler) $(gt_as_options) $<_cpp.s -o $@
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
        @sed 's/\(.*\)/ \1;/g' $< >>$@
        @echo "local:" >>$@
        @echo " *;" >>$@
        @echo "};" >>$@
endef

