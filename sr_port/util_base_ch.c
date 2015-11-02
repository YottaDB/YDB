/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

#include "mlkdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "error.h"
#include "gtmimagename.h"
#include "lke.h"
#include "dpgbldir.h"
#include "preemptive_db_clnup.h"
#include "util.h"

GBLREF boolean_t		need_core;
GBLREF boolean_t		created_core;
GBLREF boolean_t		dont_want_core;
GBLREF int4            		exi_condition;
GBLREF int4            		error_condition;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_MEMORY);
error_def(ERR_VMSMEMORY);
#ifdef VMS
error_def(ERR_MUNOFINISH);
error_def(ERR_DSENOFINISH);
error_def(ERR_LKENOFINISH);
#endif

CONDITION_HANDLER(util_base_ch)
{
	sgmnt_addrs	*csa;
	gd_addr		*addr_ptr;
	gd_region	*r_top, *r_save, *r_local;
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
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions; r_local < r_top; r_local++)
			{
				if (r_local->open && !r_local->was_open)
				{
					csa = &FILE_INFO(r_local)->s_addrs;
					if (!csa->persistent_freeze)
						region_freeze(r_local, FALSE, FALSE, FALSE);
				}
			}
		}
		if (IS_MUPIP_IMAGE)
			preemptive_db_clnup(SEVERITY);	/* for other utilities, preemptive_db_clnup() is called from util_ch() */
		UNIX_ONLY(
			if ((DUMPABLE) && !SUPPRESS_DUMP)
			{
				need_core = TRUE;
				gtm_fork_n_core();
			}
			/* rts_error sets error_condition, and util_base_ch is called only if
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
			{
				status = sys$getmsg(SIGNAL, &msglen, &msgbuf, 0, msginfo);
				if (status & 1)
				{
					if (0 < msginfo[1])
						exi_condition = (IS_MUPIP_IMAGE ? ERR_MUNOFINISH :
								(IS_DSE_IMAGE ? ERR_DSENOFINISH : ERR_LKENOFINISH));
				}
			}
		)
		UNSUPPORTED_PLATFORM_CHECK;
		EXIT(exi_condition);
	}
}
