#
#  Makefile for gtcm_server (OMI/EMI/RC)
#
#	for just OMI/EMI, comment out the values of RCO and RCLIB
#
#  $Header:$
#
#  This makefile requires you to use the "ver" command to set the
#  current GT.M version prior to execution.
#

LD	= $(CC)
SHELL	= /bin/sh
# MAKE	= /bin/make

GO	= $(gtm_obj)
DEST	= $(PWD)
GTCMLIBPATH = -Lrc -Lomi -L$(gtm_dist)/obj
IPATH	= -I$(gtm_inc) -I$(gtm_vrt)/gtcm/inc

GTCMLDFLAGS=$(LDFLAGS)
GTCMLIBS = -lomi -lrc -lmumps -lstub
SHMLIBS = -lmumps
GTMLIBS = -lmupip -ldse -lmumps -lstub -lm -ltermcap
# MUPLIBS = -lmupip -ldse -lmumps
# DSELIBS = -ldse -lmumps

# Add -DRC if you want to compile in RC
# must also add to makefile in omi directory

GTCMINSTALLABLES=	mumps mupip dse gtcm_server shmclean gtcm_pkdisp \
	gtcm_player gtcm_slist.gtc gtcm_run.gtc gtcm_gcore

OBJS	= gtcm_init.o    gtcm_prsopt.o  gtcm_maxfds.o  \
	  gtcm_bgn_net.o gtcm_end_net.o gtcm_cn_acpt.o gtcm_cn_disc.o \
	  gtcm_term.o    gtcm_dmpstat.o gtcm_rep_err.o gtcm_exit_ch.o \
	  gtcm_exit.o    gtcm_loop.o	gtcm_pktdmp.o  gtcm_ping.o    \
	  gtcm_hist.o	 rc_cpt_ops.o

RCFILES = 		rc_dbms_ch.c	rc_fnd_file.c	rc_frmt_lck.c \
	rc_gbl_ord.c	rc_oflow.c	rc_prc_clsd.c \
	rc_prc_getp.c	rc_prc_getr.c	rc_prc_kill.c	rc_prc_lock.c \
	rc_prc_logn.c	rc_prc_opnd.c	rc_prc_set.c	rc_prc_setf.c \
	rc_rundown.c

OMIFILES = 		omi_buff_rsp.c	omi_dbms_ch.c	omi_dmp_pkt.c  \
	omi_extstr.c	omi_gvextnam.c	omi_lkextnam.c	omi_prc_conn.c \
	omi_prc_def.c	omi_prc_disc.c	omi_prc_get.c	omi_prc_incr.c \
	omi_prc_kill.c	omi_prc_lock.c	omi_prc_next.c	omi_prc_ordr.c \
	omi_prc_qry.c	omi_prc_rord.c	omi_prc_set.c	omi_prc_sete.c \
	omi_prc_setp.c	omi_prc_stat.c	omi_prc_unla.c	omi_prc_unlc.c \
	omi_prc_unlk.c

GTMOBJS =
DSEOBJS = 
MUPIPOBJS = 
DMNAOBJS = 

EXTRAOBJS = mval2subsc.o	rc_iscan.o

# RC linkage
RCO	= rc_srvc_xct.o

LIBRC = librc.a
LIBOMI = libomi.a

default	:
	@echo 'You must specify the system to make GT.CM for.'
	@echo '	make < s_aix | s_hpux | s_s2 | s_sun | sunos >'

all	: checkversion movefiles mumps mupip dse $(LIBOMI) $(LIBRC) gtcm_server shmclean gtcm_run.gtc gtcm_pkdisp gtcm_player

# Systems
s_s2:	tandem
tandem:
	@echo "Making GT.CM for the Tandem (MIPS) V.4 OS"
	@if [ `basename $(gtm_exe)` = "dbg" ]; then \
		make all \
		"CFLAGS= -signed $(gt_cc_option_debug) -DDEBUG -DTANDEM -DSHADOWPW -DRC -I/usr/include/bsd -I. " \
		"LDFLAGS= -dn" \
		"GTCMLDFLAGS=-dn -Wl,-n" \
		"SYSLIBS= -lmalloc -lsocket -lnsl -ldl" ; \
	else \
		make all \
		"CFLAGS= -signed $(gt_cc_option_optimize) -DTANDEM -DSHADOWPW -DRC -I/usr/include/bsd -I. " \
		"LDFLAGS= -dn" \
		"GTCMLDFLAGS=-dn -Wl,-n" \
		"SYSLIBS= -lmalloc -lsocket -lnsl -ldl" ; \
	fi
	@echo "make completed successfully."

sunos:
	@echo "Making GT.CM for the Sun (SPARC) running SunOS 4.x (obsolete)"
	@if [ `basename $(gtm_exe)` = "dbg" ]; then \
		make all \
		"CFLAGS= -g -DDEBUG -DSUNOS -DRC" \
		"LDFLAGS= -Bstatic -g" \
		"SYSLIBS=$(gtm_exe)/libgtmrpc.a" \
		"CC= /usr/5bin/cc" ; \
	else \
		make all \
		"CFLAGS= -O -DSUNOS -DRC" \
		"LDFLAGS= -Bstatic -O" \
		"SYSLIBS=$(gtm_exe)/libgtmrpc.a" \
		"CC= /usr/5bin/cc" ; \
	fi
	@echo "make completed successfully."

s_sun:	solaris
solaris:
	@echo "Making GT.CM for the Sun (SPARC) running Solaris 2.x"
	@if [ `basename $(gtm_exe)` = "dbg" ]; then \
		make all \
		"CFLAGS= $(gt_cc_option_debug) -DDEBUG -DSUNOS -DSHADOWPW -DRC" \
		"LDFLAGS= $(gt_cc_option_debug)" \
		"CC= $(gt_cc_compiler)" \
		"SYSLIBS=$(gtm_exe)/libgtmrpc.a -lsocket -lnsl -ldl -lposix4" ; \
	else \
		make all \
		"CFLAGS= $(gt_cc_option_optimize) -DSUNOS -DSHADOWPW -DRC" \
		"LDFLAGS= $(gt_cc_option_optimize)" \
		"CC= $(gt_cc_compiler)" \
		"SYSLIBS=$(gtm_exe)/libgtmrpc.a -lsocket -lnsl -ldl -lposix4" ; \
	fi
	@echo "make completed successfully." ;

s_aix:	rs6000
rs6000:
	@echo "Making GT.CM for the RS/6000"
	@if [ `basename $(gtm_exe)` = "dbg" ]; then \
		make all \
		"CC=svcc" \
		"LD=svcc" \
		"CFLAGS= -qlist -qchars=signed $(gt_cc_option_debug) -DDEBUG -DAIX -DRC" \
		"LDFLAGS= $(gt_cc_option_debug) -Guserte" ; \
	else \
		make all \
		"CC=svcc" \
		"LD=svcc" \
		"CFLAGS= -qlist -qchars=signed $(gt_cc_option_optimize) -DAIX -DRC" \
		"LDFLAGS= -Guserte" ; \
	fi
	@echo "make completed successfully."

s_hpux: hp
hp:
	@echo "Making GT.CM for the HP"
	@if [ `basename $(gtm_exe)` = "dbg" ]; then \
		make all \
		"CFLAGS= $(gt_cc_option_debug) -DDEBUG -DHPUX -DRC -Aa -D_HPUX_SOURCE +DA1.0 +DS1.0" \
		"LDFLAGS= $(gt_cc_option_debug)" \
		"SYSLIBS= -lV3 -ldld" ; \
	else \
		make all \
		"CFLAGS= $(gt_cc_option_optimize) -DHPUX -DRC -Aa -D_HPUX_SOURCE +DA1.0 +DS1.0" \
		"LDFLAGS=" \
		"SYSLIBS= -lV3 -ldld" ; \
	fi
	@echo "make completed successfully."

$(LIBOMI):
	@-cd omi; $(MAKE) -q $(MFLAGS) $@ ;	\
	echo "attempting to create libomi.a" ;	\
	echo $@ ;				\
	pwd ;					\
	$(MAKE) $(MFLAGS) $@ ;			\
	echo > ../.libomi

$(LIBRC):
	@-cd rc; $(MAKE) -q $(MFLAGS) $@ ;	\
	echo "attempting to create librc.a" ;	\
	echo $@ ;				\
	pwd ;					\
	$(MAKE) $(MFLAGS) $@ ;			\
	echo > ../.librc

.libomi .librc:

mval2subsc.c:	movefiles

rc_iscan.c:	movefiles

install:
	@echo "Installing GT.CM files in $(gtm_dist)..."
	cp $(GTCMINSTALLABLES) $(gtm_dist)
	@echo "Done."

checkversion:
	@if [ "$(gtm_src)" = "" ]; then \
		echo " "; \
		echo "**************************************************"; \
		echo "You must run the version command before building"; \
		echo "GT.CM."; \
		echo "**************************************************"; \
		echo " "; \
		exit 1; \
	fi

movefiles:	gtcm_main 	omi	rc

gtcm_main:
	@if [ ! -f gtcm_main.c ] ; then \
		echo "Copying source files..." ; \
		cd $(gtm_vrt)/gtcm/src ; \
		cp * $(DEST) ; \
		cd $(gtm_vrt)/gtcm/tools ; \
		cp gtcm_gcore *.gtc makefile.* $(DEST) ; \
	fi

rc:
	if [ ! -d rc ] ; then		\
		mkdir rc ;		\
	fi;				\
	mv $(RCFILES) rc ;		\
	mv makefile.rc rc/makefile;	\
	cd rc; $(MAKE) -d -q $(MFLAGS) librc.a ; pwd ;	\
	echo "attempting to create librc.a" ;	\
	echo $@ ;				\
	pwd ;					\
	echo > ../.librc

omi:
	if [ ! -d omi ] ; then		\
		mkdir omi ;		\
	fi;				\
	mv $(OMIFILES) omi ;		\
	mv makefile.omi omi/makefile ;	\
	cd omi; $(MAKE) -d -q $(MFLAGS) libomi.a ; pwd ;	\
	echo "attempting to create libomi.a" ;	\
	echo $@ ;				\
	pwd ;					\
	echo > ../.libomi
  
gtcm_server	: $(OBJS) $(EXTRAOBJS) gtcm_main.o omi_srvc_xct.o $(RCO) .libomi .librc
	$(LD) $(GTCMLDFLAGS) $(GTCMLIBPATH) -o $@ gtcm_main.o $(OBJS) \
	$(EXTRAOBJS) omi_srvc_xct.o $(RCO) $(GTCMLIBS)	$(SYSLIBS)

rc_cpt_ops.c	: $(gtm_src)/rc_cpt_ops.c
	cp $(gtm_src)/rc_cpt_ops.c .

gtcm_player	: $(OBJS) gtcm_play.o omi_sx_play.o $(RCO) .libomi .librc
	$(LD) $(LDFLAGS) $(GTCMLIBPATH) -o $@ gtcm_play.o $(OBJS) \
	 omi_sx_play.o $(RCO)  $(GTCMLIBS) $(SYSLIBS)

gtcm_pkdisp	: gtcm_disp.o
	$(LD) $(LDFLAGS) $(GTCMLIBPATH) -o $@ gtcm_disp.o -lomi -lmumps

mumps:	$(EXTRAOBJS) $(GTMOBJS)
	$(LD) $(LDFLAGS) $(GTCMLIBPATH) -o mumps $(GO)/gtm.o \
	$(GO)/mumps_clitab.o $(EXTRAOBJS) $(GTMOBJS) $(GTMLIBS) $(SYSLIBS)

dse:	 $(EXTRAOBJS) $(DSEOBJS)
	$(LD) $(LDFLAGS) $(GTCMLIBPATH) -o dse $(GO)/dse.o \
	$(GO)/dse_cmd.o $(EXTRAOBJS) $(DSEOBJS) $(GTMLIBS) $(SYSLIBS)

mupip: $(EXTRAOBJS) $(MUPIPOBJS)
	$(LD) $(LDFLAGS) $(GTCMLIBPATH) -o mupip $(GO)/mupip.o \
	$(GO)/mupip_cmd.o $(EXTRAOBJS) $(MUPIPOBJS) $(GTMLIBS) $(SYSLIBS)

# not needed for GT.CM, but here for completeness.
gtm_dmna: daemon.o $(DMNAOBJS) $(EXTRAOBJS)
	$(LD) $(LDFLAGS) $(GTCMLIBPATH) -o gtm_dmna daemon.o \
	$(DMNAOBJS) $(EXTRAOBJS) $(GTMLIBS) $(SYSLIBS)

shmclean: shmclean.o
	$(LD) $(LDFLAGS) -o $@ shmclean.o $(GTCMLIBPATH) $(SHMLIBS) $(SYSLIBS)

clean:
	rm -f *~ *.c *.gtc *.o *.a core makefile.*
	rm -f mumps mupip dse gtcm_server shmclean gtcm_pkdisp gtcm_player gtcm_gcore #*#
	rm -f .librc .libomi
	rm -rf .sb
	rm -rf rc
	rm -rf omi

tar:
	tar cvf gtcm.tar *.c *.h gtcm_run rc/*.c rc/*.h omi/*.c omi/*.h
.c.o:
	$(CC) $(CFLAGS) $(IPATH) -c $*.c 

omi_sx_play.o	: omi_srvc_xct.c $(gtm_vrt)/gtcm/inc/omi.h
	cp omi_srvc_xct.c omi_sx_play.c
	$(CC) $(CFLAGS) $(IPATH) -DFILE_TCP -c omi_sx_play.c 
	rm omi_sx_play.c

gtcm.h		: $(gtm_vrt)/gtcm/inc/gtcm_sysenv.h $(gtm_vrt)/gtcm/inc/omi.h
	touch gtcm.h

gtcm_maxfds.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_maxfds.c
gtcm_bgn_net.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_bgn_net.c
gtcm_end_net.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_end_net.c
gtcm_exit.o	: $(gtm_vrt)/gtcm/inc/gtcm.h $(gtm_inc)/rc.h gtcm_exit.c
gtcm_exit_ch.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_exit_ch.c
gtcm_hist.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_hist.c
gtcm_ping.o	: $(gtm_vrt)/gtcm/inc/omi.h gtcm_ping.c
gtcm_pktdmp.o	: $(gtm_vrt)/gtcm/inc/gtcm.h $(gtm_vrt)/gtcm/inc/gtcm_sysenv.h $(gtm_vrt)/gtcm/inc/omi.h gtcm_pktdmp.c
gtcm_prsopt.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_prsopt.c
gtcm_loop.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_loop.c
gtcm_cn_acpt.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_cn_acpt.c
gtcm_cn_disc.o	: $(gtm_vrt)/gtcm/inc/gtcm.h $(gtm_vrt)/gtcm/inc/omi.h gtcm_cn_disc.c
gtcm_term.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_term.c
gtcm_dmpstat.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_dmpstat.c
gtcm_rep_err.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_rep_err.c
gtcm_init.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_init.c
gtcm_main.o	: $(gtm_vrt)/gtcm/inc/gtcm.h gtcm_main.c
pkt_dump.o	: $(gtm_vrt)/gtcm/inc/omi.h pkt_dump.c
omi_srvc_xct.o	: $(gtm_vrt)/gtcm/inc/omi.h omi_srvc_xct.c
rc_srvc_xct.o	: $(gtm_vrt)/gtcm/inc/omi.h rc_srvc_xct.c
gtcm_play.o	: $(gtm_vrt)/gtcm/inc/omi.h gtcm_play.c
shmclean.o	: $(gtm_inc)/daemon.h $(gtm_inc)/rc_cpt.h shmclean.c
gtcm_disp.o	: $(gtm_vrt)/gtcm/inc/omi.h gtcm_disp.c
