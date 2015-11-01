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
 *  gtcm_dmpstat.c ---
 *
 *
 *
 */

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#include <sys/types.h>
#include <signal.h>
#include "gtm_stdio.h"
#include <time.h>
#include <mdef.h>
#include "gtcm.h"


int
gtcm_dmpstat(sig)
    int			 sig;
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

    if (!(fp = fopen(GTCM_STAT, "a")))
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

    fprintf(fp, "%s", ctime(&t));
    OMI_DBG((omi_debug, "%s", ctime(&t)));
    fprintf(fp, "%d\n", omi_pid);
    OMI_DBG((omi_debug, "%d\n", omi_pid));
    fprintf(fp, "Up time:  %d:%.2d:%.2d\n",uphours,upmins,upsecs);
    OMI_DBG((omi_debug, "Up time:  %d:%.2d:%.2d\n",uphours,upmins,upsecs));
    if (uptime != itime)
    {
	fprintf(fp, "Time since last stat dump:  %d:%.2d:%.2d\n",ihours,imins,
		isecs);
	OMI_DBG((omi_debug, "Time since last stat dump:  %d:%.2d:%.2d\n",
		 ihours,imins,isecs));
    }
    fprintf(fp, "Good connections: %d\n", omi_conns->stats.conn);
    OMI_DBG((omi_debug, "Good connections: %d\n", omi_conns->stats.conn));
    fprintf(fp, "Bad connections: 0\n");
    OMI_DBG((omi_debug, "Bad connections: 0\n"));
    fprintf(fp, "Good disconnects: %d\n",
	    omi_conns->stats.disc - omi_conns->stats.clos);
    OMI_DBG((omi_debug, "Good disconnects: %d\n",
	     omi_conns->stats.disc - omi_conns->stats.clos));
    fprintf(fp, "Bad disconnects: %d\n", omi_conns->stats.clos);
    OMI_DBG((omi_debug, "Bad disconnects: %d\n", omi_conns->stats.clos));
    fprintf(fp, "Number of transactions: %ld\n", omi_nxact);
    OMI_DBG((omi_debug, "Number of transactions: %ld\n", omi_nxact));
    if (uptime)
    {
	fprintf(fp, "Avg. transactions/sec: %ld\n", omi_nxact/uptime);
	OMI_DBG((omi_debug, "Avg. transactions/sec: %ld\n", omi_nxact/uptime));
    }
    if (gtcm_stime)
    {
	fprintf(fp, "transactions since last stat dump: %ld\n", omi_nxact2);
	OMI_DBG((omi_debug, "transactions since last stat dump: %ld\n",
		 omi_nxact2));
	if (itime)
	{
	    fprintf(fp, "Avg. transactions/sec since last stat dump: %ld\n",
		    omi_nxact2/itime);
	    OMI_DBG((omi_debug,
		     "Avg. transactions/sec since last stat dump: %ld\n",
		     omi_nxact2/itime));
	}
    }
    fprintf(fp, "Number of errors: %ld\n", omi_nerrs);
    OMI_DBG((omi_debug, "Number of errors: %ld\n", omi_nerrs));
    fprintf(fp, "Number of bytes received: %ld\n", omi_brecv);
    OMI_DBG((omi_debug, "Number of bytes received: %ld\n", omi_brecv));
    fprintf(fp, "Number of bytes sent: %ld\n", omi_bsent);
    OMI_DBG((omi_debug, "Number of bytes sent: %ld\n", omi_bsent));
    fprintf(fp, "\n");

    gtcm_ltime = t;
    omi_nxact2 = 0;

    fflush(fp);
    fclose(fp);

    return 0;

}
