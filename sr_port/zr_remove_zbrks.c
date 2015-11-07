/****************************************************************
 *								*
 * Copyright (c) 2002-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rtnhdr.h>
#include "zbreak.h"
#include "break.h"
#include "gtmmsg.h"

GBLREF z_records 	zbrk_recs;
GBLREF int4		break_message_mask;

GTMTRIG_ONLY(error_def(ERR_TRIGZBREAKREM);)

/* Remove all ZBREAKs in given rtn */
void zr_remove_zbrks(rhdtyp *rtn, boolean_t notify_is_trigger)
{
	zbrk_struct		*zb_ptr;
	GTMTRIG_ONLY(boolean_t	msg_done = FALSE;)

	/* This function should never be called for an older version of a recursively relinked routine. Assert that. */
	USHBIN_ONLY(assert((NULL == rtn) || (NULL == rtn->active_rhead_adr));)
	for (zb_ptr = zbrk_recs.free - 1; NULL != zbrk_recs.beg && zb_ptr >= zbrk_recs.beg; zb_ptr--)
	{	/* Go in the reverse order to reduce memory movement in zr_put_free() */
		if ((NULL == rtn) || (zb_ptr->rtnhdr == rtn))
		{
			assert((NULL == rtn) || (ADDR_IN_CODE(((unsigned char *)zb_ptr->mpc), rtn)));
#			ifdef GTM_TRIGGER
			if ((BREAKMSG == notify_is_trigger) && !msg_done && (break_message_mask & TRIGGER_ZBREAK_REMOVED_MASK))
			{	/* Message is info level */
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TRIGZBREAKREM, 2, rtn->routine_name.len,
					       rtn->routine_name.addr);
				msg_done = TRUE;
			}
#			endif
			zr_remove_zbreak(&zbrk_recs, zb_ptr);
		}
	}
	return;
}
