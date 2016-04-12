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
#include "gvcmz.h"
#include "cmi.h"
#include "iosp.h"

GBLREF unsigned short	lkresponse_count;
GBLREF struct CLB	*lktask_x;
GBLREF uint4		lkstatus;
GBLREF unsigned char	cm_action;
GBLREF unsigned char	lkerror;
GBLREF struct CLB	*lkerrlnk;
GBLREF cm_lk_response	*lk_granted, *lk_suspended;
GBLREF unsigned short	lksusp_sent,lksusp_rec;

void gvcmz_lkread_ast(struct CLB *lnk)
{
	unsigned char	*ptr;
	uint4 		status;
	cm_lk_response	*lk_suspend, *lk_grant,*p;

	if (CMI_CLB_ERROR(lnk))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		lkerror = CMMS_L_LKREQUEST;
		lkerrlnk = lnk;
		return;
	}
	if (lkstatus == CMMS_M_LKBLOCKED)
	{
		lnk->ast = 0;
		ptr = lnk->mbf;
		*ptr++ = CMMS_L_LKSUSPEND;
		*ptr++ = cm_action;
		lnk->cbl = S_HDRSIZE + S_LAFLAGSIZE;
		status = cmi_write(lnk);
		if (CMI_ERROR(status))
		{
			((link_info *)(lnk->usr))->neterr = TRUE;
			lkerror = CMMS_L_LKREQUEST;
			lkerrlnk = lnk;
			return;
		}
		assert(0 == lksusp_sent && NULL == lk_suspended || 0 < lksusp_sent && NULL != lk_suspended);
		lksusp_sent++;
		lk_suspend = &((link_info *)(lnk->usr))->lk_response;
		lk_suspend->next = lk_suspended;
		lk_suspend->response = lnk;
		lk_suspended = lk_suspend;
		lk_suspend->response->ast = gvcmz_lksuspend_ast;
		status = cmi_read(lnk);
		if (CMI_ERROR(status))
		{
			((link_info *)(lnk->usr))->neterr = TRUE;
			lkerror = CMMS_L_LKREQUEST;
			lkerrlnk = lnk;
			return;
		}
	} else if (lkstatus != CMMS_L_LKCANCEL)
	{
		assert(lkstatus == SS_NORMAL);
		if (*lnk->mbf == CMMS_M_LKBLOCKED)
		{
			lktask_x = lnk;
			lnk->ast = 0;
			ptr = lnk->mbf;
			*ptr++ = CMMS_L_LKACQUIRE;
			*ptr++ = cm_action;
			lnk->cbl = S_HDRSIZE + S_LAFLAGSIZE;
			status = cmi_write(lnk);
			if (CMI_ERROR(status))
			{
				((link_info *)(lnk->usr))->neterr = TRUE;
				lkerror = CMMS_L_LKREQUEST;
				lkerrlnk = lnk;
				return;
			}
			lksusp_sent = 0;
			lksusp_rec = 0;
			lk_suspend = lk_granted;
			while (lk_suspend)
			{
				lk_granted = lk_granted->next;
				lk_suspend->response->ast = 0;
				ptr = lk_suspend->response->mbf;
				*ptr++ = CMMS_L_LKSUSPEND;
				*ptr++ = cm_action;
				lk_suspend->response->cbl = S_HDRSIZE + S_LAFLAGSIZE;
				status = cmi_write(lk_suspend->response);
				if (CMI_ERROR(status))
				{
					((link_info *)(lk_suspend->response->usr))->neterr = TRUE;
					lkerror = CMMS_L_LKREQUEST;
					lkerrlnk = lk_suspend->response;
					return;
				}
				lksusp_sent++;
				lk_suspend->next = lk_suspended;
				lk_suspended = lk_suspend;
				lk_suspend->response->ast = gvcmz_lksuspend_ast;
				status = cmi_read(lk_suspend->response);
				if (CMI_ERROR(status))
				{
					((link_info *)(lk_suspend->response->usr))->neterr = TRUE;
					lkerror = CMMS_L_LKREQUEST;
					lkerrlnk = lk_suspend->response;
					return;
				}
				lk_suspend = lk_granted;
			}
			lkstatus = CMMS_M_LKBLOCKED;
		} else if (*(lnk->mbf) == CMMS_M_LKABORT)
		{
			lkstatus = CMMS_L_LKCANCEL;
			lnk->ast = 0;
		} else
		{
			if (*(lnk->mbf) != CMMS_M_LKGRANTED)
			{
				lkerror = CMMS_L_LKREQUEST;
				lkerrlnk = lnk;
				return;
			}
			((link_info *)(lnk->usr))->lck_info |= (cm_action == CM_ZALLOCATES ? REMOTE_ZALLOCATES : REMOTE_LOCKS);
			lnk->ast = 0;
			lk_grant = &((link_info *)(lnk->usr))->lk_response;
			lk_grant->next = lk_granted;
			lk_grant->response = lnk;
			lk_granted = lk_grant;
		}
	}
	lkresponse_count++;
}
