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

#include "gtm_stdlib.h"		/* for EXIT() */
#include "gtm_string.h"

#include "gtmimagename.h"
#include "send_msg.h"
#include "getzposition.h"

#include "op.h"

LITREF	gtmImageName	gtmImageNames[];

error_def(ERR_PROCTERM);

void op_halt(void)
{
#	ifdef GTM_TRIGGER
	mval	zposition;

	/* If HALT is done from a non-runtime trigger, send a warning message to oplog to record the fact
	 * of this uncommon process termination method.
	 */
	if (!IS_GTM_IMAGE)
        {
		zposition.mvtype = 0;   /* It's not an mval yet till getzposition fills it in */
		getzposition(&zposition);
		assert(MV_IS_STRING(&zposition) && (0 < zposition.str.len));
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_PROCTERM, 7, GTMIMAGENAMETXT(image_type), RTS_ERROR_TEXT("HALT"),
			 0, zposition.str.len, zposition.str.addr);
	}
#	endif
	EXIT(EXIT_SUCCESS);
}
