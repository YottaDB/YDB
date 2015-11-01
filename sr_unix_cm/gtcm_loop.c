/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include "gtm_unistd.h"		/* for execlp() and fork() */
#include "gtm_stdlib.h"		/* for exit() */

#include <sys/wait.h>		/* for wait() */

#include "gtcm.h"

#if defined(sun) || defined(mips)
#	include <sys/time.h>
#else
#	ifdef SEQUOIA
#		include <sys/bsd_time.h>
#	else
#		include <time.h>
#	endif
#endif

#include <errno.h>
#ifdef DEBUG
#include "gtm_stdio.h"
#include "gtm_fcntl.h"
#endif /* defined(DEBUG) */

#include "gt_timer.h"	/* for cancel_timer() and start_timer() atleast */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

GBLREF int	psock;
GBLREF int 	conn_timeout;
GBLREF int	history;
GBLREF int	omi_pid;
GBLDEF omi_conn *curr_conn;

static int detect_hang = 5;

void
gtcm_loop(cll)
    omi_conn_ll	*cll;
{
    extern int	 omi_exitp;

    int		 nfds, res;
    int		 now, rsp;
    fd_set	 r_fds, e_fds;
    omi_conn	*cptr, *prev;
    struct timeval timeout, *tp;
    void hang_handler();

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
	if ((nfds = select(++nfds, &r_fds, (fd_set *)0, (fd_set *)0,
			   tp)) < 0)
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

	now = time(0);

/*	if we are doing pinging...check the ping socket */
	if (psock >= 0 && FD_ISSET(psock, &r_fds))
		rsp = get_ping_rsp();
	else
		rsp = -1;

/*	Loop through the connections servicing transactions */
	for (cptr = cll->head, prev = (omi_conn *)0; cptr; ) {
	    if (FD_ISSET(cptr->fd, &r_fds))
	    {
		if (detect_hang)
		    start_timer(gtcm_loop, 60000, hang_handler, 0, NULL);

		curr_conn = cptr;
		res = omi_srvc_xact(cptr);
		curr_conn = NULL;

		if (detect_hang)
		    cancel_timer(gtcm_loop);

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
				     SRVR_NAME,cptr->stats.id,
				     cptr->stats.id));
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

/*	Sanity check: is nfds == 0? */

    }

    return;

}


void hang_handler()
{
	void gcore_server();
	OMI_DBG_STMP;
	OMI_DBG((omi_debug, "%s: server appears to be hung...generating core file...\n",
		 SRVR_NAME));

	gcore_server();
	detect_hang--;	   /* don't core dump when this value reaches zero */
	wait(NULL);	   /* wait for a signal or the child to exit */
}


void gcore_server()
{
	int pid;
	char *gtm_dist, *getenv();
	char path[256];

	if (history)
	{
		OMI_DBG((omi_debug, "%s:  dumping RC history\n", SRVR_NAME));
		dump_rc_hist();
	}

	gtm_dist = getenv("gtm_dist");
	if (gtm_dist)
	{
	    	char omi_pid_str[12];
		sprintf(omi_pid_str,"%d",omi_pid);
		strcpy(path,gtm_dist);
		strcat(path,"/gtcm_gcore");

		pid=fork();	/* fork error */
		if (pid < 0)
		{
			OMI_DBG((omi_debug,
				 "%s: unable to start a new process to gcore the server\n",
				 SRVR_NAME));
			perror(SRVR_NAME);
		}
		else if (!pid)  /* child */
		{
			execlp(path,path,omi_pid_str,(char *) 0);
			OMI_DBG((omi_debug, "%s: unable to generate core file\n", SRVR_NAME));
			exit(1);
		}
	}
}


