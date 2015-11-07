/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  gtcm_loop.c ---
 *
 *	GTCM server forever loop.  BSD_TCP!
 *
 */

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_unistd.h"		/* for execlp and fork */
#include "gtm_stdlib.h"		/* for exit */
#include "gtm_stdio.h"		/* for SPRINTF */

#include <sys/wait.h>		/* for wait */

#include "gtcm.h"

#if defined(sun) || defined(mips)
#	include <sys/time.h>
#else
#	ifdef SEQUOIA
#		include <sys/bsd_time.h>
#	else
#		include "gtm_time.h"
#	endif
#endif

#include <errno.h>
#ifdef DEBUG
#include "gtm_fcntl.h"
#endif /* defined(DEBUG) */

#include "gt_timer.h"	/* for cancel_timer and start_timer and TID declaration at least */
#include "error.h"
#include "gtmio.h"
#include "have_crit.h"
#include "gtm_time.h"
#include "fork_init.h"

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

GBLREF int		psock;
GBLREF int 		conn_timeout;
GBLREF int		history;
GBLREF int		omi_pid;
GBLDEF omi_conn 	*curr_conn;
GBLDEF boolean_t	servtime_expired;
GBLREF int		per_conn_servtime;


void	hang_handler(void);
void	gcore_server(void);

error_def(ERR_OMISERVHANG);

void gtcm_loop(omi_conn_ll *cll)
{
    extern int	 omi_exitp;

    int		 nfds, res;
    int		 now, rsp;
    fd_set	 r_fds, e_fds;
    omi_conn	*cptr, *prev;
    struct timeval timeout, *tp;

    if (psock >= 0)
    {
	    timeout.tv_sec = PING_TIMEOUT;
	    timeout.tv_usec = 0;
	    tp = &timeout;
    }
    else
	    tp = NULL;

    curr_conn = NULL;
/*  Loop forever accepting connections and transactions */
    for (;;) {

	FD_ZERO(&r_fds);
	FD_ZERO(&e_fds);

/*	Pay attention to the network visible end point */
	if (INV_FD_P((nfds = cll->nve)))
	    break;
	FD_SET(cll->nve, &r_fds);
	FD_SET(cll->nve, &e_fds);
	if (psock >= 0)
		FD_SET(psock, &r_fds);

/*	We will service transactions from any existing connection */
	for (cptr = cll->head; cptr; cptr = cptr->next) {
	    FD_SET(cptr->fd, &r_fds);
	    FD_SET(cptr->fd, &e_fds);
	    if (cptr->fd > nfds)
		nfds = cptr->fd;
	}

	if (omi_exitp)
		break;

/*	Block in the system waiting for network events */
	nfds++;
	if ((nfds = select(nfds, &r_fds, (fd_set *)0, (fd_set *)0, tp)) < 0)
	{
	    if (errno == EINTR) {
		if (!omi_exitp)
		    continue;
	    }
	    else if (errno == EAGAIN)	/* temporary OS condition, retry */
	    {
		continue;
	    }
	    else
		gtcm_rep_err("select() call failed", errno);
	    break;
	}

	now = (int)time(0);

/*	if we are doing pinging...check the ping socket */
	if (psock >= 0 && FD_ISSET(psock, &r_fds))
		rsp = get_ping_rsp();
	else
		rsp = -1;

/*	Loop through the connections servicing transactions */
	for (cptr = cll->head, prev = (omi_conn *)0; cptr; ) {
	    if (FD_ISSET(cptr->fd, &r_fds))
	    {
	    	servtime_expired = FALSE; /* inside hang handler this is set TRUE */
		start_timer((TID)gtcm_loop, per_conn_servtime * 1000, hang_handler, 0, NULL);

		curr_conn = cptr;
		res = omi_srvc_xact(cptr);
		curr_conn = NULL;

		cancel_timer((TID)gtcm_loop);

		if (res >= 0)
		{
			cptr->timeout = now + conn_timeout;
			cptr->ping_cnt = 0;
		}
		else
		{
		    if (prev)
			prev->next = cptr->next;
		    else
			cll->head  = cptr->next;
		    if (cll->tail == cptr)
			cll->tail  = prev;
		    gtcm_cn_disc(cptr, cll);
		    cptr = prev;
		}
		nfds--;
	    }
	    else if (cptr->fd == rsp)  /* got a ping response for socket */
	    {
		    cptr->timeout = now + conn_timeout;
		    cptr->ping_cnt = 0;
	    }
	    else if (psock >= 0 && now >= cptr->timeout)
	    {
		    if (cptr->ping_cnt >= MAX_PING_CNT)
		    {
			    OMI_DBG((omi_debug, "%s: no response from connection %d, dropping...\n",
				     SRVR_NAME,cptr->stats.id));
			    if (prev)
				    prev->next = cptr->next;
			    else
				    cll->head  = cptr->next;
			    if (cll->tail == cptr)
				    cll->tail  = prev;
			    gtcm_cn_disc(cptr, cll);
			    cptr = prev;
		    }
		    else
		    {
			    if (cptr->ping_cnt)
				    OMI_DBG((omi_debug, "%s: no response from connection %d, checking...\n",
				     SRVR_NAME,cptr->stats.id));
			    else
				    OMI_DBG((omi_debug, "%s: checking connection %d.\n",
				     SRVR_NAME,cptr->stats.id));
			    icmp_ping(cptr->fd);
			    cptr->ping_cnt++;
			    cptr->timeout = now + PING_TIMEOUT;
		    }
	    }

	    if ((prev = cptr))
		cptr = cptr->next;
	}

/*	If true, accept a new connection */
	if (nfds > 0 && FD_ISSET(cll->nve, &r_fds)) {
	    if (gtcm_cn_acpt(cll,now) < 0) {
		gtcm_rep_err("Unable to accept new connection", errno);
/*		break;   eliminated:  it is acceptable to retry */
	    }
	    nfds--;
	}
	/* Sanity check: is nfds == 0? */
    }
    return;
}

void hang_handler(void)
{
	OMI_DBG_STMP;
	OMI_DBG((omi_debug, "%s: server appears to be hung...generating core file...\n",
		 SRVR_NAME));
	gtcm_rep_err("", ERR_OMISERVHANG);

	gcore_server();
	wait(NULL);	   /* wait for a signal or the child to exit */
	servtime_expired = TRUE;
}


void gcore_server(void)
{
	int pid;

	if (history)
	{
		OMI_DBG((omi_debug, "%s:  dumping RC history\n", SRVR_NAME));
		dump_rc_hist();
	}

	FORK(pid);	/* BYPASSOK: we are dumping a core, so no FORK_CLEAN needed */
	if (pid < 0)	/* fork error */
	{
		OMI_DBG((omi_debug,
			 "%s: unable to start a new process to generate the core\n",
			 SRVR_NAME));
		perror(SRVR_NAME);
	} else if (!pid)  /* child */
		DUMP_CORE;
}


