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

/*
 * -----------------------------------------------
 * lke_clearlock : removes the qualified lock node
 * used in	 : lke_cleartree.c
 * -----------------------------------------------
 */

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "util.h"
#include "lke.h"
#include "cmi.h"
#include "gvcmz.h"
#include "lke_clearlock.h"

#define FLUSH		1

error_def(ERR_LCKGONE);


bool	lke_clearlock(
		      gd_region		*region,
		      struct CLB	*lnk,
		      mlk_ctldata_ptr_t	ctl,
		      mlk_shrblk_ptr_t	node,
		      mstr		*name,
		      bool		all,
		      bool 		interactive,
		      int4 		pid)
{
	clear_confirm	confirm;
	clear_reply	reply;
	uint4		status;
	int		len;
	bool 		unlock = FALSE;
	sgmnt_addrs	*csa;

	if (node->owner != 0  &&  (pid == node->owner  ||  pid == 0))
	{
		if (interactive)
			if (lnk == NULL)
				unlock = lke_get_answ("Clear lock ? ");
			else
			{
				lnk->mbl = sizeof confirm;
				lnk->mbf = (unsigned char *)&confirm;
				lnk->ast = NULL;
				status = cmi_read(lnk);
				if ((status & 1) == 0)
				{
					((link_info *)(lnk->usr))->neterr = TRUE;
					gvcmz_error(CMMS_U_LKEDELETE, status);
					return FALSE;
				}
				unlock = confirm.clear;
			}
		else
			unlock = TRUE;

		if (unlock)
		{
			csa = &FILE_INFO(region)->s_addrs;
			node->owner = 0;
			node->sequence = csa->hdr->trans_hist.lock_sequence++;
			len = name->len - 1;
			if (name->addr[len] != '(')
				++len;
			if (lnk == NULL)
				util_out_print("Lock removed : !AD", FLUSH, len, name->addr);
			else
				if (!interactive)
				{
					reply.code = CMMS_V_LKESHOW;
					reply.status = ERR_LCKGONE;
					reply.locknamelength = len;
					memcpy(reply.lockname, name->addr, len);
					lnk->cbl = sizeof reply - (sizeof reply.lockname - len);
					lnk->mbf = (unsigned char *)&reply;
					lnk->ast = NULL;
					status = cmi_write(lnk);
					if ((status & 1) == 0)
					{
						((link_info *)(lnk->usr))->neterr = TRUE;
						gvcmz_error(CMMS_V_LKESHOW, status);
						return FALSE;
					}
				}
		}
	}

	return unlock;
}
