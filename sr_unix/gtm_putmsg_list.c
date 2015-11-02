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

#include <stdarg.h>
#include "gtm_stdio.h"

#include "fao_parm.h"
#include "error.h"
#include "util.h"
#include "util_out_print_vaparm.h"
#include "gtmmsg.h"
#include "gtm_putmsg_list.h"
#include "gtmimagename.h"	/* needed for IS_GTM_IMAGE macro */
#include "gtmio.h"		/* needed for FFLUSH macro */
#include "io.h"

GBLREF	boolean_t	donot_fflush_NULL;
GBLREF	va_list		last_va_list_ptr;

#define	NOFLUSH	0
#define FLUSH	1
#define RESET	2

/*
 * ----------------------------------------------------------------------------------------
 *  WARNING:	For chained error messages, all messages MUST be followed by an fao count;
 *  =======	zero MUST be specified if there are no parameters.
 * ----------------------------------------------------------------------------------------
 */

void gtm_putmsg_list(int arg_count, va_list var)
{
	int		i, msg_id, fao_actual, fao_count, dummy;
	char		msg_buffer[1024];
	mstr		msg_string;
	boolean_t	first_error;
	const err_msg	*msg;
	const err_ctl	*ctl;

	/* Before starting to write to stderr, make sure all other buffered streams are flushed.
	 * This way we avoid out-of-order logging issues with multiple streams mapping to the same file
	 * e.g. stdout/stderr could both end up in the same file. We do this now only for the utilities
	 * (and not mumps) since the implications of that change (is it safe or not) are not yet clear.
	 */
	if (!IS_GTM_IMAGE && !donot_fflush_NULL)
		FFLUSH(NULL);
	assert(0 < arg_count);
	util_out_print(NULL, RESET);
	first_error = TRUE;
	flush_pio();

	for (;;)
	{
		msg_id = va_arg(var, int);
		--arg_count;

		if (NULL == (ctl = err_check(msg_id)))
			msg = NULL;
		else
		{
			assert(0 != (msg_id & FACMASK(ctl->facnum))  &&  MSGMASK(msg_id, ctl->facnum) <= ctl->msg_cnt);

			msg = ctl->fst_msg + MSGMASK(msg_id, ctl->facnum) - 1;
		}

		if (first_error)
		{
			first_error = FALSE;
			error_condition = msg_id;
			severity = NULL == msg ? ERROR : SEVMASK(msg_id);
		}

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
		}
		else
		{
			if (0 < arg_count)
			{
				fao_actual = va_arg(var, int);
				--arg_count;

				fao_count = fao_actual < msg->parm_count ? fao_actual : msg->parm_count;
				if (MAX_FAO_PARMS < fao_count)
					fao_count = MAX_FAO_PARMS;
			}
			else
			{
				fao_actual = fao_count = 0;
			}

			util_out_print_vaparm(msg_string.addr, NOFLUSH, var, fao_count);
			va_end(var);	/* needed before used as dest in copy */
			VAR_COPY(var, last_va_list_ptr);			/* How much we unwound */
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
			va_end(last_va_list_ptr);
		}

		if (0 == arg_count)
			break;

		util_out_print("!/", NOFLUSH);
	}
}
