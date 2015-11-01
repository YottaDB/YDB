############### Define platform-specific directory ring-down ##################################

common_dirs_sp=unix_gnp unix_cm unix port_cm port
gt_os_type=$(shell uname -s)
gt_machine_type=$(shell uname -m)

ifeq ($(gt_os_type), SunOS)
lib_dirs_sp=sun sparc $(common_dirs_sp)
endif
ifeq ($(gt_os_type), AIX)
lib_dirs_sp=aix rs6000 $(common_dirs_sp)
endif
ifeq ($(gt_os_type), OSF1)
lib_dirs_sp=dux alpha $(common_dirs_sp)
endif
ifeq ($(gt_os_type), Linux)
ifeq ($(gt_machine_type),s390)
lib_dirs_sp=linux l390 $(common_dirs_sp)
else
lib_dirs_sp=linux i386 $(common_dirs_sp)
endif
endif
ifeq ($(gt_os_type), HP-UX)
lib_dirs_sp=hpux hppa $(common_dirs_sp)
endif
ifeq ($(gt_os_type), OS/390)
lib_dirs_sp=os390 s390 $(common_dirs_sp)
endif
gt_src_list:=src inc tools pct $(addprefix sr_, $(lib_dirs_sp))
