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
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "cmi.h"
#include "iosp.h"
#include "gvcmx.h"
#include "gvcmy_remlkmgr.h"
#include "gvcmz.h"

GBLREF struct NTD	*ntd_root;
GBLREF unsigned short	lkresponse_count;
GBLREF struct CLB	*lktask_x;
GBLREF uint4		lkstatus;
GBLREF unsigned char	lkerror;
GBLREF unsigned char	cmlk_num;
GBLREF spdesc		stringpool;

bool gvcmx_reqremlk(unsigned char laflag, int4 time)
{
	unsigned char	*c_ptr, action, sent;
	unsigned short	count;
	uint4		status, buffer;
	struct CLB	*clb_ptr;

	if (!ntd_root)
		return FALSE;
	if (time == 0)
		action = CMMS_L_LKREQIMMED;
	else
		action = CMMS_L_LKREQUEST;
	lkerror = FALSE;
	lkstatus = SS_NORMAL;
	buffer = lkresponse_count = count = 0;
	lktask_x = 0;
	cmlk_num++;
	sent = (laflag == CM_ZALLOCATES ? ZAREQUEST_SENT : LREQUEST_SENT);
	for (clb_ptr = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl);  clb_ptr != (struct CLB *)ntd_root;
		clb_ptr = (struct CLB *)RELQUE2PTR(clb_ptr->cqe.fl))
	{
		if (((link_info *)(clb_ptr->usr))->lck_info & REQUEST_PENDING)
			buffer += clb_ptr->mbl;
	}
	ENSURE_STP_FREE_SPACE(buffer);
	c_ptr = stringpool.free;
	for (clb_ptr = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl);  clb_ptr != (struct CLB *)ntd_root;
		clb_ptr = (struct CLB *)RELQUE2PTR(clb_ptr->cqe.fl))
	{
		if (((link_info *)(clb_ptr->usr))->lck_info & REQUEST_PENDING)
		{
			clb_ptr->mbf = c_ptr;
			clb_ptr->ast = 0;
			c_ptr = clb_ptr->mbf;
			*c_ptr++ = action;
			*c_ptr++ = laflag;
			*c_ptr++ = cmlk_num;
			gvcmz_lksublist(clb_ptr);
			status = cmi_write(clb_ptr);
			if (CMI_ERROR(status))
			{
				((link_info *)(clb_ptr->usr))->neterr = TRUE;
				gvcmz_error(action, status);
				return FALSE;
			}
			((link_info *)(clb_ptr->usr))->lck_info |= sent;
			clb_ptr->ast = gvcmz_lkread_ast;
			status = cmi_read(clb_ptr);
			if (CMI_ERROR(status))
			{
				((link_info *)(clb_ptr->usr))->neterr = TRUE;
				gvcmz_error(action, status);
				return FALSE;
			}
			count++;
			c_ptr = clb_ptr->mbf + clb_ptr->mbl;
		}
	}
	return gvcmy_remlkmgr(count);
}
