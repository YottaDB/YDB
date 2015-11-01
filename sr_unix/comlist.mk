#################################################################
#								#
#	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# Commands to build GT.M downloaded from SourceForge
# 1. 'cd' to the GT.M directory where sr_* directories are copied to.
# 2. Define an environment variable gtm_dist to point to a prior GT.M installation.
#    (download and install GT.M binary distribution from SourceForge if you do not have
#    GT.M installed already).
# 3. To build debug version with no compiler optimzations -
# 		gmake -f sr_unix/comlist.mk -I./sr_unix buildtypes=dbg
#    To build a version enabling optimizations -
#    		gmake -f sr_unix/comlist.mk -I./sr_unix buildtypes=pro
#

# get_lib_dirs.mk must be in the same directory as this makefile
verbose ?= 0
include get_lib_dirs.mk

ifndef buildtypes
buildtypes=pro
endif

ifndef CURDIR
CURDIR=$(shell pwd)
endif

ifndef gtm_ver
gtm_ver=$(CURDIR)
endif

gt_ar_archiver=ar
gt_ar_options=rv

ifneq ($(MAKELEVEL),0)

include gtm_env_sp.mk


VPATH=$(addprefix $(gtm_ver)/, $(gt_src_list))
gt_cc_option_I:=$(gt_cc_option_I) $(addprefix -I$(gtm_ver)/, $(gt_src_list)) -I$(CURDIR)
gt_as_option_I:=$(gt_cc_option_I)

aux_list=dse geteuid gtm_dmna gtmsecshr lke mupip gtcm_server gtcm_gnp_server gtcm_play gtcm_pkdisp gtcm_shmclean semstat2 ftok


# all files based on gt_src_list
# NOTE: sort/notdir will weed out duplicates
allfiles_list:=$(sort $(notdir $(foreach d,$(gt_src_list),$(wildcard $(gtm_ver)/$(d)/*))))

# suffix specific lists filtered from allfiles_list

# m file stuff.  These list builds go to great pain to insure that either post cms_load
# forms and pre-cms load forms work.
mfile_list:=$(filter-out _%.m, $(filter %.m, $(allfiles_list)))
mptfile_list:=$(sort $(basename $(filter %.mpt, $(allfiles_list))) $(basename $(patsubst _%.m, %, $(filter _%.m, $(allfiles_list)))))
mfile_targets:=$(addsuffix .m,$(foreach f,$(basename $(mfile_list)), $(shell echo $(f) | tr '[:lower:]' '[:upper:]')))
mptfile_targets:=$(addprefix _,$(addsuffix .m, $(foreach f,$(mptfile_list), $(shell echo $(f) | tr '[:lower:]' '[:upper:]'))))

cfile_list:=$(filter %.c, $(allfiles_list))

ifdef gt_as_src_from_suffix
#
# DUX requires .m64 to be gawk'ed and assembled as well
#
sfile_list:=$(filter %$(gt_as_src_suffix) %$(gt_as_src_from_suffix), $(allfiles_list))
else
sfile_list:=$(filter %$(gt_as_src_suffix), $(allfiles_list))
endif

helpfile_list:=$(filter %.hlp, $(allfiles_list))
sh_list:=$(filter %.sh, $(allfiles_list))
gtc_list:=$(filter %.gtc, $(allfiles_list))
csh_files:=$(filter lower%.csh upper%.csh, $(allfiles_list))
list_files:=$(filter %.list, $(allfiles_list))
msgfile_list:=$(filter %.msg, $(allfiles_list))

hfile_list= gtm_stdio.h gtm_stdlib.h gtm_string.h gtmxc_types.h
sh_targets:=$(basename $(sh_list))

msgcfile_list=$(addsuffix _ctl.c,$(basename $(msgfile_list)))
msgofile_list=$(addsuffix .o,$(basename $(msgcfile_list)))
list_file_libs=$(addsuffix .a,$(basename $(list_files)))



# object files
# NOTE: sort/basename weeds out .s, .c duplication and
#       rules giving %.s priority over %.c cause the %.s
#       version to always be used
ofile_list:=$(addsuffix .o,$(sort $(basename $(cfile_list)) $(basename $(sfile_list))))
# Since MAKE compiles all source files, 'unsupported_modules' facilitates to
# avoid compilation of a list of modules to be excluded from the build.
ofile_list:=$(filter-out $(unsupported_modules), $(ofile_list))

#
# dynamic depend list - weed out .s based .o's
#
dep_list:=$(addsuffix .d,$(filter-out $(basename $(sfile_list)),$(basename $(cfile_list))))

# objects on link command lines
mumps_obj=gtm.o mumps_clitab.o
lke_obj=lke.o lke_cmd.o
dse_obj=dse.o dse_cmd.o
mupip_obj=mupip.o mupip_cmd.o
gtm_dmna_obj=daemon.o
gtmsecshr_obj=gtmsecshr.o
geteuid_obj=geteuid.o
semstat2_obj=semstat2.o
ftok_obj=ftok.o
gtcm_server_obj=gtcm_main.o omi_srvc_xct.o
gtcm_gnp_server_obj=gtcm_gnp_server.o gtcm_gnp_clitab.o
gtcm_play_obj=gtcm_play.o omi_sx_play.o
gtcm_pkdisp_obj=gtcm_pkdisp.o
gtcm_shmclean_obj=gtcm_shmclean.o
dtgbldir_obj=dtgbldir.o

# exclude .o's in .list files, .o's used in ld's below, plus dtgblkdir.o (which doesn't appear to be
# used anywhere!
non_mumps_objs:=$(addsuffix .o,$(shell cat $(foreach d,$(gt_src_list),$(wildcard $(gtm_ver)/$(d)/*.list))))
exclude_list:= \
	$(non_mumps_objs) \
	$(mumps_obj) \
	$(lke_obj) \
	$(dse_obj) \
	$(mupip_obj) \
	$(gtm_dmna_obj) \
	$(gtmsecshr_obj) \
	$(geteuid_obj) \
	$(semstat2_obj) \
	$(ftok_obj) \
	$(gtcm_server_obj) \
	$(gtcm_gnp_server_obj) \
	$(gtcm_play_obj) \
	$(gtcm_pkdisp_obj) \
	$(gtcm_shmclean_obj) \
	$(dtgbldir_obj)



# rules, lists, variables specific to each type of build

ifndef gtm_dist
gtm_dist=$(gtm_ver)/$(CURRENT_BUILDTYPE)
endif

gt_cc_option_DDEBUG=-DDEBUG
ifeq ($(CURRENT_BUILDTYPE), pro)
gt_cc_options=$(gt_cc_option_optimize) $(gt_cc_options_common)
gt_as_options=$(gt_as_option_optimize) $(gt_as_options_common)
gt_ld_options_buildsp=$(gt_ld_options_pro)
endif
ifeq ($(CURRENT_BUILDTYPE), bta)
gt_cc_options=$(gt_cc_option_DDEBUG) $(gt_cc_option_optimize) $(gt_cc_options_common)
gt_as_options=$(gt_as_option_DDEBUG) $(gt_as_option_optimize) $(gt_as_options_common)
gt_ld_options_buildsp=$(gt_ld_options_bta)
endif
ifeq ($(CURRENT_BUILDTYPE), dbg)
gt_cc_options=$(gt_cc_option_DDEBUG) $(gt_cc_option_debug) $(gt_cc_options_common)
gt_as_options=$(gt_as_option_DDEBUG) $(gt_as_option_debug) $(gt_as_options_common)
gt_ld_options_buildsp=$(gt_ld_options_dbg)
endif
gt_cc_options += $(gt_cc_option_I)
gt_as_options += $(gt_cc_option_I)

gt_ld_options=$(gt_ld_options_common) $(gt_ld_options_buildsp) -L$(CURDIR)

gt_cpus ?= 2

ifdef gt_ar_gtmrpc_name
gt_ar_gtmrpc_name_target=../lib$(gt_ar_gtmrpc_name).a
endif

ifeq ($(PASS),2)
PASS2ITEMS=$(gt_ar_gtmrpc_name_target) dotcsh dotsh helpfiles hfiles gtcmconfig \
	../mumps.gld ../gtmhelp.dat ../gdehelp.dat
endif

all:	testit messagefiles compiles \
	$(list_file_libs) libmumps.a links mfiles mcompiles \
	$(PASS2ITEMS)

testit:
	echo $(PASS2ITEMS)

links: ../mumps $(addprefix ../,$(aux_list))

ifeq ($(MAKELEVEL),1)
compiles:
	$(MAKE) -j $(gt_cpus) -f $(MYMAKE) -$(MAKEFLAGS) gtm_ver=$(gtm_ver) PASS=$(PASS) CURRENT_BUILDTYPE=$(CURRENT_BUILDTYPE) depends compiles

.PRECIOUS: omi_sx_play.o $(ofile_list)

endif
ifeq ($(MAKELEVEL),2)

depends: $(dep_list)

vars:
	echo MAKECMDGOALS $(MAKECMDGOALS)

compiles: omi_sx_play.o $(ofile_list)

#links: ../mumps $(addprefix ../,$(aux_list))

#
# autodepend files for C files
#
-include $(dep_list)

endif

../mumps.gld:
	cd ..;gtm_dist=$(gtm_dist);export gtm_dist;gtmgbldir=./$(notdir $@);export gtmgbldir;\
		echo exit | ./mumps -run GDE

define compile-help
cd ..;gtm_dist=$(gtm_dist);export gtm_dist;gtmgbldir=$(gtm_dist)/$(notdir $(basename $@));export gtmgbldir; \
	echo Change -segment DEFAULT -block=2048 -file=$(gtm_dist)/$(notdir $@) > hctemp;  \
	echo Change -region DEFAULT -record=1020 -key=255 >> hctemp; \
	echo exit >> hctemp; \
	cat hctemp | ./mumps -run GDE; \
	./mupip create; \
	echo "Do ^GTMHLPLD" > hctemp; \
	echo $(gtm_dist)/$(notdir $^) >> hctemp; \
	echo Halt >> hctemp; \
	cat hctemp | ./mumps -direct; \
	rm -f hctemp
endef
../gtmhelp.dat: ../mumps.hlp
	$(compile-help)
../gdehelp.dat: ../gde.hlp
	$(compile-help)

mcompiles:
	cd ..;gtm_dist=$(dir $(CURDIR));export gtm_dist;gtmgbldir=$(notdir $@);export gtmgbldir;\
		./mumps *.m

dotcsh: $(csh_files)
	cp -f $^ ..
	cd ..;chmod +x $(notdir $^)

dotsh: $(sh_targets)
	cp -f $^ ..

helpfiles: $(helpfile_list)
	cp -pf $^ ..

hfiles: $(hfile_list)
	cp -f $^ ..

mfiles: $(addprefix ../, $(mfile_targets) $(mptfile_targets))

$(list_file_libs): $(list_files)

ifdef gt_ar_gtmrpc_name_target
$(gt_ar_gtmrpc_name_target): lib$(gt_ar_gtmrpc_name).a
	cp $< $@
endif

# executables

define gt-ld
rm -f $@
$(gt_ld_linker) $(gt_ld_options) -o $@ $(gt_ld_sysrtns) $^ $(gt_ld_syslibs)
endef
../mumps: $(mumps_obj) -lmumps -lgnpclient -lcmisockettcp $(gt_ld_gtmrpc_library_option)
	$(gt-ld) $(gt_ld_options_symbols) > ../map/$(notdir $@).map 2>&1

../dse: $(dse_obj) -ldse -lmumps -lstub $(gt_ld_gtmrpc_library_option)
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../geteuid: $(geteuid_obj) -lmumps
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../gtm_dmna: $(gtm_dmna_obj) -lmumps
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../gtmsecshr: $(gtmsecshr_obj) -lmumps
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../lke: $(lke_obj) -llke -lmumps -lstub $(gt_ld_gtmrpc_library_option)
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../mupip: $(mupip_obj) -lmupip -lmumps -lstub $(gt_ld_gtmrpc_library_option)
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../gtcm_server: $(gtcm_server_obj) -lgtcm -lmumps -lstub $(gt_ld_gtmrpc_library_option)
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../gtcm_gnp_server: $(gtcm_gnp_server_obj) -lgnpserver -llke -lmumps -lcmisockettcp -lstub  $(gt_ld_gtmrpc_library_option)
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../gtcm_play: $(gtcm_play_obj) -lgtcm -lmumps -lstub $(gt_ld_gtmrpc_library_option)
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../gtcm_pkdisp: $(gtcm_pkdisp_obj) -lgtcm -lmumps -lstub $(gt_ld_gtmrpc_library_option)
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../gtcm_shmclean: $(gtcm_shmclean_obj) -lgtcm -lmumps -lstub $(gt_ld_gtmrpc_library_option)
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../semstat2: $(semstat2_obj)
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

../ftok: $(ftok_obj)
	$(gt-ld) > ../map/$(notdir $@).map 2>&1

define update-lib
@if [ -s $*.a.updates ]; then cat $*.a.updates | xargs $(gt_ar_archiver) $(gt_ar_options) $@;cat /dev/null > $*.a.updates; fi
endef

libmumps_obj:=$(sort $(filter-out $(exclude_list),$(ofile_list)) $(msgofile_list))
libmumps.a: $(libmumps_obj)
	@echo Processing $@
ifdef gt_smallarglist
	@echo $(wordlist 1, 500, $?) | xargs $(gt_ar_archiver) $(gt_ar_options) $@
	@echo $(wordlist 501, 1000, $?) | xargs $(gt_ar_archiver) $(gt_ar_options) $@
	@echo $(wordlist 1001, 1500, $?) | xargs $(gt_ar_archiver) $(gt_ar_options) $@
	@echo $(wordlist 1501, 2000, $?) | xargs $(gt_ar_archiver) $(gt_ar_options) $@
	@echo $(wordlist 2001, 2500, $?) | xargs $(gt_ar_archiver) $(gt_ar_options) $@
else
	$(gt_ar_archiver) $(gt_ar_options) $@ $?
endif

messagefiles: $(msgofile_list)

gtcmconfig: $(gtc_list) gtcm_gcore
	cp -f $^ ..
	cd ..;chmod a-wx $(notdir $^);mv -f configure.gtc configure
	cd ..;touch gtmhelp.dmp;chmod a+rw gtmhelp.dmp

test_type:
ifndef gt_cc_options
	$(error CURRENT_BUILDTYPE not properly defined)
endif

#
# autodepend to locate directory containing msg.m
# and to set the path of msg.m into msgdir
-include msgdir.mk
#
# autodepend files for M files
#
-include $(mfile_list:.m=.mdep)
#
# autodepend files for mpt files
#
-include $(mptfile_list:=.mptdep)
#
# autodepend files for .a files
#
-include $(list_files:.list=.ldep)

%.d:%.c
	$(gt-dep)

%.ldep:%.list
	@echo $*.a\:$$\(addsuffix .o,$$\(shell cat $<\)\) > $@
	@$(gt_echoe) "\t@echo Processing "$$\@ >> $@
	@$(gt_echoe) "\t@echo "$$\(filter-out %.list,$$\?\) "| xargs $(gt_ar_archiver) $(gt_ar_options) "$$\@ >> $@
%.mdep:%.m
	@echo ../$(shell echo $* | tr '[:lower:]' '[:upper:]').m: $< > $@
	@$(gt_echoe) "\t"cp $$\< $$\@ >> $@
%.mptdep:_%.m
	@echo ../_$(shell echo $* | tr '[:lower:]' '[:upper:]').m: $< > $@
	@$(gt_echoe) "\t"cp $$\< $$\@ >> $@
%.mptdep:%.mpt
	@echo ../_$(shell echo $* | tr '[:lower:]' '[:upper:]').m: $< > $@
	@$(gt_echoe) "\t"cp $$\< $$\@ >> $@
msgdir.mk: msg.m
	@echo msgdir=$(dir $<) > $@

%_ctl.o:%.msg
ifndef gtm_curpro
# use the mumps program we just compiled
	$(gtm_dist)/mumps $(msgdir)/msg.m
	$(gtm_dist)/mumps -run msg $< unix
else
# use the gtm_curpro mumps program
	$(shell gtm_dist=$(gtm_root)/$(gtm_curpro)/pro;export gtm_dist;\
		$(gtm_root)/$(gtm_curpro)/pro/mumps $(msgdir)/msg.m;\
		$(gtm_root)/$(gtm_curpro)/pro/mumps -run msg $< unix)
endif
	rm -f msg.o
	$(gt_cc_compiler) -c $(gt_cc_options) $*_ctl.c -o $@ && rm -f $*_ctl.c

# By default, .s files are preferred to .c files if both versions exist for a module.
# To reverse this behavior, gt_cc_before_as should be defined to the list of .o files
# to be compiled from .c instead of from .s files.
ifdef gt_cc_before_as
$(gt_cc_before_as):%.o:%.c	#override rules for gt_cc_before_as modules only
ifeq ($(verbose),1)
	$(gt_cc_compiler) -c $(gt_cc_options) $< -o $@
else
	@echo $<
	@$(gt_cc_compiler) -c $(gt_cc_options) $< -o $@
endif
endif

ifdef gt_as_src_from_suffix
%.o:%$(gt_as_src_from_suffix)
ifeq ($(verbose),1)
	$(gt-as-convert)
else
	@echo $<
	@$(gt-as-convert)
endif
endif
%.o:%$(gt_as_src_suffix)
ifeq ($(verbose),1)
	$(gt-as)
else
	@echo $<
	@$(gt-as)
endif

ifndef gt_cc_before_as
%.o:%.c
ifeq ($(verbose),1)
	$(gt_cc_compiler) -c $(gt_cc_options) $< -o $@
else
	@echo $<
	@$(gt_cc_compiler) -c $(gt_cc_options) $< -o $@
endif
endif

omi_sx_play.c: omi_srvc_xct.c
	@cp $< $@

else

# top level make - builds directory structure, calls make for each build type,
# and creates package

VPATH=$(addprefix $(gtm_ver)/, $(gt_src_list))
make_i_flags=$(addprefix -I$(gtm_ver)/, $(gt_src_list))

all:	$(addprefix $(gtm_ver)/, $(addsuffix /obj, $(buildtypes))) \
	$(addprefix $(gtm_ver)/, $(addsuffix /map, $(buildtypes))) \
	$(addsuffix _build, $(buildtypes))

clean: $(addsuffix _clean, $(buildtypes))
	rm -f idtemp ostemp

package: $(addsuffix _tar, $(buildtypes))

%_tar: release_name.h
	@echo "packaging GT.M..."
	@grep RELEASE_NAME $< | awk '{print $$4}' | sed 's/[\.]//' | sed 's/-//' > idtemp
	@grep RELEASE_NAME $< | awk '{print $$5}' | tr '[:upper:]' '[:lower:]' > ostemp
	@tar -zcvf gtm_`cat idtemp`_`cat ostemp`_$*.tar.gz -C $* $(filter-out obj map, $(notdir $(wildcard $*/*)))
	@rm -f idtemp ostemp

%_build: comlist.mk
ifndef gtm_curpro
	$(MAKE) -C $(gtm_ver)/$*/obj -I$(gtm_ver)/$*/obj $(make_i_flags) -f $< gtm_ver=$(gtm_ver) CURRENT_BUILDTYPE=$* PASS=1 MYMAKE=$<
	rm -f $(addprefix $(gtm_ver)/$*/obj/, $(msgofile_list))
endif
	$(MAKE) -C $(gtm_ver)/$*/obj -I$(gtm_ver)/$*/obj $(make_i_flags) -f $< gtm_ver=$(gtm_ver) CURRENT_BUILDTYPE=$* PASS=2 MYMAKE=$<
%_clean:
	rm -rf $(gtm_ver)/$*
	rm -f $(gtm_ver)/*$*.tar.gz
%/obj:
	mkdir -p $@
%/map:
	mkdir -p $@

endif

