/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

#include "gtm_multi_thread.h"
#include "fao_parm.h"
#include "error.h"
#include "util.h"
#include "util_format.h"
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
#include "restrict.h"			/* For GTM-7759 logging control */

#define	COLON_SEPARATOR		" : "

/*
 * ----------------------------------------------------------------------------------------
 *  WARNING:	For chained error messages, all messages MUST be followed by an fao count;
 *  =======	zero MUST be specified if there are no parameters.
 * ----------------------------------------------------------------------------------------
 */

/* #GTM_THREAD_SAFE : The below function (gtm_putmsg_list) is thread-safe because caller ensures serialization with locks */
void gtm_putmsg_list(void *csa, int arg_count, va_list var)
{
	GBLREF      boolean_t               ydb_dist_ok_to_use;

	int		i, msg_id, fao_actual, fao_count, freeze_msg_id;
	char		msg_buffer[1024];
	mstr		msg_string;
	const err_msg	*msg;
	const err_ctl	*ctl;
	boolean_t	freeze_needed = FALSE;
	char		*rname;
	jnlpool_addrs_ptr_t	local_jnlpool;	/* used by CHECK_IF_FREEZE_ON_ERROR_NEEDED and FREEZE_INSTANCE_IF_NEEDED */
	boolean_t	mustlog;
	va_list var_sav;
	char fmt_buf[2048];
	char *fmt_ptr;
	int f_actual, f_count;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!IS_GTMSECSHR_IMAGE)
	{	/* Note gtmsecshr does no stdout/stderr IO - everything goes to operator log so this doesn't apply */
		util_out_print(NULL, RESET);
	}
	assert(0 < arg_count);
	assert(IS_PTHREAD_LOCKED_AND_HOLDER);
#	ifdef GTM_PTHREAD
	if (INSIDE_THREADED_CODE(rname))	/* Note: "rname" is not initialized if macro returns FALSE */
	{	/* If running with threads, identify which thread is generating this message with a prefix using
		 * thread-specific-key (which points to the region name for mur* related threads). This helps decipher
		 * the output of a mupip journal command where the region specific output might be intermixed.
		 */
		assert((NULL != rname) && ('\0' != rname[0]));
		util_out_print(rname, NOFLUSH_OUT);
		util_out_print(COLON_SEPARATOR, NOFLUSH_OUT);
	}
#	endif
	for (; ; )
	{
		msg_id = va_arg(var, int);
	 	mustlog = ( ydb_dist_ok_to_use && (!(RESTRICTED(logdenials))) &&
			(MSGFLAG(msg_id) & MSGMUSTLOG) ); /* GTM-7759 logging unless restricted */
		CHECK_IF_FREEZE_ON_ERROR_NEEDED(csa, msg_id, freeze_needed, freeze_msg_id, local_jnlpool);
		--arg_count;
		if (NULL == (ctl = err_check(msg_id)))
			msg = NULL;
		else
			GET_MSG_INFO(msg_id, ctl, msg);
		msg_string.addr = msg_buffer;
		msg_string.len = sizeof msg_buffer;
		gtm_getmsg(msg_id, &msg_string);
		if (mustlog)
		{				/* If this message must be sysloged for GTM-7759, do it here */
			va_copy(var_sav, var);
			fmt_ptr = &fmt_buf[0];
			if ((0 < arg_count) && msg)
			{				/* We don't consume arg_count here */
				f_actual = va_arg(var_sav, int);
				f_count = f_actual < msg->parm_count ? f_actual : msg->parm_count;
				if (MAX_FAO_PARMS < f_count)
					f_count = MAX_FAO_PARMS;
			} else
				f_actual = f_count = 0;
			fmt_ptr = util_format(msg_string.addr, var_sav, fmt_ptr, SIZEOF(fmt_buf) - 1, f_count);
			*fmt_ptr = '\0';
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(fmt_buf));
			va_end(var_sav);
		}
		if (NULL == msg)
		{
			util_out_print(msg_string.addr, NOFLUSH_OUT, msg_id);
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
			util_out_print_vaparm(msg_string.addr, NOFLUSH_OUT, var, fao_count);
			va_end(var);	/* needed before used as dest in copy */
			VAR_COPY(var, TREF(last_va_list_ptr));			/* How much we unwound */
			arg_count -= fao_count;
			/* ------------------------------
			 * Skim off any extra parameters
			 * ------------------------------
			 */
			for (i = fao_count;  i < fao_actual;  ++i)
			{
				va_arg(var, int);
				--arg_count;
			}
			va_end(TREF(last_va_list_ptr));
		}
		if (0 == arg_count)
			break;
		if (!IS_GTMSECSHR_IMAGE)
			util_out_print("!/", NOFLUSH_OUT);
	}
	FREEZE_INSTANCE_IF_NEEDED(csa, freeze_needed, freeze_msg_id, local_jnlpool);
}
