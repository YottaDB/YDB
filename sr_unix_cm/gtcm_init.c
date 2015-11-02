/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc *
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

#include "mdef.h"

#include <sys/types.h>
#include <errno.h>
#include <signal.h>

#include "gtm_stdlib.h"		/* for exit() */
#include "gtm_stdio.h"
#include "gtm_unistd.h"		/* for getpid() */

#include "gtcm.h"
#include "stp_parms.h"
#include "patcode.h"
#include "error.h"
#include "stringpool.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "getjobnum.h"		/* for getjobnum() prototype */
#include "getmaxfds.h"		/* for getmaxfds() prototype */
#include "getzdir.h"		/* for getzdir() prototype */
#include "gt_timer.h"		/* for prealloc_gt_timers() prototype */
#include "cli.h"
#include "filestruct.h"
#include "jnl.h"		/* for inctn_detail */
#include "gdskill.h"
#include "buddy_list.h"
#include "hashtab_int4.h"
#include "tp.h"
#include "init_secshr_addrs.h"
#include "fork_init.h"
#include "gtmio.h"
#include "have_crit.h"
#include "gt_timers_add_safe_hndlrs.h"
#ifdef UNICODE_SUPPORTED
# include "gtm_icu_api.h"
# include "gtm_utf8.h"
#endif

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

GBLDEF	CLI_ENTRY	*cmd_ary = NULL; /* The GTCM server does not have any command tables so initialize command array to NULL */

GBLREF short 			gtcm_ast_avail;
GBLREF bool			licensed;
GBLREF int			omi_pid;
GBLREF int4			omi_errno;
GBLREF int			history;
GBLREF pattern			*pattern_list;
GBLREF pattern			*curr_pattern;
GBLREF pattern			mumps_pattern;
GBLREF uint4			*pattern_typemask;
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF int			process_exiting;

void	gtcm_fail(int sig);

/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
 * the only exception and, in particular because the operating system does not support such an exception, the argv
 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
 */
void gtcm_init(int argc, char_ptr_t argv[])
{
	char			*ptr;
	struct sigaction 	ignore, act;
	void			get_page_size();
	int		  	pid;
	char			msg[256];
	int			save_errno, maxfds;

	/*  Disassociate from the rest of the universe */
	get_page_size();
	gtm_wcswidth_fnptr = gtm_wcswidth;
#	ifndef GTCM_DEBUG_NOBACKGROUND
	FORK_CLEAN(pid);
	if (0 > pid)
	{
		save_errno = errno;
		SPRINTF(msg, "Unable to detach %s from controlling tty", SRVR_NAME);
		gtcm_rep_err(msg, save_errno);
		exit(-1);
	}
	else if (0 < pid)
		exit(0);
	(void) setpgrp();
#	endif
	/* Initialize logging */
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
#	ifdef GTCM_RC
	act.sa_handler = gtcm_fail;
	act.sa_flags = SA_RESETHAND;
	/* restore signal handler to default action upon receipt
	   of signal */
	(void) sigaction(SIGSEGV, &act, 0);
	(void) sigaction(SIGBUS, &act, 0);
	(void) sigaction(SIGILL, &act, 0);
	(void) sigaction(SIGTRAP, &act, 0);
	(void) sigaction(SIGABRT, &act, 0);
#	ifndef __linux__
	(void) sigaction(SIGEMT, &act, 0);
	(void) sigaction(SIGSYS, &act, 0);
#	endif
#	endif
	/*  Initialize the process flags */
	if (0 != gtcm_prsopt(argc, argv))
		exit(-1);
	/* Write down pid into log file */
	 OMI_DBG((omi_debug, "GTCM_SERVER pid : %d\n", omi_pid));
	/* Initialize history mechanism */
	if (history)
	{
		init_hist();
		act.sa_handler = (void (*)())dump_rc_hist;
		act.sa_flags = 0;
		(void) sigaction(SIGUSR2, &act, 0);
	}
	/*  Initialize the DBMS */
	licensed = TRUE;
	getjobnum();
	getzdir();
	if ((maxfds = getmaxfds()) < 0)
	{
		gtcm_rep_err("Unable to get system resource limits", errno);
		exit(errno);
	}
	assert(SIZEOF(gtcm_ast_avail) == 2);	/* check that short is size 2 bytes as following code relies on that */
	gtcm_ast_avail = (maxfds > MAXINT2) ? MAXINT2 : maxfds;
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	curr_pattern = pattern_list = &mumps_pattern;
	pattern_typemask = mumps_pattern.typemask;
	INVOKE_INIT_SECSHR_ADDRS;
	initialize_pattern_table();
	/* Preallocate some timer blocks. */
	prealloc_gt_timers();
	gt_timers_add_safe_hndlrs();
	/* Moved to omi_gvextnam, omi_lkextnam */
	/*    gvinit(); */
	return;
}

/* signal handler called when the process is about to core dump */
void gtcm_fail(int sig)
{
	void rc_rundown();
        struct sigaction def;

	FPRINTF(stderr,"GT.CM terminating on signal %d, cleaning up...\n", sig);
	/* quickie cleanup */
	rc_rundown();

	sigemptyset(&def.sa_mask);
	def.sa_flags = 0;
	def.sa_handler = SIG_DFL;
	(void) sigaction(SIGQUIT, &def, 0);
	kill(getpid(),SIGQUIT);
        exit(sig);
}
