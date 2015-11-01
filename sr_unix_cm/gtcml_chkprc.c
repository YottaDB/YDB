/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gtcmlkdef.h"

GBLREF	relque action_que;
GBLREF  gd_region *gv_cur_region;

void gtcml_chkprc(lck)
cm_lckblklck *lck;
{
	cm_lckblkprc *prc, *prc1;
	bool found;
	uint4 status;
	error_def(CMERR_CMINTQUE);

	found = FALSE;
	prc = lck->prc;

	while (!found)
	{
		if (prc->user->state != CMMS_L_LKACQUIRE || prc->user->transnum == prc->user->lk_cancel)
		{
			prc->next->last = prc->last;
			prc->last->next = prc->next;
			prc1 = prc->next;
			if (prc->next == prc)
				lck->prc = 0;
			else if (prc == lck->prc)
				lck->prc = prc1;
			free(prc);

			if (!lck->prc || prc1->next == lck->prc)
				break;
			prc = prc1;
			continue;
		}
		if (lck->seq != lck->node->sequence)
		{
			lck->seq = lck->node->sequence;
			found = TRUE;
		}
		else
		{
			if (prc->next == lck->prc)
				break;
			prc = prc->next;
		}
	}

	if (found)
	{
		if (((connection_struct *)RELQUE2PTR(prc->user->qent.fl))->qent.bl +
			prc->user->qent.fl != 0)
			status = insqt(prc->user,&action_que);
		if (status == INTERLOCK_FAIL)
		{	rts_error(CMERR_CMINTQUE);
		}

		prc->next->last = prc->last;
		prc->last->next = prc->next;
		if (prc->next == prc)
			lck->prc = 0;
		else if (prc == lck->prc)
			lck->prc = prc->next;
		free(prc);
	}
}
