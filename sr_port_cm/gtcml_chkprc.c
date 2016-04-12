/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_signal.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "ast.h"
#include "gtcml.h"
#include "gtcm_action_pending.h"

GBLREF relque		action_que;
GBLREF gd_region	*gv_cur_region;
GBLREF struct NTD	*ntd_root;

error_def(CMERR_CMINTQUE);

void gtcml_chkprc(cm_lckblklck *lck)
{
	cm_lckblkprc	*prc, *prc1;
	boolean_t	found;
	long		status;
	CMI_MUTEX_DECL(cmi_mutex_rc);

	CMI_MUTEX_BLOCK(cmi_mutex_rc);
	found = FALSE;
	prc = lck->prc;
	/* it appears that the design assumes that prc should never be null, but we have empirical evidence that it happens.
	 * because we do not (at the moment of writing) have resources to pursue the discrepancy, we simply protect
	 * against it, and hope that we don't encounter other symptoms
	 */
	while ((FALSE == found) && (NULL != prc))
	{
		if (CMMS_L_LKACQUIRE != prc->user->state || prc->user->transnum == prc->user->lk_cancel)
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
		if (lck->sequence != lck->node->sequence)
		{
			lck->sequence = lck->node->sequence;
			found = TRUE;
		} else
		{
			if (prc->next == lck->prc)
				break;
			prc = prc->next;
		}
	}
	if (found)
	{
		status = gtcm_action_pending(prc->user);
		if (INTERLOCK_FAIL == status)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) CMERR_CMINTQUE);
		prc->next->last = prc->last;
		prc->last->next = prc->next;
		if (prc->next == prc)
			lck->prc = 0;
		else if (prc == lck->prc)
			lck->prc = prc->next;
		free(prc);
	}
	CMI_MUTEX_RESTORE(cmi_mutex_rc);
}
