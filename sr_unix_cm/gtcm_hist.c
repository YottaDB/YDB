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

/* gtcm_hist.c - routines to save a history of recently received and
 *  transmitted packets.
 */

#define GTCM_HIST_C

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"		/* for SPRINTF() atleast */
#include "gtm_time.h"
#include "have_crit.h"
#include <signal.h>

#include "gtcm.h"

GBLREF omi_conn *curr_conn;

/* init_omi_hist - Set up the next OMI history record */
void init_hist(void)
{
	int	i;

	omi_hist = malloc(SIZEOF(omi_hist_rec) * HISTORY);
	rc_hist = malloc(SIZEOF(rc_hist_rec) * HISTORY);

	for(i = 0; i < HISTORY; i++)
	{
		omi_hist[i].timestamp  = rc_hist[i].timestamp  = 0;
		omi_hist[i].toobigflag = rc_hist[i].toobigflag = 0;
		omi_hist[i].req_len    = rc_hist[i].req_len    = 0;
		omi_hist[i].rsp_len    = rc_hist[i].rsp_len    = 0;
	}
	omi_hist_num = rc_hist_num = -1;
}

void init_omi_hist(int conn)
{
	omi_hist_num = (omi_hist_num + 1) % HISTORY;

	omi_hist[omi_hist_num].timestamp = time(0);
	omi_hist[omi_hist_num].conn = conn;
	omi_hist[omi_hist_num].req_len = omi_hist[omi_hist_num].rsp_len = 0;
	omi_hist[omi_hist_num].toobigflag = 0;
}

void save_omi_req(char *buff, int len)
{
	omi_hist[omi_hist_num].req_len = len;
	if (len >= 0 && len <= OMI_HIST_BUFSIZ)
		memcpy(omi_hist[omi_hist_num].req, buff, len);
	else
	{
		omi_hist[omi_hist_num].toobigflag = 1;
		memcpy(omi_hist[omi_hist_num].req, buff, OMI_HIST_BUFSIZ);
	}
}

void save_omi_rsp(char *buff, int len)
{
	omi_hist[omi_hist_num].rsp_len = len;
	if (len >= 0 && len <= OMI_HIST_BUFSIZ)
		memcpy(omi_hist[omi_hist_num].rsp, buff, len);
	else
	{
		omi_hist[omi_hist_num].toobigflag = 1;
		memcpy(omi_hist[omi_hist_num].rsp, buff, OMI_HIST_BUFSIZ);
	}
}

/* init_rc_hist - Set up the next RC history record */
void init_rc_hist(int conn)
{
	rc_hist_num = (rc_hist_num + 1) % HISTORY;
	rc_hist[rc_hist_num].timestamp = time(0);
	rc_hist[rc_hist_num].conn = conn;
	rc_hist[rc_hist_num].req_len = rc_hist[rc_hist_num].rsp_len = 0;
	rc_hist[rc_hist_num].toobigflag = 0;
}

void save_rc_req(char *buff, int len)
{
	rc_hist[rc_hist_num].req_len = len;
	if (len >= 0 && len <= RC_HIST_BUFSIZ)
		memcpy(rc_hist[rc_hist_num].req, buff, len);
	else
	{
		rc_hist[rc_hist_num].toobigflag = 1;
		memcpy(rc_hist[rc_hist_num].req, buff, RC_HIST_BUFSIZ);
	}
}

void save_rc_rsp(char *buff, int len)
{
	rc_hist[rc_hist_num].rsp_len = len;
	if (len >= 0 && len <= RC_HIST_BUFSIZ)
		memcpy(rc_hist[rc_hist_num].rsp, buff, len);
	else
	{
		rc_hist[rc_hist_num].toobigflag = 1;
		memcpy(rc_hist[rc_hist_num].rsp, buff, RC_HIST_BUFSIZ);
	}
}

void dump_omi_rq(void)
{
    char	msg[256];
    char	*then;
    omi_conn	*temp;

    GTM_CTIME(then, &omi_hist[omi_hist_num].timestamp);
    then[24] = '\0';
    if (omi_hist_num < 0)   /* no history? */
	return;
    SPRINTF(msg,
	    "OMI Rq dump: %ld / %s Lg size %d conn %d",
	    omi_hist[omi_hist_num].timestamp, then,
	    omi_hist[omi_hist_num].req_len, omi_hist[omi_hist_num].conn);
    gtcm_pktdmp(omi_hist[omi_hist_num].req, 0, msg);

}

void dump_rc_hist(void)
{
	int	i;
	char	msg[256];
	char	*then;

	if (rc_hist_num < 0)	/* no history? */
		return;
	i = rc_hist_num;
	do
	{
		i = (i + 1) % HISTORY;
		if (rc_hist[i].timestamp)
		{
			GTM_CTIME(then, &rc_hist[i].timestamp);
			then[24] = '\0'; /* eliminate newline */
			if (rc_hist[i].toobigflag)
			{
				SPRINTF(msg,
				"RC Rq history: %ld / %s Lg size %d conn %d",
					rc_hist[i].timestamp, then,
					rc_hist[i].req_len, rc_hist[i].conn);
				gtcm_pktdmp(rc_hist[i].req, 0, msg);
				SPRINTF(msg,
				"RC Aq history: %ld / %s Lg size %d conn %d",
					rc_hist[i].timestamp, then,
					rc_hist[i].rsp_len, rc_hist[i].conn);
				gtcm_pktdmp(rc_hist[i].rsp, 0, msg);
			}
			else
			{
				SPRINTF(msg,
				"RC Rq history: %ld / %s size %d conn %d",
					rc_hist[i].timestamp, then,
					rc_hist[i].req_len, rc_hist[i].conn);
				gtcm_pktdmp(rc_hist[i].req,
					    rc_hist[i].req_len,
					    msg);
				SPRINTF(msg,
				"RC Aq history: %ld / %s size %d conn %d",
					rc_hist[i].timestamp, then,
					rc_hist[i].rsp_len, rc_hist[i].conn);
				gtcm_pktdmp(rc_hist[i].rsp,
					    rc_hist[i].rsp_len,
					    msg);
			}
		}
	} while (i != rc_hist_num);
}
