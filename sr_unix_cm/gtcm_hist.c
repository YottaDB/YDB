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

/* gtcm_hist.c - routines to save a history of recently received and
 *  transmitted packets.
 */

#define GTCM_HIST_C

#include <signal.h>
#include <time.h>
#include "mdef.h"
#include "gtcm.h"

GBLREF omi_conn *curr_conn;

/* init_omi_hist - Set up the next OMI history record */
void init_hist(void)
{
	int	i;

	omi_hist = malloc(sizeof(omi_hist_rec) * HISTORY);
	rc_hist = malloc(sizeof(rc_hist_rec) * HISTORY);

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
    char	*then = ctime(&omi_hist[omi_hist_num].timestamp);
    omi_conn	*temp;

    then[24] = '\0';
    if (omi_hist_num < 0)   /* no history? */
	return;
    sprintf(msg,
	    "OMI Rq dump: %d / %s Lg size %d conn %d",
	    omi_hist[omi_hist_num].timestamp, then,
	    omi_hist[omi_hist_num].req_len, omi_hist[omi_hist_num].conn);
    gtcm_pktdmp(omi_hist[omi_hist_num].req, 0, msg);

}

void dump_rc_hist(void)
{
	int i;

	if (rc_hist_num < 0)	/* no history? */
		return;
	i = rc_hist_num;
	do
	{
		i = (i+1) % HISTORY;
		if (rc_hist[i].timestamp)
		{
			char msg[256];
			char *then = ctime(&rc_hist[i].timestamp);
			then[24] = '\0'; /* eliminate newline */
			if (rc_hist[i].toobigflag)
			{
				sprintf(msg,
				"RC Rq history: %d / %s Lg size %d conn %d",
					rc_hist[i].timestamp, then,
					rc_hist[i].req_len, rc_hist[i].conn);
				gtcm_pktdmp(rc_hist[i].req, 0, msg);
				sprintf(msg,
				"RC Aq history: %d / %s Lg size %d conn %d",
					rc_hist[i].timestamp, then,
					rc_hist[i].rsp_len, rc_hist[i].conn);
				gtcm_pktdmp(rc_hist[i].rsp, 0, msg);
			}
			else
			{
				sprintf(msg,
				"RC Rq history: %d / %s size %d conn %d",
					rc_hist[i].timestamp, then,
					rc_hist[i].req_len, rc_hist[i].conn);
				gtcm_pktdmp(rc_hist[i].req,
					    rc_hist[i].req_len,
					    msg);
				sprintf(msg,
				"RC Aq history: %d / %s size %d conn %d",
					rc_hist[i].timestamp, then,
					rc_hist[i].rsp_len, rc_hist[i].conn);
				gtcm_pktdmp(rc_hist[i].rsp,
					    rc_hist[i].rsp_len,
					    msg);
			}
		}
	}
	while (i != rc_hist_num);
}
