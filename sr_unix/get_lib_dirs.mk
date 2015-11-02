#################################################################
#								#
#	Copyright 2002, 2008 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
############### Define platform-specific directory ring-down ##################################

##################################
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

gt_os_type=$(shell uname -s)
ifndef usertype
ifeq ($(gt_os_type), OSF1)
common_dirs_sp=unix_gnp unix_cm unix port_cm port
else
ifeq ($(gt_os_type), HP-UX)
common_dirs_sp=unix_gnp unix_cm unix port_cm port
else
ifeq ($(gt_os_type), SunOS)
common_dirs_sp=unix_gnp unix_cm unix port_cm port
else
ifeq ($(gt_os_type), AIX)
common_dirs_sp=unix_gnp unix_cm unix port_cm port
else
ifeq ($(gt_os_type), OS/390)
common_dirs_sp=unix_gnp unix_cm unix_nsb unix port_cm port
else
ifeq ($(findstring CYGWIN,$(gt_os_type)), CYGWIN)
common_dirs_sp=unix_gnp unix_cm unix_nsb unix port_cm port
else
ifeq ($(gt_os_type), Linux)
ifeq ($(linux_build_type),32)
common_dirs_sp=unix_gnp unix_cm unix_nsb unix port_cm port
else
common_dirs_sp=unix_gnp unix_cm unix port_cm port
endif
endif
endif
endif
endif
endif
endif
endif
ifeq ($(gt_os_type), SunOS)
lib_dirs_sp=sun sparc $(common_dirs_sp)
endif

ifeq ($(gt_os_type), AIX)
lib_dirs_sp=aix rs6000 $(common_dirs_sp)
endif

ifeq ($(gt_os_type), OSF1)
lib_dirs_sp=dux alpha $(common_dirs_sp)
endif

ifeq ($(findstring CYGWIN,$(gt_os_type)), CYGWIN)
lib_dirs_sp=linux i386 x86_regs $(common_dirs_sp)
endif

ifeq ($(gt_os_type), Linux)
ifeq ($(gt_machine_type),s390)
lib_dirs_sp=linux l390 $(common_dirs_sp)
else
ifeq ($(gt_machine_type), ia64)
lib_dirs_sp=linux ia64 $(common_dirs_sp)
else
ifeq ($(linux_build_type),32)
lib_dirs_sp=linux i386 x86_regs $(common_dirs_sp)
else
lib_dirs_sp=linux x86_64 x86_regs $(common_dirs_sp)
endif
endif
endif
endif

ifeq ($(gt_os_type), HP-UX)
ifeq ($(gt_machine_type),ia64)
lib_dirs_sp=hpux ia64 $(common_dirs_sp)
else
lib_dirs_sp=hpux hppa $(common_dirs_sp)
endif
endif

ifeq ($(gt_os_type), OS/390)
lib_dirs_sp=os390 s390 $(common_dirs_sp)
endif
gt_src_list:=$(addprefix sr_, $(lib_dirs_sp))
else ### inhouse
gt_src_list:=src inc tools pct
endif
