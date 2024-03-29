/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include <stdarg.h>

#include "gtm_multi_thread.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "repl_msg.h"
#include "gtmsource.h"

#include "gtmmsg.h"

#include "error.h"
#include "fao_parm.h"
#include "util.h"
#include "util_out_print_vaparm.h"
#include "sgtm_putmsg.h"

/*
**  WARNING:    For chained error messages, all messages MUST be followed by an fao count;
**  =======     zero MUST be specified if there are no parameters.
*   This routine is a variation on the unix version of rts_error, and has an identical interface.
*/

void sgtm_putmsg(char *out_str, size_t out_str_len, ...)
{
	va_list	var;
	int	arg_count, dummy, fao_count, i, msg_id;
	char	msg_buffer[OUT_BUFF_SIZE];
	mstr	msg_string;
	size_t	util_outbufflen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, out_str_len);
	arg_count = va_arg(var, int);

	assert(arg_count > 0);
	ASSERT_SAFE_TO_UPDATE_THREAD_GBLS;
	util_out_print(NULL, RESET);

	for (;;)
	{
		msg_id = (int) va_arg(var, VA_ARG_TYPE);
		--arg_count;

		msg_string.addr = msg_buffer;
		msg_string.len = SIZEOF(msg_buffer);
		gtm_getmsg(msg_id, &msg_string);

		if (arg_count > 0)
		{
			fao_count = (int) va_arg(var, VA_ARG_TYPE);
			--arg_count;

			if (fao_count > MAX_FAO_PARMS)
			{
				assert(FALSE);
				fao_count = MAX_FAO_PARMS;
			}
		} else
			fao_count = 0;

		util_out_print_vaparm(msg_string.addr, NOFLUSH_OUT, var, fao_count);
		va_end(var);	/* need before using as dest in va_copy */
		VAR_COPY(var, TREF(last_va_list_ptr));
		va_end(TREF(last_va_list_ptr));
		arg_count -= fao_count;

		if (0 >= arg_count)
			break;

		util_out_print("!/", NOFLUSH_OUT);
	}
	va_end(var);

	util_out_print(NULL, SPRINT);
	util_outbufflen = STRLEN(TREF(util_outbuff_ptr));
	out_str_len -= 2;	// for the trailing \n and \0
	out_str_len = MIN(out_str_len, util_outbufflen);
	memcpy(out_str, TREF(util_outbuff_ptr), out_str_len);
	out_str[out_str_len] = '\n';
	out_str[out_str_len + 1] = '\0';
}
