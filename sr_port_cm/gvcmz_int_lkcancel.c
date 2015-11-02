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

#if defined(VMS)
#include <ssdef.h>
#endif

#if defined(UNIX)
#include <signal.h>
#endif

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "ast.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gvcmz.h"
#include "cmi.h"
#include "iosp.h"
#include "gt_timer.h"
#include "copy.h"

GBLDEF int		lkcancel_count;

GBLREF struct NTD	*ntd_root;
GBLREF unsigned char	cm_action;
GBLREF unsigned char	lkerror;
GBLREF struct CLB	*lkerrlnk;
GBLREF unsigned char	cmlk_num;

#define CM_LKCANCEL_WAIT_TIME	100 /* ms */

void gvcmz_int_lkcancel(void)
{
	static unsigned char	temp[16];
	bool			read_inprog;
	char			errbuf[90];
	unsigned char		*ptr, action, sent;
	unsigned short		count, cbl;
	uint4			status, norm_stat;
	int			loopcounter = 0;
	struct CLB		*p;
	CMI_MUTEX_DECL;
	DEBUG_ONLY(void		(*oldast)();)

	error_def(ERR_BADSRVRNETMSG);
	error_def(ERR_TEXT);

	if (!ntd_root)
		GTMASSERT;
	CMI_MUTEX_BLOCK;
	action = CMMS_L_LKCANCEL;
	temp[0] = CMMS_S_INTERRUPT;
	temp[3] = action;
	temp[4] = cm_action;
	temp[5] = cmlk_num;
	lkcancel_count = count = 0;
	sent = (CM_ZALLOCATES == cm_action ? ZAREQUEST_SENT : LREQUEST_SENT);
	for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl);  p != (struct CLB *)ntd_root;  p = (struct CLB *)RELQUE2PTR(p->cqe.fl))
	{
		if (((link_info*)(p->usr))->lck_info & (REQUEST_PENDING | sent))
		{
			ptr = p->mbf;
			cbl = p->cbl;
			CM_PUT_USHORT(&temp[1], ((link_info*)(p->usr))->procnum, ((link_info*)(p->usr))->convert_byteorder);
			DEBUG_ONLY(oldast = p->ast;)
			if (CM_CLB_READ == p->sta)
				read_inprog = TRUE;
			else
				read_inprog = FALSE;
			if (read_inprog)
			{
				p->mbf = &temp[CM_URGDATA_OFFSET];
				p->cbl = CM_URGDATA_LEN;
				status = cmi_write_int(p);
			} else
			{
				assert(0 == p->ast); /* below cmi_write has to be synchronous. vinu 06/18/2001 */
				p->mbf = &temp[3];
				p->cbl = 3;
				status = cmi_write(p);
			}
			p->mbf = ptr;
			p->cbl = cbl;
			if (CMI_ERROR(status))
			{
				((link_info *)(p->usr))->neterr = TRUE;
			/* safe to always enable since error ??? smw 96/11 */
				VMS_ONLY(was_setast = SS$_WASSET;) /* to force ENABLE_AST in CMI_MUTEX_RESTORE */
				CMI_MUTEX_RESTORE;
				gvcmz_error(action, status);
				return;
			}
			if (!read_inprog)
			{
				p->ast = gvcmz_lkcancel_ast;
				status = cmi_read(p);
				if (CMI_ERROR(status))
				{
					((link_info *)(p->usr))->neterr = TRUE;
				/* safe to always enable since error ??? smw 96/11 */
					VMS_ONLY(was_setast = SS$_WASSET;) /* to force ENABLE_AST in CMI_MUTEX_RESTORE */
					CMI_MUTEX_RESTORE;
					gvcmz_error(action, status);
					return;
				}
			} else
			{
				assert(CM_CLB_READ == p->sta);
				p->ast = gvcmz_lkcancel_ast;
			}
			count++;
		}
	}
	CMI_MUTEX_RESTORE;
/* 97/6/23 smw need to rethink break condition here */
	while (lkcancel_count < count && !lkerror)
	{
		CMI_IDLE(CM_LKCANCEL_WAIT_TIME);
		DEBUG_ONLY(loopcounter++;)
	}
	if (lkerror)
	{
		if (CMI_CLB_ERROR(lkerrlnk))
			gvcmz_error(lkerror, CMI_CLB_IOSTATUS(lkerrlnk));
		else
		{
			if (CMMS_E_ERROR != *(lkerrlnk->mbf))
			{
				SPRINTF(errbuf, "gvcmz_int_lkcancel: expected CMMS_E_ERROR, got %d", (int)(*(lkerrlnk->mbf)));
				rts_error(VARLSTCNT(6) ERR_BADSRVRNETMSG, 0, ERR_TEXT, 2, LEN_AND_STR(errbuf));
			} else
				gvcmz_errmsg(lkerrlnk, FALSE);
		}
	}
	return;
}
