/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "copy.h"
#include "iosp.h"
#include "gt_timer.h"
#include "gvcmz.h"

#define CM_ZDEF_WAIT_TIME	10

GBLDEF struct CLB	*zdeferr;
GBLDEF int4		zdef_sent, zdef_rcv;

GBLREF struct NTD	*ntd_root;
GBLREF bool		zdefactive;
GBLREF int4		outofband;

void gvcmz_zflush(void)
{
	int4		status;
	struct CLB	*p;
	link_info	*usr;
	unsigned short	sav_mbl;
	error_def(ERR_BADSRVRNETMSG);

	if (!zdefactive)
		return;
	zdefactive = FALSE;			/* turn off ZDEFER */
	if (!ntd_root || !ntd_root->cqh.fl)	/* if no open CM connections, no work */
		return;
	zdeferr = NULL;
	zdef_sent = zdef_rcv = 0;
	for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl); p != (struct CLB *)ntd_root; p = (struct CLB *)RELQUE2PTR(p->cqe.fl))
	{
		usr = (link_info *)(p->usr);
		if (usr->buffered_count)
		{
			assert(CMMS_B_BUFFLUSH == *usr->buffer);
			sav_mbl = p->mbl;
			p->ast = gvcmz_zdefw_ast;
			p->mbf = usr->buffer;
			CM_PUT_USHORT(p->mbf + 1, usr->buffered_count, usr->convert_byteorder);
			p->cbl = usr->buffer_used;
			usr->buffered_count = 0;
			usr->buffer_used = 0;
			status = cmi_write(p);
			p->mbl = sav_mbl;
			if (CMI_ERROR(status))
			{
				usr->neterr = TRUE;
				zdeferr = p;
				CMI_CLB_IOSTATUS(zdeferr) = status;
			} else
				zdef_sent++;
		}
	}
	while(zdef_sent != zdef_rcv)
	{
		if (outofband)
			break;
		CMI_IDLE(CM_ZDEF_WAIT_TIME);
	}
	if (zdeferr)
	{
		if (CMI_CLB_ERROR(zdeferr))
			gvcmz_error(CMMS_Q_PUT, CMI_CLB_IOSTATUS(zdeferr));
		else
		{
			if (CMMS_E_ERROR != *(zdeferr->mbf))
				rts_error(VARLSTCNT(1) ERR_BADSRVRNETMSG);
			else
				gvcmz_errmsg(zdeferr, FALSE);
		}
	}
}
