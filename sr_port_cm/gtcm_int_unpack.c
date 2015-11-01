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
#include "gt_timer.h"
#include "gtcm_int_unpack.h"
#include "gtcm_action_pending.h"

GBLREF connection_struct	*curr_entry;

/*  called from gtcm_server and gtcm_write_ast
    must be call in an AST or as DCLAST for gtcm_action_pending */
void gtcm_int_unpack(connection_struct *cnx)
{
        struct CLB *tell;

        VMS_ONLY(assert(lib$ast_in_prog()));		/* remove if call with AST disabled */
	assert(FALSE == cnx->waiting_in_queue);		/* may be too strict due to lk starve */
	assert(curr_entry != cnx);
	assert(1 == (cnx->int_cancel.laflag & 1));
	tell = cnx->clb_ptr;
	assert(tell);
	assert(S_HDRSIZE + S_LAFLAGSIZE + 1 <= tell->mbl);
	assert(CM_NOLKCANCEL == cnx->last_cancelled);
	cnx->lk_cancel = cnx->last_cancelled = cnx->int_cancel.transnum;
	tell->mbf[0] = CMMS_L_LKCANCEL;
	tell->mbf[1] = (cnx->int_cancel.laflag & ~1);  /* remove valid flag */
	tell->mbf[2] = cnx->int_cancel.transnum;
	if (-1 != gtcm_action_pending(cnx))
	{ 	/* on queue */
		cnx->int_cancel.laflag = 0;
		cnx->new_msg = FALSE;
		cancel_timer((TID)cnx);    /* lock starvation timer if any */
        }
}
