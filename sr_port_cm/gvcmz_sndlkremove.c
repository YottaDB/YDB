/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "mlkdef.h"
#include "gvcmz.h"
#include "cmi.h"
#include "iosp.h"

GBLDEF unsigned char	cmlk_num;

error_def(ERR_BADSRVRNETMSG);

void gvcmz_sndlkremove(struct CLB *lnk, unsigned char oper, unsigned char cancel)
{
	uint4		status,count;
	unsigned char	*ptr;
	mlk_pvtblk	*temp,*temp1;

	ASSERT_IS_LIBGNPCLIENT;
	ptr = lnk->mbf;
	*ptr++ = cancel;
	*ptr++ = oper;
	*ptr++ = cmlk_num;
	lnk->cbl = S_HDRSIZE + S_LAFLAGSIZE + 1;
	if (cancel == CMMS_L_LKDELETE)
	{
		gvcmz_lksublist(lnk);
		temp = ((link_info*)(lnk->usr))->netlocks;
		while (temp)
		{
			temp1 = temp->next;
			free(temp);
			temp = temp1;
		}
		((link_info*)(lnk->usr))->netlocks = 0;
		((link_info*)(lnk->usr))->lck_info &= ~REQUEST_PENDING;
	}
	status = cmi_write(lnk);
	if (CMI_ERROR(status))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		gvcmz_error(cancel, status);
		return;
	}
	status = cmi_read(lnk);
	if (CMI_ERROR(status))
	{
		((link_info *)(lnk->usr))->neterr = TRUE;
		gvcmz_error(cancel, status);
		return;
	}
	if (cancel == CMMS_L_LKCANCEL && (*(lnk->mbf) == CMMS_M_LKGRANTED || *(lnk->mbf) == CMMS_M_LKSUSPENDED))
	{	/* because a LKCANCEL can be sent on an interrupt, allow for a message underway */
		status = cmi_read(lnk);
		if (CMI_ERROR(status))
		{
			((link_info *)(lnk->usr))->neterr = TRUE;
			gvcmz_error(cancel, status);
			return;
		}
	}
	if (*(lnk->mbf) != CMMS_M_LKDELETED)
	{
		if (*(lnk->mbf) != CMMS_E_ERROR)
			RTS_ERROR_ABT(VARLSTCNT(1) ERR_BADSRVRNETMSG);
		gvcmz_errmsg(lnk,FALSE);
	}
}
