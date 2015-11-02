#################################################################
#                                                               #
#       Copyright 2001, 2012 Fidelity Information Services, Inc #
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
#       		if not Linux we assume Cygwin and x86
#
##########################################################################################
# GNU assembler options

gt_as_assembler=as
gt_as_option_DDEBUG=
gt_as_option_debug=--gstabs
gt_as_option_nooptimize=
gt_as_option_optimize=
gt_as_options=
gt_as_options_common=--64
gt_as_src_suffix=.s

ifeq ($(gt_machine_type), ia64)
gt_as_assembler=gcc -c
gt_as_option_debug=
else
ifeq  ($(gt_machine_type), s390x)
gt_as_options_common=-march=z9-109 -m64
gt_as_option_debug=--gdwarf-2
else
ifeq  ($(gt_machine_type), CYGWIN)
gt_as_option_debug=--gdwarf-2
gt_as_options_common=--defsym cygwin=1
endif
endif
endif
ifeq ($(gt_build_type), 32)
ifeq ($(gt_os_type),Linux)
gt_as_options_common=--32
endif
endif

# C compiler options

gt_cc_shl_fpic=-fPIC
gt_cc_options_common=-c -ansi
ifeq ($(gt_os_type),CYGWIN)
gt_cc_option_debug=$(gt_cc_option_debug) -gdwarf-2 -fno-inline -fno-inline-functions
gt_cc_shl_fpic=
gt_cc_options_common=-c
endif

gt_cc_compiler=cc
gt_cc_option_DBTABLD=-DNOLIBGTMSHR
gt_cc_option_DDEBUG=-DDEBUG
gt_cc_option_debug=-g
gt_cc_option_nooptimize=
gt_cc_option_optimize=-O2 -fno-defer-pop -fno-strict-aliasing -ffloat-store
gt_cc_options_common+= -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE=600 -fsigned-char

ifeq ($(gt_machine_type), x86_64)
ifeq ($(gt_build_type),32)
# The /emul/ia32-linux/... directory doesn't exist on most machines, but when it's there we need it.  No problem
# with always includeing it.
gt_cc_option_I+= -I/emul/ia32-linux/usr/include/
else
gt_cc_option_I=
endif
endif

# autodepend option
gt_cc_dep_option=-w

ifeq ($(gt_machine_type), ia64)
gt_cc_option_optimize=-O
else
gt_cc_options_common+= $(gt_cc_shl_fpic) -Wmissing-prototypes -D_LARGEFILE64_SOURCE
ifeq ($(gt_build_type),32)
gt_cc_option_optimize+= -march=i686
endif
endif

gt_cc_shl_options=-c $(gt_cc_shl_fpic)

ifeq ($(gt_os_type),CYGWIN)
gt_cc_options_common+= -DNO_SEM_TIME -DNO_SEM_GETPID
endif

# Linker definitions:

gt_ld_aio_syslib=
gt_ld_ci_options=-Wl,-u,gtm_ci -Wl,-u,gtm_filename_to_id -Wl,--version-script,gtmshr_symbols.export
gt_ld_ci_u_option=-Wl,-u,gtm_ci
gt_ld_linker=$(gt_cc_compiler)
gt_ld_m_shl_linker=ld
gt_ld_m_shl_options=-shared
gt_ld_option_output=-o
gt_ld_options_all_exe=-rdynamic -Wl,-u,gtm_filename_to_id -Wl,-u,gtm_zstatus -Wl,--version-script,gtmexe_symbols.export
gt_ld_options_bta=-Wl,-M
gt_ld_options_common=-Wl,-M
gt_ld_options_dbg=-Wl,-M
gt_ld_options_gtmshr=-Wl,-u,gtm_filename_to_id -Wl,--version-script,gtmshr_symbols.export
gt_ld_options_pro=-Wl,-M
gt_ld_shl_linker=cc
gt_ld_shl_options=-shared
gt_ld_shl_suffix=.so
gt_ld_sysrtns=

ifeq ($(gt_build_type),32)
gt_ld_m_shl_options=
endif

# -lrt for async I/O in mupip recover/rollback
# -lrt doesn't work to pull in semaphores with GCC 4.6, so use -lpthread.
# Add -lc in front of -lpthread to avoid linking in thread-safe versions
# of libc routines from libpthread.
ifeq ($(gt_build_type), 64)
gt_ld_syslibs=-lrt -lelf -lncurses -lm -ldl -lc -lpthread
else
ifeq ($(gt_os_type),Linux)
gt_ld_syslibs=-lrt -lncurses -lm -ldl -lc -lpthread
else
gt_ld_syslibs=-lncurses -lm -lcrypt
endif
endif

# lint definition overrides
gt_lint_linter=
gt_lint_options_library=-x
gt_lint_options_common=

gt_cpus=$(shell grep -c process /proc/cpuinfo)
# used to build VPATH
# Apparently Ubuntu does not like the -e option for echo, delete this if the *.mdep make files generate an error
ifeq ($(distro),ubuntu)
gt_echoe=echo
else
gt_echoe=echo -e
endif

ifeq ($(gt_build_type), 32)
gt_cc_options_common+=-m32
gt_ld_options_gtmshr+=-m32
gt_cc_shl_options+=-m32
gt_ld_options_common+=-m32
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
        echo $(notdir $(filter-out /usr/include% /usr/lib% /usr/local/include% /usr/local/lib/%, \
			$(filter %.c %.h,$(shell $(gt_cc_compiler) -M $(gt_cc_options) $(gt_cc_dep_option) $<)))) >> $@
endef
define gt-export
        @echo "{" >$@
        @echo "global:" >>$@
        @sed 's/\(.*\)/ \1;/g' $< >>$@
        @echo "local:" >>$@
        @echo " *;" >>$@
        @echo "};" >>$@
endef

