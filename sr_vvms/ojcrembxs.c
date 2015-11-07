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

#include "mdef.h"
#include <descrip.h>
#include <lnmdef.h>
#include <ssdef.h>
#include "vmsdtype.h"
#include <accdef.h>
#include "job.h"
#include "gt_timer.h"
#include "outofband.h"
#include "dfntmpmbx.h"

GBLREF	bool	ojtimeout;
GBLREF	short	ojpchan;
GBLREF	short	ojcchan;
GBLREF  int4	outofband;

static readonly mstr lnm$group = {9,  "LNM$GROUP"};
static readonly mstr lnm$process = {11, "LNM$PROCESS"};

bool ojcrembxs(uint4 *punit, struct dsc$descriptor_s *cmbx, int4 cmaxmsg, bool timed)
{
	int4		status;
	$DESCRIPTOR	(plognam, "GTM$JOB_PMBX");
	$DESCRIPTOR	(clognam, "GTM$JOB_CMBX");
	$DESCRIPTOR	(lnmtab, "LNM$TEMPORARY_MAILBOX");
	char		pmbxnam[MAX_MBXNAM_LEN];
	short		pmbxnamsz;
	struct
	{
		item_list_3	le[1];
		int4		terminator;
	}		item_list;
	uint4	ojmba_to_unit ();


	status = dfntmpmbx (lnm$process.len, lnm$process.addr);
	if (!(status & 1))
	{
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) status);
	}
	do {
		status = sys$crembx (0, &ojpchan, ACC$K_TERMLEN, ACC$K_TERMLEN,
					0, 0, &plognam);
		if (outofband)
		{	ojcleanup();
			outofband_action(FALSE);
		}
		switch (status)
		{
		case SS$_NORMAL:
			break;
		case SS$_NOIOCHAN:
			hiber_start (1000);
			if (timed && ojtimeout)
			{
				status = dfntmpmbx (lnm$group.len, lnm$group.addr);
				if (!(status & 1))
				{
					ojerrcleanup ();
					rts_error(VARLSTCNT(1) status);
				}
				return FALSE;
			}
			break;
		default:
			dfntmpmbx (lnm$group.len, lnm$group.addr);
			ojerrcleanup ();
			rts_error(VARLSTCNT(1) status);
			break;
		}
	} while (status != SS$_NORMAL);
	item_list.le[0].buffer_length		= MAX_MBXNAM_LEN;
	item_list.le[0].item_code		= LNM$_STRING;
	item_list.le[0].buffer_address		= &pmbxnam[0];
	item_list.le[0].return_length_address	= &pmbxnamsz;
	item_list.terminator			= 0;
	status = sys$trnlnm (0, &lnmtab, &plognam, 0, &item_list);
	if (!(status & 1))
	{
		dfntmpmbx (lnm$group.len, lnm$group.addr);
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) status);
	}
	status = sys$dellnm (&lnmtab, &plognam, 0);
	if (!(status & 1))
	{
		dfntmpmbx (lnm$group.len, lnm$group.addr);
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) status);
	}
	*punit = ojmba_to_unit (&pmbxnam[0]);

	do {
		status = sys$crembx (0, &ojcchan, cmaxmsg, cmaxmsg,
					0, 0, &clognam);
		if (outofband)
		{	ojcleanup();
			outofband_action(FALSE);
		}
		switch (status)
		{
		case SS$_NORMAL:
			break;
		case SS$_NOIOCHAN:
			hiber_start (1000);
			if (timed && ojtimeout)
			{
				status = dfntmpmbx (lnm$group.len, lnm$group.addr);
				if (!(status & 1))
				{
					ojerrcleanup ();
					rts_error(VARLSTCNT(1) status);
				}
				return FALSE;
			}
			break;
		default:
			dfntmpmbx (lnm$group.len, lnm$group.addr);
			ojerrcleanup ();
			rts_error(VARLSTCNT(1) status);
			break;
		}
	} while (status != SS$_NORMAL);
	item_list.le[0].buffer_address		= cmbx->dsc$a_pointer;
	item_list.le[0].return_length_address	= &(cmbx->dsc$w_length);
	status = sys$trnlnm (0, &lnmtab, &clognam, 0, &item_list);
	if (!(status & 1))
	{
		dfntmpmbx (lnm$group.len, lnm$group.addr);
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) status);
	}
	status = sys$dellnm (&lnmtab, &clognam, 0);
	if (!(status & 1))
	{
		dfntmpmbx (lnm$group.len, lnm$group.addr);
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) status);
	}

	status = dfntmpmbx (lnm$group.len, lnm$group.addr);
	if (!(status & 1))
	{
		ojerrcleanup ();
		rts_error(VARLSTCNT(1) status);
	}
	return TRUE;
}
