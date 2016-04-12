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

#include <errno.h>
#include "mdef.h"
#include "cmidef.h"
#include "caller_id.h"
#include "iosp.h"

error_def(CMI_NETFAIL);

cmi_status_t cmj_postevent(struct CLB *lnk)
{
	struct NTD *tsk = lnk->ntd;
	cmi_reason_t reason = lnk->deferred_reason;
	cmi_status_t status = lnk->deferred_status;

	lnk->deferred_event = FALSE;
	switch (reason)
	{
	case CMI_REASON_IODONE:
		if (lnk->ast)
		{
			CMI_DPRINT(("CALLING AST FROM CMJ_POSTEVENT after IODONE, called from 0x%x\n", caller_id()));
			(*lnk->ast)(lnk);
		}
		break;
	case CMI_REASON_INTMSG:
		if (tsk->trc)
			(*tsk->trc)(lnk, CM_CLB_READ_URG, &lnk->urgdata, (size_t)1);
		if (tsk->urg)
			(*tsk->urg)(lnk, lnk->urgdata);
		break;
	case CMI_REASON_CONNECT:
		if (tsk->crq)
			(*tsk->crq)(lnk);
		break;
	default:
		switch (status)
		{
		case ECONNABORTED:
			reason = CMI_REASON_ABORT;
			break;
		case ETIMEDOUT:
			reason = CMI_REASON_TIMEOUT;
			break;
		case ECONNREFUSED:
			reason = CMI_REASON_REJECT;
			break;
		default:
			reason = CMI_REASON_DISCON;
			break;
		}
		if (lnk->ast)
		{
			CMI_DPRINT(("CALLING AST FROM CMJ_POSTEVENT after ERROR, called from 0x%x\n", caller_id()));
			(*lnk->ast)(lnk);
		}
		if (tsk->err)
			(*tsk->err)(tsk, lnk, reason);
		return reason;
	}
	return SS_NORMAL;
}
