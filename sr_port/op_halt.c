/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"		/* for exit() */
#include "gtm_string.h"

#include "gtmimagename.h"
#include "send_msg.h"
#include "getzposition.h"

#ifdef VMS
#  include <ssdef.h>
#endif
#include "op.h"

LITREF	gtmImageName	gtmImageNames[];

error_def(ERR_PROCTERM);

void op_halt(void)
{
#ifdef VMS
	sys$exit(SS$_NORMAL);
#else
#	ifdef GTM_TRIGGER
	mval	zposition;

	/* If HALT is done from a non-runtime trigger, send a warning message to oplog to record the fact
	 * of this uncommon process termination method.
	 */
	if (!IS_GTM_IMAGE && !IS_GTM_SVC_DAL_IMAGE)
        {
		zposition.mvtype = 0;   /* It's not an mval yet till getzposition fills it in */
		getzposition(&zposition);
		assert(MV_IS_STRING(&zposition) && (0 < zposition.str.len));
		send_msg(VARLSTCNT(9) ERR_PROCTERM, 7, GTMIMAGENAMETXT(image_type), RTS_ERROR_TEXT("HALT"),
			 0, zposition.str.len, zposition.str.addr);
	}
#	endif
	exit(EXIT_SUCCESS);
#endif
}
