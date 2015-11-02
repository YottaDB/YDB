#################################################################
#								#
#	Copyright 2002, 2011 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
############### Define platform-specific directory ring-down ##################################
# The purpose of this file is to define the source directories
# for each platform
# It also defines
# - gt_build_type <--- significance for 32bit Linux
# - gt_use_nsb 		 <--- 32bit Linux and CYGWIN use NSB
# - gt_build_xfer_desc <- ia64/x86_64 Linuxen and HPUX IA64 use this

# Preserve sanity ###############################################
# 32bit Linux/CYGWIN are the only GT.M builds that use
# non-shared binaries, aka nsb.  32bit Linux builds are
# the most error prone since x86 starts at i386 till i686
# and you can build 32bit on x86_64 by setting OBJECT_MODE
# to 32
# We must do these checks first to ID NSB builds and 32bit
# linux builds which throw a monkey wrench into the whole
# process.

##################################
# gt_machine_type	set in comlist.mk to $(shell uname -m)
# gt_os_type		set in comlist.mk to $(shell uname -s)

# only HPUX ia64 and Linux ia64/x86_64 need this option
# optimize for not using it
gt_build_xfer_desc=0

# default to using shared libraries
gt_use_nsb=0

# Sanitize the CYGWIN gt_os_type and mark it as NSB
ifeq ($(findstring CYGWIN,$(gt_os_type)), CYGWIN)
$(info Cygwin Host)
gt_os_type=CYGWIN
gt_use_nsb=1
endif

# BEGIN Linux host, check for 32 bitness
gt_build_type=0
ifeq ($(gt_os_type), Linux)
gt_build_type=64

ifeq ($(gt_machine_type),i386)
gt_use_nsb=1
gt_build_type=32
endif

ifeq ($(gt_machine_type),i686)
gt_use_nsb=1
gt_build_type=32
endif

ifeq ($(OBJECT_MODE),32)
gt_use_nsb=1
gt_build_type=32
# Checking for OBJECT_MODE 32 is not accurate
# throw an error if the ARCH is not x86_64
ifneq ($(gt_machine_type),x86_64)
$(error OBJECT_MODE set to 32, but arch is $(gt_machine_type)))
endif
endif

$(info Linux Host $(gt_build_type))
endif
# END Linux host, check for 32 bitness


# BEGIN common dirs, optimized for shared libs
common_dirs_sp=unix_gnp unix_cm unix port_cm port
ifeq ($(gt_use_nsb), 1)
common_dirs_sp=unix_gnp unix_cm unix_nsb unix port_cm port
endif
# END common dirs, optimized for shared libs


# BEGIN ARCH dirs
ifeq ($(gt_os_type), SunOS)
lib_dirs_sp=sun sparc $(common_dirs_sp)
endif

ifeq ($(gt_os_type), AIX)
lib_dirs_sp=aix rs6000 $(common_dirs_sp)
endif

ifeq ($(gt_os_type), OSF1)
lib_dirs_sp=dux alpha $(common_dirs_sp)
endif

ifeq ($(gt_os_type), CYGWIN)
lib_dirs_sp=linux i386 x86_regs $(common_dirs_sp)
endif

ifeq ($(gt_os_type), Linux)

## Begin Linux specific cludgery
### Ugliness due to building 32bit x86 GT.M on x86_64 machines
ifeq ($(gt_build_type),32)
linux_arch=linux i386 x86_regs
else
linux_arch=linux x86_64 x86_regs
gt_build_xfer_desc=1
endif

### WARNING: leave all 64bit Linuxen below this point
ifeq ($(gt_machine_type),s390x)
linux_arch=l390 s390 linux
endif
ifeq ($(gt_machine_type), ia64)
linux_arch=linux ia64
gt_build_xfer_desc=1
endif
## now set lib_dirs_sp
lib_dirs_sp=$(linux_arch) $(common_dirs_sp)
$(info Linux Host $(linux_arch))
## End Linux specific cludgery
endif

ifeq ($(gt_os_type), HP-UX)
ifeq ($(gt_machine_type),ia64)
lib_dirs_sp=hpux ia64 $(common_dirs_sp)
gt_build_xfer_desc=1
else
lib_dirs_sp=hpux hppa $(common_dirs_sp)
endif
endif

ifeq ($(gt_os_type), OS/390)
lib_dirs_sp=os390 s390 $(common_dirs_sp)
endif
# END Arch


# final sources list, prepend sr_
gt_src_list:=$(addprefix sr_, $(lib_dirs_sp))

## in house. override the above selections
## dunno what this is for
ifdef usertype
gt_src_list:=src inc tools pct
endif

$(info Source Directory List: $(gt_src_list))
