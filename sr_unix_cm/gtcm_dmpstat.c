/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  gtcm_dmpstat.c ---
 *
 *
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include "mdef.h"
#include <sys/types.h>
#include <signal.h>
#include "gtm_stdio.h"
#include "gtm_time.h"
#include "gtcm.h"
#include "eintr_wrappers.h"

int gtcm_dmpstat(int sig)
{
    extern omi_conn_ll	*omi_conns;
    extern int		 omi_pid;
    extern int4	 omi_nxact;
    extern int4	 omi_nxact2;
    extern int4	 omi_nerrs;
    extern int4	 omi_brecv;
    extern int4	 omi_bsent;
    extern int4		 gtcm_stime;  /* start time for GT.CM */
    extern int4		 gtcm_ltime;  /* last time stats were gathered */

    FILE		*fp;
    time_t		 t;
    time_t		 uptime, uphours, upmins, upsecs;
    time_t		 itime, ihours, imins, isecs;
    int			status;

    if (!(fp = Fopen(GTCM_STAT, "a")))
	return -1;

    t = time((time_t *)0);
    uptime = t - gtcm_stime;
    uphours = uptime / 3600;
    upmins = (uptime % 3600) / 60;
    upsecs = uptime % 60;

    itime = t - gtcm_ltime;
    ihours = itime / 3600;
    imins = (itime % 3600) / 60;
    isecs = itime % 60;

    FPRINTF(fp, "%s", GTM_CTIME(&t));
    OMI_DBG((omi_debug, "%s", GTM_CTIME(&t)));
    FPRINTF(fp, "%d\n", omi_pid);
    OMI_DBG((omi_debug, "%d\n", omi_pid));
    FPRINTF(fp, "Up time:  %d:%.2d:%.2d\n",uphours,upmins,upsecs);
    OMI_DBG((omi_debug, "Up time:  %d:%.2d:%.2d\n",uphours,upmins,upsecs));
    if (uptime != itime)
    {
	FPRINTF(fp, "Time since last stat dump:  %d:%.2d:%.2d\n",ihours,imins,
		isecs);
	OMI_DBG((omi_debug, "Time since last stat dump:  %d:%.2d:%.2d\n",
		 ihours,imins,isecs));
    }
    FPRINTF(fp, "Good connections: %d\n", omi_conns->stats.conn);
    OMI_DBG((omi_debug, "Good connections: %d\n", omi_conns->stats.conn));
    FPRINTF(fp, "Bad connections: 0\n");
    OMI_DBG((omi_debug, "Bad connections: 0\n"));
    FPRINTF(fp, "Good disconnects: %d\n",
	    omi_conns->stats.disc - omi_conns->stats.clos);
    OMI_DBG((omi_debug, "Good disconnects: %d\n",
	     omi_conns->stats.disc - omi_conns->stats.clos));
    FPRINTF(fp, "Bad disconnects: %d\n", omi_conns->stats.clos);
    OMI_DBG((omi_debug, "Bad disconnects: %d\n", omi_conns->stats.clos));
    FPRINTF(fp, "Number of transactions: %ld\n", omi_nxact);
    OMI_DBG((omi_debug, "Number of transactions: %ld\n", omi_nxact));
    if (uptime)
    {
	FPRINTF(fp, "Avg. transactions/sec: %ld\n", omi_nxact/uptime);
	OMI_DBG((omi_debug, "Avg. transactions/sec: %ld\n", omi_nxact/uptime));
    }
    if (gtcm_stime)
    {
	FPRINTF(fp, "transactions since last stat dump: %ld\n", omi_nxact2);
	OMI_DBG((omi_debug, "transactions since last stat dump: %ld\n",
		 omi_nxact2));
	if (itime)
	{
	    FPRINTF(fp, "Avg. transactions/sec since last stat dump: %ld\n",
		    omi_nxact2/itime);
	    OMI_DBG((omi_debug,
		     "Avg. transactions/sec since last stat dump: %ld\n",
		     omi_nxact2/itime));
	}
    }
    FPRINTF(fp, "Number of errors: %ld\n", omi_nerrs);
    OMI_DBG((omi_debug, "Number of errors: %ld\n", omi_nerrs));
    FPRINTF(fp, "Number of bytes received: %ld\n", omi_brecv);
    OMI_DBG((omi_debug, "Number of bytes received: %ld\n", omi_brecv));
    FPRINTF(fp, "Number of bytes sent: %ld\n", omi_bsent);
    OMI_DBG((omi_debug, "Number of bytes sent: %ld\n", omi_bsent));
    FPRINTF(fp, "\n");

    gtcm_ltime = t;
    omi_nxact2 = 0;

    fflush(fp);
    FCLOSE(fp, status);

    return 0;

}
