/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "gtmio.h"
#include "have_crit.h"
#ifdef __MVS__
#include "gtm_stat.h"
#include "gtm_zos_io.h"
#endif

int gtcm_dmpstat(int sig)
{
    extern omi_conn_ll	*omi_conns;
    extern int		omi_pid;
    extern int4		omi_nxact;
    extern int4		omi_nxact2;
    extern int4		omi_nerrs;
    extern int4		omi_brecv;
    extern int4		omi_bsent;
    extern int4		gtcm_stime;  /* start time for GT.CM */
    extern int4		gtcm_ltime;  /* last time stats were gathered */

    FILE		*fp;
    time_t		t;
    time_t		uptime, uphours, upmins, upsecs;
    time_t		itime, ihours, imins, isecs;
    int			status;
    char		*tmp_time;

#ifdef __MVS__
    int tag_status;
    tag_status = gtm_zos_create_tagged_file(GTCM_STAT, TAG_EBCDIC);
#endif
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

    GTM_CTIME(tmp_time, &t);
    FPRINTF(fp, "%s", tmp_time);
    OMI_DBG((omi_debug, "%s", tmp_time));
    FPRINTF(fp, "%d\n", omi_pid);
    OMI_DBG((omi_debug, "%d\n", omi_pid));
    FPRINTF(fp, "Up time:  %ld:%.2ld:%.2ld\n",uphours,upmins,upsecs);
    OMI_DBG((omi_debug, "Up time:  %ld:%.2ld:%.2ld\n",uphours,upmins,upsecs));
    if (uptime != itime)
    {
	FPRINTF(fp, "Time since last stat dump:  %ld:%.2ld:%.2ld\n",ihours,imins,
		isecs);
	OMI_DBG((omi_debug, "Time since last stat dump:  %ld:%.2ld:%.2ld\n",
		 ihours,imins,isecs));
    }
    ZOS_ONLY(FPRINTF(fp, "Log file tag status : %s\n", (-1 == tag_status)?"failed":"success");)
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
    NON_IA64_ONLY(FPRINTF(fp, "Number of transactions: %ld\n", omi_nxact));
    IA64_ONLY(FPRINTF(fp, "Number of transactions: %d\n", omi_nxact));
    NON_IA64_ONLY(OMI_DBG((omi_debug, "Number of transactions: %ld\n", omi_nxact)));
    IA64_ONLY(OMI_DBG((omi_debug, "Number of transactions: %d\n", omi_nxact)));
    if (uptime)
    {
	FPRINTF(fp, "Avg. transactions/sec: %ld\n", omi_nxact/uptime);
	OMI_DBG((omi_debug, "Avg. transactions/sec: %ld\n", omi_nxact/uptime));
    }
    if (gtcm_stime)
    {
	NON_IA64_ONLY(FPRINTF(fp, "transactions since last stat dump: %ld\n", omi_nxact2));
	IA64_ONLY(FPRINTF(fp, "transactions since last stat dump: %d\n", omi_nxact2));
	NON_IA64_ONLY(OMI_DBG((omi_debug, "transactions since last stat dump: %ld\n",
		 omi_nxact2)));
	IA64_ONLY(OMI_DBG((omi_debug, "transactions since last stat dump: %d\n",
		 omi_nxact2)));
	if (itime)
	{
	    FPRINTF(fp, "Avg. transactions/sec since last stat dump: %ld\n",
		    omi_nxact2/itime);
	    OMI_DBG((omi_debug,
		     "Avg. transactions/sec since last stat dump: %ld\n",
		     omi_nxact2/itime));
	}
    }
    NON_IA64_ONLY(FPRINTF(fp, "Number of errors: %ld\n", omi_nerrs));
    IA64_ONLY(FPRINTF(fp, "Number of errors: %d\n", omi_nerrs));
    NON_IA64_ONLY(OMI_DBG((omi_debug, "Number of errors: %ld\n", omi_nerrs)));
    IA64_ONLY(OMI_DBG((omi_debug, "Number of errors: %d\n", omi_nerrs)));
    NON_IA64_ONLY(FPRINTF(fp, "Number of bytes received: %ld\n", omi_brecv));
    IA64_ONLY(FPRINTF(fp, "Number of bytes received: %d\n", omi_brecv));
    NON_IA64_ONLY(OMI_DBG((omi_debug, "Number of bytes received: %ld\n", omi_brecv)));
    IA64_ONLY(OMI_DBG((omi_debug, "Number of bytes received: %d\n", omi_brecv)));
    NON_IA64_ONLY(FPRINTF(fp, "Number of bytes sent: %ld\n", omi_bsent));
    IA64_ONLY(FPRINTF(fp, "Number of bytes sent: %d\n", omi_bsent));
    NON_IA64_ONLY(OMI_DBG((omi_debug, "Number of bytes sent: %ld\n", omi_bsent)));
    IA64_ONLY(OMI_DBG((omi_debug, "Number of bytes sent: %d\n", omi_bsent)));
    FPRINTF(fp, "\n");

    gtcm_ltime = (int4)t;
    omi_nxact2 = 0;

    FFLUSH(fp);
    FCLOSE(fp, status);

    return 0;

}
