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
#include "gtcm_neterr.h"
#include "gtcm_action_pending.h"

GBLREF connection_struct *curr_entry;

void gtcm_neterr(struct NTD *tsk, struct CLB *lnk, cmi_reason_t msg)
{
	connection_struct *cnx;

	if (	msg == CMI_REASON_INTMSG ||
		msg == CMI_REASON_DISCON ||
		msg == CMI_REASON_ABORT ||
		msg == CMI_REASON_EXIT ||
		msg == CMI_REASON_PATHLOST ||
		msg == CMI_REASON_PROTOCOL ||
		msg == CMI_REASON_THIRDPARTY ||
		msg == CMI_REASON_TIMEOUT ||
		msg == CMI_REASON_NETSHUT ||
		msg == CMI_REASON_REJECT ||
		msg == CMI_REASON_CONFIRM)
	{
                if (lnk && curr_entry && lnk != curr_entry->clb_ptr)
                {
			if (*lnk->mbf != CMMS_S_TERMINATE)
			{
				*lnk->mbf = CMMS_E_TERMINATE;
				lnk->cbl = 1;
				cnx = (connection_struct *) lnk->usr;
				cnx->state = 0;
/*                              if (((connection_struct *)RELQUE2PTR(cnx->qent.fl))->qent.bl + cnx->qent.fl != 0) */
                                if (!cnx->waiting_in_queue)
				{
					(void)gtcm_action_pending(cnx);
				}
			}
		}
	}
}
