/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_stdio.h"

#include "fao_parm.h"
#include "error.h"
#include "util.h"
#include "util_out_print_vaparm.h"
#include "gtmmsg.h"
#include "gtm_putmsg_list.h"
#include "gtmimagename.h"		/* needed for IS_GTM_IMAGE macro */
#include "gtmio.h"			/* needed for FFLUSH macro */
#include "io.h"
/* database/replication related includes due to anticipatory freeze */
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "anticipatory_freeze.h"	/* for SET_ANTICIPATORY_FREEZE_IF_NEEDED */

/*
 * ----------------------------------------------------------------------------------------
 *  WARNING:	For chained error messages, all messages MUST be followed by an fao count;
 *  =======	zero MUST be specified if there are no parameters.
 * ----------------------------------------------------------------------------------------
 */

void gtm_putmsg_list(void *csa, int arg_count, va_list var)
{
	int		i, msg_id, fao_actual, fao_count, dummy, freeze_msg_id;
	char		msg_buffer[1024];
	mstr		msg_string;
	const err_msg	*msg;
	const err_ctl	*ctl;
	boolean_t	freeze_needed = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!IS_GTMSECSHR_IMAGE)
	{	/* Note gtmsecshr does no stdout/stderr IO - everything goes to operator log so this doesn't apply */
		util_out_print(NULL, RESET);
	}
	assert(0 < arg_count);
	for (; ; )
	{
		msg_id = va_arg(var, int);
		CHECK_IF_FREEZE_ON_ERROR_NEEDED(csa, msg_id, freeze_needed, freeze_msg_id);
		--arg_count;
		if (NULL == (ctl = err_check(msg_id)))
			msg = NULL;
		else
			GET_MSG_INFO(msg_id, ctl, msg);
		msg_string.addr = msg_buffer;
		msg_string.len = sizeof msg_buffer;
		gtm_getmsg(msg_id, &msg_string);
		if (NULL == msg)
		{
			util_out_print(msg_string.addr, NOFLUSH, msg_id);
			if (0 < arg_count)
			{
				/* --------------------------
				 * Print the message to date
				 * --------------------------
				 */
				util_out_print(NULL, FLUSH);
				/* ---------------------------------------
				 * Chained error;  scan off the fao count
				 * (it should be zero)
				 * ---------------------------------------
				 */
				i = va_arg(var, int);
				--arg_count;
				assert(0 == i);
			}
		} else
		{
			if (0 < arg_count)
			{
				fao_actual = va_arg(var, int);
				--arg_count;

				fao_count = fao_actual < msg->parm_count ? fao_actual : msg->parm_count;
				if (MAX_FAO_PARMS < fao_count)
					fao_count = MAX_FAO_PARMS;
			} else
				fao_actual = fao_count = 0;
			util_out_print_vaparm(msg_string.addr, NOFLUSH, var, fao_count);
			va_end(var);	/* needed before used as dest in copy */
			VAR_COPY(var, TREF(last_va_list_ptr));			/* How much we unwound */
			arg_count -= fao_count;
			/* ------------------------------
			 * Skim off any extra parameters
			 * ------------------------------
			 */
			for (i = fao_count;  i < fao_actual;  ++i)
			{
				dummy = va_arg(var, int);
				--arg_count;
			}
			va_end(TREF(last_va_list_ptr));
		}
		if (0 == arg_count)
			break;
		if (!IS_GTMSECSHR_IMAGE)
			util_out_print("!/", NOFLUSH);
	}
	FREEZE_INSTANCE_IF_NEEDED(csa, freeze_needed, freeze_msg_id);
}
