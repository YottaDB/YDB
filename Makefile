#################################################################
#								#
#	Copyright 2001 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
# these values are base on those in sr_linux/gtm_env_sp.csh
AR=ar
ARFLAGS=rv
#gt_ar_option_create=qSv
#gt_ar_option_update=rv
#gt_ar_use_ranlib=yes

gt_as_assembler=as
gt_as_option_debug=--gstabs
gt_as_option_DDEBUG=
gt_as_option_nooptimize=
gt_as_option_optimize=
gt_as_src_suffix=.s

gt_cc_option_DDEBUG=-DDEBUG
gt_cc_option_debug=-g
gt_cc_option_nooptimize=
gt_cc_option_optimize=
gt_cc_compiler=cc -pipe

gt_ld_linker=cc
gt_ld_options_common=
gt_ld_syslibs=-lcurses -lm -ldl
gt_ld_sysrtns=
gt_ld_shl_linker=cc
gt_ld_shl_suffix=.so
gt_ld_shl_options=-shared

# use environment or command line override to build a subset
buildtypes=dbg bta pro

ifndef DIR
DIR := $(CURDIR)
endif

src_list=sr_linux sr_i386 sr_unix_cm sr_unix sr_port_cm sr_port

gt_cc_option_I=$(addprefix -I$(DIR)/, $(src_list))
gt_cc_dep_option=-w
gt_as_option_I=$(gt_cc_option_I)
gt_as_options_common=$(gt_as_option_I) $(gt_as_option_DDEBUG) $(gt_as_option_nooptimize)
gt_cc_options_common=$(gt_cc_option_I) -ansi -Wimplicit -Wmissing-prototypes -DFULLBLOCKWRITES -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
VPATH=$(DIR) $(addprefix $(DIR)/, $(src_list))

lib_list=dse lke mupip gtcm stub mumps
aux_list=dse geteuid gtm_dmna gtmsecshr lke mupip gtcm_server gtcm_play gtcm_pkdisp gtcm_shmclean semstat2 ftok


mfile_list=$(sort $(notdir $(wildcard $(DIR)/sr_*/*.m)))
mptfile_list=$(sort $(notdir $(wildcard $(DIR)/sr_*/*.mpt)))
helpfile_list=$(sort $(notdir $(wildcard $(DIR)/sr_*/*.hlp)))
mfile_targets:=$(addsuffix .m,$(foreach f,$(basename $(mfile_list)), $(shell echo $(f) | tr '[:lower:]' '[:upper:]')))
mptfile_targets:=$(addprefix _,$(addsuffix .m,$(foreach f,$(basename $(mptfile_list)), $(shell echo $(f) | tr '[:lower:]' '[:upper:]'))))
sh_list=$(sort $(notdir $(wildcard $(DIR)/sr_*/*.sh)))
csh_files=$(sort $(notdir $(wildcard $(DIR)/sr_*/lower*.csh) $(wildcard $(DIR)/sr_*/upper*.csh) ))
gtc_list=$(sort $(notdir $(wildcard $(DIR)/sr_*/*.gtc)))
hfile_list= gtm_stdio.h gtm_stdlib.h gtm_string.h gtmxc_types.h
sh_targets:=$(basename $(sh_list))
cfile_list=$(sort $(notdir $(wildcard $(DIR)/sr_*/*.c)))
sfile_list=$(sort $(notdir $(wildcard $(DIR)/sr_*/*.s)))
ofile_list=$(addsuffix .o,$(sort $(basename $(cfile_list)) $(basename $(sfile_list))))
msgfile_list=$(sort $(notdir $(wildcard $(DIR)/sr_*/*.msg)))
msgcfile_list=$(addsuffix .c,$(basename $(msgfile_list)))
msgofile_list=$(addsuffix .o,$(basename $(msgcfile_list)))
list_files=$(sort $(notdir $(wildcard $(DIR)/sr_*/*list)))
list_file_libs=$(addsuffix .a,$(basename $(list_files)))

#
# dynamic depend list - weed out .s based .o's
#
dep_list=$(addsuffix .c,$(filter-out $(basename $(sfile_list)),$(basename $(cfile_list))))

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
gtcm_play_obj=gtcm_play.o omi_sx_play.o
gtcm_pkdisp_obj=gtcm_pkdisp.o
gtcm_shmclean_obj=gtcm_shmclean.o
dtgbldir_obj=dtgbldir.o

# exclude .o's used in ld's below, plus dtgbldir.o (which doesn't appear to be
# used anywhere!
non_mumps_objs:=$(addsuffix .o,$(shell cat $(foreach d,$(src_list),$(wildcard $(DIR)/$(d)/*.list))))
exclude_list= \
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
	$(gtcm_play_obj) \
	$(gtcm_pkdisp_obj) \
	$(gtcm_shmclean_obj) \
	$(dtgbldir_obj)

libmumps_obj=$(sort $(filter-out $(exclude_list),$(ofile_list)) $(msgofile_list))

ifdef CURRENT_BUILDTYPE

# rules, lists, variables specific to each type of build

gtm_dist=$(DIR)/$(CURRENT_BUILDTYPE)

ifeq ($(CURRENT_BUILDTYPE), pro)
gt_cc_options=$(gt_cc_option_optimize) $(gt_cc_options_common)
gt_as_options=$(gt_as_option_optimize) $(gt_as_options_common)
gt_ld_options_buildsp=
endif
ifeq ($(CURRENT_BUILDTYPE), bta)
gt_cc_options=$(gt_cc_option_DDEBUG) $(gt_cc_option_optimize) $(gt_cc_options_common)
gt_as_options=$(gt_as_option_DDEBUG) $(gt_as_option_optimize) $(gt_as_options_common)
gt_ld_options_buildsp=
endif
ifeq ($(CURRENT_BUILDTYPE), dbg)
gt_cc_options=$(gt_cc_option_DDEBUG) $(gt_cc_option_debug) $(gt_cc_options_common)
gt_as_options=$(gt_as_option_DDEBUG) $(gt_as_option_debug) $(gt_as_options_common)
gt_ld_options_buildsp=-g
endif

gt_ld_options=$(gt_ld_options_common) $(gt_ld_options_buildsp) -L$(CURDIR)

ifeq ($(PASS),2)
PASS2ITEMS=dotcsh dotsh helpfiles hfiles gtcmconfig \
	../mumps.gld ../gtmhelp.dat ../gdehelp.dat
endif

all:	messagefiles omi_sx_play.o $(ofile_list) \
	$(list_file_libs) libmumps.a ../mumps mfiles mcompiles $(addprefix ../,$(aux_list)) \
	$(PASS2ITEMS)

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

# executables

define gt-ld
$(gt_ld_linker) $(gt_ld_options) -o $@ $^ $(gt_ld_syslibs)
endef
../mumps: $(mumps_obj) $(gt_ld_systrns) -lmumps -lstub
	$(gt-ld)

../dse: $(dse_obj) $(gt_ld_systrns) -ldse -lmumps -lstub
	$(gt-ld)

../geteuid: $(geteuid_obj) $(gt_ld_sysrtns) -lmumps
	$(gt-ld)

../gtm_dmna: $(gtm_dmna_obj) $(gt_ld_sysrtns) -lmumps -lstub
	$(gt-ld)

../gtmsecshr: $(gtmsecshr_obj) $(gt_ld_sysrtns) -lmumps
	$(gt-ld)

../lke: $(lke_obj) $(gt_ld_sysrtns) -llke -lmumps -lstub
	$(gt-ld)

../mupip: $(mupip_obj) $(gt_ld_sysrtns) -lmupip -lmumps -lstub
	$(gt-ld)

../gtcm_server: $(gtcm_server_obj) $(gt_ld_sysrtns) -lgtcm -lmumps -lstub
	$(gt-ld)

../gtcm_play: $(gtcm_play_obj) $(gt_ld_sysrtns) -lgtcm -lmumps -lstub
	$(gt-ld)

../gtcm_pkdisp: $(gtcm_pkdisp_obj) $(gt_ld_sysrtns) -lgtcm -lmumps -lstub
	$(gt-ld)

../gtcm_shmclean: $(gtcm_shmclean_obj) $(gt_ld_sysrtns) -lgtcm -lmumps -lstub
	$(gt-ld)

../semstat2: $(semstat2_obj) $(gt_ld_sysrtns)
	$(gt-ld)

../ftok: $(ftok_obj) $(gt_ld_sysrtns)
	$(gt-ld)

define update-lib
@if [ -s $*.a.updates ]; then cat $*.a.updates | xargs $(AR) $(ARFLAGS) $@;cat /dev/null > $*.a.updates; fi
endef

libmumps.a: libmumps.a($(libmumps_obj)) libmumps.a.updates
	$(update-lib)

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
# autodepend files for M files
#
-include $(mfile_list:.m=.mdep)
#
# autodepend files for mpt files
#
-include $(mptfile_list:.mpt=.mptdep)
#
# autodepend files for C files
#
-include $(dep_list:.c=.d)
#
# autodepend files for .a files
#
-include $(list_files:.list=.ldep)

%.ldep:%.list
	@echo $*.a\:$<" "$*.a"("\$$"(addsuffix .o,"\$$"(shell cat "$<"))) "$*.a.updates > $@
	@echo -e "\t"\$$"(update-lib)" >> $@
%.d: %.c
	@set -e; $(gt_cc_compiler) -M $(gt_cc_options) $(gt_cc_dep_option) $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
	[ -s $@ ] || rm -f $@
%.mdep:%.m
	@echo ../$(shell echo $* | tr '[:lower:]' '[:upper:]').m: $< > $@
	@echo -e "\t"cp \$$"< "\$$"@" >> $@
%.mptdep:%.mpt
	@echo ../_$(shell echo $* | tr '[:lower:]' '[:upper:]').m: $< > $@
	@echo -e "\t"cp \$$"< "\$$"@" >> $@

# modify default rule to create a TODO file
# so that ar updates can be batched via xargs
(%): %
	@if [ ! -f $@ ]; then \
		$(AR) $(ARFLAGS) $@ $< ;\
		> $@.updates; \
	else \
		echo $< >> $@.updates; \
	fi

ifeq ($(PASS),1)
# use the c program
%.o:%.msg
	$(DIR)/parsemsg $< > $*.c && $(gt_cc_compiler) -c $(gt_cc_options) $*.c -o $@ && rm -f $*.c
else
# use the mumps program we just compiled
%.o:%.msg
	gtm_dist=$(gtm_dist);export gtm_dist;cd $(gtm_dist); \
		echo "Do ^MSG" > msgtemp; \
		echo $^ >> msgtemp; \
		echo Halt >> msgtemp; \
		cat msgtemp | ./mumps -direct; \
		rm -f msgtemp; \
		mv $* $(CURDIR)/$*.c;
	$(gt_cc_compiler) -c $(gt_cc_options) $*.c -o $@ && rm -f $*.c
endif

%.o:%.s
	$(gt_as_assembler) $(gt_as_options) $< -o $@
%.o:%.c
	$(gt_cc_compiler) -c $(gt_cc_options) $< -o $@

omi_sx_play.c: omi_srvc_xct.c
	cp $< $@

else

# top level make - builds directory structure, calls make for each build type,
# and creates package

all: parsemsg $(addsuffix /obj, $(buildtypes)) $(addsuffix _build, $(buildtypes))

clean: $(addsuffix _clean, $(buildtypes))
	rm -f parsemsg idtemp ostemp

package: $(addsuffix _tar, $(buildtypes))

%_tar: release_name.h
	grep RELEASE_NAME $< | awk '{print $$4}' | sed 's/[\.]//' | sed 's/-//' > idtemp
	grep RELEASE_NAME $< | awk '{print $$5}' | tr '[:upper:]' '[:lower:]' > ostemp
	cd $*;tar -zcvf ../gtm_`cat ../idtemp`_$*_`cat ../ostemp`.tar.gz `cat ../tar.list`
	rm -f idtemp ostemp 

%_build:
	$(MAKE) -C $*/obj -f $(CURDIR)/Makefile DIR=$(CURDIR) CURRENT_BUILDTYPE=$* PASS=1
	rm -f $(addprefix $*/obj/, $(msgofile_list))
	$(MAKE) -C $*/obj -f $(CURDIR)/Makefile DIR=$(CURDIR) CURRENT_BUILDTYPE=$* PASS=2
%_clean:
	rm -rf $*
	rm -f *$**.tar.gz
%/obj:
	mkdir -p $@

endif

