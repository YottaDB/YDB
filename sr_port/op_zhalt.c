/****************************************************************
 *								*
 * Copyright (c) 2011-2015 Fidelity National Information	*
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
#include "gtm_stdio.h"

#include "gtmimagename.h"
#include "send_msg.h"
#include "getzposition.h"
#include "mvalconv.h"
#include "op.h"

LITREF	gtmImageName	gtmImageNames[];

error_def(ERR_PROCTERM);

/* Exit process with given return code */
void op_zhalt(mval *returncode)
{
	int	retcode;
	GTMTRIG_ONLY(mval zposition;)

	retcode = 0;
	assert(returncode);
	retcode = mval2i(returncode);		/* can only use integer portion */
#	ifdef GTM_TRIGGER
	/* If ZHALT is done from a non-runtime trigger, send a warning message to oplog to record the fact
	 * of this uncommon process termination method.
	 */
	if (!IS_GTM_IMAGE)
        {
		zposition.mvtype = 0;   /* It's not an mval yet till getzposition fills it in */
		getzposition(&zposition);
		assert(MV_IS_STRING(&zposition) && (0 < zposition.str.len));
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_PROCTERM, 7, GTMIMAGENAMETXT(image_type), RTS_ERROR_TEXT("ZHALT"),
			 retcode, zposition.str.len, zposition.str.addr);
	}
#	endif
	if ((0 != retcode) && (0 == (retcode & 0xFF)))
		retcode = 255;;	/* If the truncated return code that can be passed back to a parent process is zero
				 * set the retcode to 255 so a non-zero return code is returned instead (UNIX only).
				 */
	EXIT(retcode);
}
