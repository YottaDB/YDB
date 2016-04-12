/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <gtm_stdio.h>

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "cmi.h"
#include "gt_timer.h"
#include "gvcmy_remlkmgr.h"
#include "gvcmz.h"
#include "lock_cmtimeout.h"
#include "iosp.h"

GBLREF volatile int4	outofband;
GBLREF unsigned char	cm_action;
GBLDEF unsigned short	lkresponse_count;
GBLDEF struct CLB	*lktask_x;
GBLDEF uint4		lkstatus;
GBLDEF unsigned char	lkerror;
GBLDEF struct CLB	*lkerrlnk;
GBLREF cm_lk_response	*lk_granted, *lk_suspended;
GBLREF unsigned short	lksusp_sent, lksusp_rec;
GBLREF bool		out_of_time;
GBLDEF bool		lk_cm_noresponse;

#define CM_LKWAIT_TIME		100 /* ms */
#define CM_LKNORESPONSE_TIME	60 * 1000 /* ms */

bool gvcmy_remlkmgr(unsigned short count)
{
	char		errbuf[90];
	unsigned char	*c_ptr;
	int4		*delay;
	uint4		status, norm_stat;
	boolean_t	one_try, noresponse_timer;
	cm_lk_response	*lk_res_ptr, *lk_resume;

	error_def(ERR_BADSRVRNETMSG);
	error_def(ERR_TEXT);

	one_try = TRUE;
	lk_cm_noresponse = FALSE;
	noresponse_timer = FALSE;
	while (CMMS_M_LKGRANTED != lkstatus)
	{
		while (lkresponse_count < count && !lkerror)
		{
			if (!one_try && (outofband || out_of_time))
			{
				lkstatus = CMMS_L_LKCANCEL;
				break;
			}
			if (one_try && out_of_time && !noresponse_timer)
			{
				start_timer((TID)lock_cmtimeout, CM_LKNORESPONSE_TIME, lock_cmtimeout, 0, NULL);
				noresponse_timer = TRUE;
			}
			if (one_try && lk_cm_noresponse)
			{
				lkstatus = CMMS_L_LKCANCEL;
				break;
			}
			CMI_IDLE(CM_LKWAIT_TIME);
		}
		if (noresponse_timer && (FALSE == lk_cm_noresponse))
			cancel_timer((TID)lock_cmtimeout);
		lk_cm_noresponse = FALSE;	/* reset flags */
		noresponse_timer = FALSE;
		if (lkerror)
		{
			if (CMI_CLB_ERROR(lkerrlnk))
				gvcmz_error(lkerror, CMI_CLB_IOSTATUS(lkerrlnk));
			else
			{
				if (CMMS_E_ERROR != *(lkerrlnk->mbf))
				{
					SPRINTF(errbuf, "gvcmy_remlkmgr 1: expected CMMS_E_ERROR, got %d", (int)(*(lkerrlnk->mbf)));
					rts_error(VARLSTCNT(6) ERR_BADSRVRNETMSG, 0, ERR_TEXT, 2, LEN_AND_STR(errbuf));
				} else
					gvcmz_errmsg(lkerrlnk, FALSE);
			}
		}
		one_try = FALSE;
		if (SS_NORMAL == lkstatus)
			lkstatus = CMMS_M_LKGRANTED;
		else if (CMMS_L_LKCANCEL == lkstatus)
		{
			gvcmz_int_lkcancel();
			return FALSE;
		} else
		{
			assert(lktask_x);
			lktask_x->ast = gvcmz_lkacquire_ast;
			status = cmi_read(lktask_x);
			if (CMI_ERROR(status))
			{
				((link_info *)(lktask_x->usr))->neterr = TRUE;
				gvcmz_error(CMMS_L_LKACQUIRE, status);
				return FALSE;
			}
			while ((lksusp_sent != lksusp_rec) || lktask_x->ast)
			{
				if (lkerror || ((lksusp_sent == lksusp_rec) && (CMMS_L_LKCANCEL == lkstatus)))
					break;
				if (outofband || out_of_time)
				{
					lkstatus = CMMS_L_LKCANCEL;
					break;
				}
				CMI_IDLE(CM_LKWAIT_TIME);
			}
			if (lkerror)
			{
				if (CMI_CLB_ERROR(lkerrlnk))
					gvcmz_error(lkerror, CMI_CLB_IOSTATUS(lkerrlnk));
				else
				{	if (CMMS_E_ERROR != *(lkerrlnk->mbf))
					{
						SPRINTF(errbuf, "gvcmy_remlkmgr 2: expected CMMS_E_ERROR, got %d",
							(int)(*(lkerrlnk->mbf)));
						rts_error(VARLSTCNT(6) ERR_BADSRVRNETMSG, 0, ERR_TEXT, 2, LEN_AND_STR(errbuf));
					} else
						gvcmz_errmsg(lkerrlnk, FALSE);
				}
			}
			if (CMMS_L_LKCANCEL == lkstatus)
			{
				gvcmz_int_lkcancel();
				return FALSE;
			}
			if (CMMS_M_LKGRANTED != *lktask_x->mbf)
			{
				if (CMMS_E_ERROR != *(lktask_x->mbf))
				{
					SPRINTF(errbuf, "gvcmy_remlkmgr 3: expected CMMS_E_ERROR, got %d", (int)(*(lkerrlnk->mbf)));
					rts_error(VARLSTCNT(6) ERR_BADSRVRNETMSG, 0, ERR_TEXT, 2, LEN_AND_STR(errbuf));
				}
				gvcmz_errmsg(lktask_x, FALSE);
			}
			((link_info *)(lktask_x->usr))->lck_info |=
				(CM_ZALLOCATES == cm_action) ? REMOTE_ZALLOCATES : REMOTE_LOCKS;
			lk_res_ptr = &((link_info *)(lktask_x->usr))->lk_response;
			lk_res_ptr->response = lktask_x;
			lk_res_ptr->next = lk_granted;
			lk_granted = lk_res_ptr;
			lktask_x = 0;
			lkstatus = SS_NORMAL;
			lk_resume = lk_suspended;
			while (lk_resume)
			{
				lk_suspended = lk_suspended->next;
				lk_resume->response->ast = 0;
				c_ptr = lk_resume->response->mbf;
				*c_ptr++ = CMMS_L_LKRESUME;
				*c_ptr++ = cm_action;
				lk_resume->response->cbl = S_HDRSIZE + S_LAFLAGSIZE;
				status = cmi_write(lk_resume->response);
				if (CMI_ERROR(status))
				{
					((link_info *)(lk_resume->response->usr))->neterr = TRUE;
					gvcmz_error(CMMS_L_LKRESUME, status);
					return FALSE;
				}
				((link_info *)(lk_resume->response->usr))->lck_info -=
					(CM_ZALLOCATES == cm_action) ? REMOTE_ZALLOCATES : REMOTE_LOCKS;
				lk_resume->response->ast = gvcmz_lkread_ast;
				status = cmi_read(lk_resume->response);
				if (CMI_ERROR(status))
				{
					((link_info *)(lk_resume->response->usr))->neterr = TRUE;
					gvcmz_error(CMMS_L_LKRESUME, status);
					return FALSE;
				}
				lk_resume->next = NULL;
				lk_resume = lk_suspended;
			}
			if (lkerror)
			{
				if (CMI_CLB_ERROR(lkerrlnk))
					gvcmz_error(lkerror, CMI_CLB_IOSTATUS(lkerrlnk));
				else
				{
					if (CMMS_E_ERROR != *(lkerrlnk->mbf))
					{
						SPRINTF(errbuf, "gvcmy_remlkmgr 4: expected CMMS_E_ERROR, got %d",
							(int)(*(lkerrlnk->mbf)));
						rts_error(VARLSTCNT(6) ERR_BADSRVRNETMSG, 0, ERR_TEXT, 2, LEN_AND_STR(errbuf));
					} else
						gvcmz_errmsg(lkerrlnk,FALSE);
				}
			}
			lkresponse_count = 1;	/* smw 97/7/7 what is this */
		}
	}
	return TRUE;
}
