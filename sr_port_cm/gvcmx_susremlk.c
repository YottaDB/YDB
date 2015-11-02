/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "stringpool.h"
#include "gt_timer.h"
#include "gvcmx.h"
#include "gvcmz.h"
#include "cmi.h"
#include "iosp.h"

GBLREF struct NTD	*ntd_root;
GBLREF unsigned short	lksusp_sent, lksusp_rec;
GBLREF spdesc		stringpool;
GBLREF unsigned char	lkerror;
GBLREF struct CLB	*lkerrlnk;

#define CM_LKSUSPEND_TIME		100 /* ms */

void gvcmx_susremlk(unsigned char rmv_locks)
{
	uint4		status,count,buffer;
	unsigned char	*ptr;
	struct CLB	*p;

	error_def(ERR_BADSRVRNETMSG);

	if (!ntd_root)
		return;
	buffer = lksusp_sent = lksusp_rec = 0;
	for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl); p != (struct CLB *)ntd_root; p = (struct CLB *)RELQUE2PTR(p->cqe.fl))
	{
		if (((link_info*)(p->usr))->lck_info & REQUEST_PENDING)
			buffer += p->mbl;
	}
	ENSURE_STP_FREE_SPACE(buffer);
	ptr = stringpool.free;
	for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl); p != (struct CLB *)ntd_root; p = (struct CLB *)RELQUE2PTR(p->cqe.fl))
	{
		if (((link_info *)(p->usr))->lck_info & REQUEST_PENDING)
		{
			p->mbf = ptr;
			*ptr++ = CMMS_L_LKSUSPEND;
			*ptr++ = rmv_locks;
			p->cbl = 2;
			p->ast = 0;
			status = cmi_write(p);
			if (CMI_ERROR(status))
			{
				((link_info *)(p->usr))->neterr = TRUE;
				gvcmz_error(CMMS_M_LKSUSPENDED, status);
				return;
			}
			p->ast = gvcmz_lksuspend_ast;
			status = cmi_read(p);
			if (CMI_ERROR(status))
			{
				((link_info *)(p->usr))->neterr = TRUE;
				gvcmz_error(CMMS_M_LKSUSPENDED, status);
				return;
			}
			lksusp_sent++;
			ptr = p->mbf + p->mbl;
		}
	}
	while (lksusp_sent != lksusp_rec && !lkerror)
		CMI_IDLE(CM_LKSUSPEND_TIME);
	if (lkerror)
	{
		if (CMI_CLB_ERROR(lkerrlnk))
			gvcmz_error(lkerror, CMI_CLB_IOSTATUS(lkerrlnk));
		else
		{
			if (*(lkerrlnk->mbf) != CMMS_E_ERROR)
				rts_error(VARLSTCNT(1) ERR_BADSRVRNETMSG);
			else
				gvcmz_errmsg(lkerrlnk, FALSE);
		}
	}
}
