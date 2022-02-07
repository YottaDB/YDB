/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
		      mlk_pvtctl_ptr_t	pctl,
		      struct CLB	*lnk,
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
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
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
			csa = pctl->csa;
			node->owner = 0;
			node->sequence = csa->hdr->trans_hist.lock_sequence++;
			len = name->len - 1;
			if (name->addr[len] != '(')
				++len;
			if (lnk == NULL)
			{	/* Cannot use "util_out_print" here since it has a 2K limit whereas len can be greater (YDB#845).
				 * Hence using "fprintf" to stderr directly.
				 */
				if (TREF(util_outptr) != TREF(util_outbuff_ptr))
				{	/* This means this is the first lock name being printed in
					 * this region after the "util_out_print()" call in "lke_clear.c"
					 * which set up the util_output buffers to hold the region name.
					 * So print that out and then move on to FPRINTF calls.
					 */
					util_out_print("", FLUSH);
				}
				FPRINTF(stderr, "Lock removed : %.*s\n", len, name->addr);
			} else
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
