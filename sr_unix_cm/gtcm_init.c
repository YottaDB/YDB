/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  gtcm_init.c ---
 *
 *	Init routine for the GTCM server.  Process the command line
 *	arguments and initialize everything.
 *
 */

#ifndef lint
static char rcsid[] = "$Header: /cvsroot/sanchez-gtm/gtm/sr_unix_cm/gtcm_init.c,v 1.1.1.1 2001/05/16 14:01:54 marcinim Exp $";
#endif

#include <sys/types.h>
#include <signal.h>
#include "gtm_stdio.h"

#include "mdef.h"
#include "gtcm.h"
#include "stp_parms.h"
#include "patcode.h"
#include "error.h"
#include "gtmimagename.h"
#include "stringpool.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "repl_msg.h"
#include "gtmsource.h"

GBLREF int				errno;
GBLREF short 				gtcm_ast_avail;
GBLREF bool				licensed;
GBLREF int				omi_pid;
GBLREF int4				omi_errno;
GBLREF int				history;
GBLREF pattern          		*pattern_list;
GBLREF pattern          		*curr_pattern;
GBLREF pattern          		mumps_pattern;
GBLREF uint4    			*pattern_typemask;
GBLREF bool				is_db_updater;
GBLREF enum gtmImageTypes 		image_type;
GBLREF spdesc       			rts_stringpool, stringpool;
GBLREF unsigned char 			cw_set_depth;
GBLREF cw_set_element 			cw_set[];
GBLREF uint4 				process_id;
GBLREF jnlpool_addrs 			jnlpool;
GBLREF bool 				certify_all_blocks;

/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
 * the only exception and, in particular because the operating system does not support such an exception, the argv
 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
 */
void gtcm_init(int argc, char_ptr_t argv[])
{
	char			*ptr, *getenv();
	struct sigaction 	ignore, act;
	void			gtcm_fail();
	void			get_page_size();
	void			init_secshr_addrs();
	gd_addr 		*get_next_gdr();

	/*  Disassociate from the rest of the universe */
	get_page_size();
	is_db_updater = TRUE; /* it is necessary to initialize is_db_updater for running replication */
	image_type = GTCM_SERVER_IMAGE;

	if (NULL != getenv("GTCM_GDSCERT"))
		certify_all_blocks = TRUE;
#ifndef DEBUG
	{
		int		  pid;

		if ((pid = fork()) < 0)
		{
			char msg[256];
			sprintf(msg,"Unable to detach %s from controlling tty", SRVR_NAME);
			gtcm_rep_err(msg,errno);
			exit(-1);
		}
		else if (pid > 0)
			exit(0);
		(void) setpgrp();
	}
#endif				/* !defined(DEBUG) */

	/*  Initialize logging */
#ifdef BSD_LOG
	openlog(SRVR_NAME, LOG_PID, LOG_LOCAL7);
#endif				/* defined(BSD_LOG) */

	omi_pid = getpid();

	/*  Initialize signals */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	ignore = act;
	ignore.sa_handler = SIG_IGN;

	act.sa_handler = (void (*)()) gtcm_term;
	(void) sigaction(SIGTERM, &act, 0);
	act.sa_handler = (void (*)()) gtcm_dmpstat;
	(void) sigaction(SIGUSR1, &act, 0);

	(void) sigaction(SIGUSR2, &ignore, 0);
	(void) sigaction(SIGALRM, &ignore, 0);
	(void) sigaction(SIGPIPE, &ignore, 0);
	(void) sigaction(SIGINT, &ignore, 0);
#ifdef GTCM_RC
	act.sa_handler = gtcm_fail;
	act.sa_flags = SA_RESETHAND;
	/* restore signal handler to default action upon receipt
	   of signal */
	(void) sigaction(SIGSEGV, &act, 0);
	(void) sigaction(SIGBUS, &act, 0);
	(void) sigaction(SIGILL, &act, 0);
	(void) sigaction(SIGTRAP, &act, 0);
	(void) sigaction(SIGABRT, &act, 0);
#ifndef __linux__
	(void) sigaction(SIGEMT, &act, 0);
	(void) sigaction(SIGSYS, &act, 0);
#endif
#endif

	/*  Initialize the process flags */
	gtcm_prsopt(argc, argv);

	/* Initialize history mechanism */
	if (history)
	{
		init_hist();
		act.sa_handler = dump_rc_hist;
		act.sa_flags = 0;
		(void) sigaction(SIGUSR2, &act, 0);
	}

	/*  Initialize the DBMS */
	licensed = TRUE;
	getjobnum();
	getzdir();
	if ((gtcm_ast_avail = getmaxfds()) < 0)
	{
		gtcm_rep_err("Unable to get system resource limits", errno);
		exit(errno);
	}
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	cache_init();

	curr_pattern = pattern_list = &mumps_pattern;
	pattern_typemask = mumps_pattern.typemask;

	init_secshr_addrs(get_next_gdr, cw_set, NULL, &cw_set_depth, process_id, OS_PAGE_SIZE, &jnlpool.jnlpool_dummy_reg);
	initialize_pattern_table();

	 /* Preallocate some timer blocks. */
	  prealloc_gt_timers();

	/* Moved to omi_gvextnam, omi_lkextnam */
	/*    gvinit(); */
	return;
}

/* signal handler called when the process is about to core dump */
void gtcm_fail(int sig)
{
	void rc_rundown();
        struct sigaction def;

	fprintf(stderr,"GT.CM terminating on signal %d, cleaning up...\n", sig);
	/* quickie cleanup */
	rc_rundown();

	sigemptyset(&def.sa_mask);
	def.sa_flags = 0;
	def.sa_handler = SIG_DFL;
	(void) sigaction(SIGQUIT, &def, 0);
	kill(getpid(),SIGQUIT);
        exit(sig);
}
