/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
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
#include "gtm_stdio.h"
#ifdef VMS
#include <ssdef.h>
#endif

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
#	ifdef VMS
	exit((0 == retcode) ? EXIT_SUCCESS : EXIT_FAILURE);
#	else
#	ifdef GTM_TRIGGER
	/* If ZHALT is done from a non-runtime trigger, send a warning message to oplog to record the fact
	 * of this uncommon process termination method.
	 */
	if (!IS_GTM_IMAGE && !IS_GTM_SVC_DAL_IMAGE)
        {
		zposition.mvtype = 0;   /* It's not an mval yet till getzposition fills it in */
		getzposition(&zposition);
		assert(MV_IS_STRING(&zposition) && (0 < zposition.str.len));
		send_msg(VARLSTCNT(9) ERR_PROCTERM, 7, GTMIMAGENAMETXT(image_type), RTS_ERROR_TEXT("ZHALT"),
			 retcode, zposition.str.len, zposition.str.addr);
	}
#	endif
	if ((0 != retcode) && (0 == (retcode & 0xFF)))
		retcode = 255;;	/* If the truncated return code that can be passed back to a parent process is zero
				 * set the retcode to 255 so a non-zero return code is returned instead (UNIX only).
				 */
	exit(retcode);
#	endif
}
