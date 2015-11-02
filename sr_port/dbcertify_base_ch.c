/****************************************************************
 *								*
 *	Copyright 2005, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"
#ifdef VMS
#include <descrip.h>
#endif

#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "gdsblkops.h"
#include "error.h"
#include "gtmimagename.h"
#include "filestruct.h"
#include "v15_filestruct.h"
#include "dbcertify.h"

GBLREF boolean_t		need_core;
GBLREF boolean_t		created_core;
GBLREF boolean_t		dont_want_core;
GBLREF int4            		exi_condition;
GBLREF int4            		error_condition;
GBLREF enum gtmImageTypes	image_type;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_VMSMEMORY);
VMS_ONLY(error_def(ERR_DBCNOFINISH);)

CONDITION_HANDLER(dbcertify_base_ch)
{
	VMS_ONLY(
		unsigned short	msglen;
		uint4		status;
		unsigned char	msginfo[4];
		unsigned char	msg_buff[MAX_MSG_SIZE + 1];
		$DESCRIPTOR(msgbuf, msg_buff);
	)

	START_CH;
	PRN_ERROR;
	if (SUCCESS == SEVERITY || INFO == SEVERITY)
	{
		CONTINUE;
	} else
	{
		UNIX_ONLY(
			if ((DUMPABLE) && !SUPPRESS_DUMP)
			{
				need_core = TRUE;
				gtm_fork_n_core();
			}
			/* rts_error sets error_condition, and dbcertify_base_ch is called only if
			 * exiting thru rts_error. Setup exi_condition to reflect error
			 * exit status. Note, if the last eight bits (the only relevant bits
			 * for Unix exit status) of error_condition is non-zero in case of
			 * errors, we make sure that an error exit status (non-zero value -1)
			 * is setup. This is a hack.
			 */
			if (0 == exi_condition)
				exi_condition = (((error_condition & UNIX_EXIT_STATUS_MASK) != 0) ? error_condition : -1);
		)
		VMS_ONLY(
			if ((DUMPABLE) && !SUPPRESS_DUMP)
			{
				gtm_dump();
				TERMINATE;
			}
			exi_condition = SIGNAL;
			/* following is a hack to avoid FAO directives getting printed without expanding
			 * in the error message during EXIT()
			 */
			if (IS_GTM_ERROR(SIGNAL))
			        exi_condition = ERR_DBCNOFINISH;
		)
		UNSUPPORTED_PLATFORM_CHECK;
		EXIT(exi_condition);
	}
}
