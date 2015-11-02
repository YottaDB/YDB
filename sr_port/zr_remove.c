/****************************************************************
 *								*
 *	Copyright 2002, 2011 Fidelity Information Services, Inc	*
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

/* remove all breaks in rtn */
void zr_remove(rhdtyp *rtn, boolean_t notify_is_trigger)
{
	zbrk_struct		*zb_ptr;
	GTMTRIG_ONLY(boolean_t	msg_done = FALSE;)

	for (zb_ptr = zbrk_recs.free - 1; NULL != zbrk_recs.beg && zb_ptr >= zbrk_recs.beg; zb_ptr--)
	{	/* Go in the reverse order to reduce memory movement in zr_put_free() */
		if ((NULL == rtn) || (ADDR_IN_CODE(((unsigned char *)zb_ptr->mpc), rtn)))
		{
#			ifdef GTM_TRIGGER
			if ((BREAKMSG == notify_is_trigger) && !msg_done && (break_message_mask & TRIGGER_ZBREAK_REMOVED_MASK))
			{	/* Message is info level */
				gtm_putmsg(VARLSTCNT(4) ERR_TRIGZBREAKREM, 2, rtn->routine_name.len, rtn->routine_name.addr);
				msg_done = TRUE;
			}
#			endif
			zr_put_free(&zbrk_recs, zb_ptr);
		}
	}
	return;
}
