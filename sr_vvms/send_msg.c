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

#include "gtm_stdio.h"
#include <stdarg.h>

#include "error.h"
#include "fao_parm.h"
#include "util.h"
#include "gtmmsg.h"

GBLREF bool caller_id_flag;

#define NOFLUSH 0
#define FLUSH   1
#define RESET   2
#define OPER    4



/*
**  WARNING:    For chained error messages, all messages MUST be followed by an fao count;
**  =======     zero MUST be specified if there are no parameters.
*/

/* This routine is a variation on the unix version of rts_error, and has an identical interface */

void send_msg(int msg_id_arg, ...)
{
        va_list		var;
        int		arg_count, dummy, fao_actual, fao_count, fao_list[MAX_FAO_PARMS + 1], i, msg_id;
        char		msg_buffer[1024];
        mstr		msg_string;
	char		*save_util_outptr;
	va_list		save_last_va_list_ptr;
	boolean_t	util_copy_saved = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
        VAR_START(var, msg_id_arg);
        va_count(arg_count);
        assert(arg_count > 0);
	msg_id = msg_id_arg;
	if ((NULL != TREF(util_outptr)) && (TREF(util_outptr) != TREF(util_outbuff_ptr)))
	{
		SAVE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
	}
        util_out_print(NULL, RESET);

        for (;;)
        {
                --arg_count;

                msg_string.addr = msg_buffer;
                msg_string.len = SIZEOF(msg_buffer);
                gtm_getmsg(msg_id, &msg_string);

                if (arg_count > 0)
                {
                        fao_actual = va_arg(var, int);
                        --arg_count;

                        fao_count = fao_actual;
                        if (fao_count > MAX_FAO_PARMS)
			{
				assert(FALSE);
				fao_count = MAX_FAO_PARMS;
			}
                } else
                        fao_actual = fao_count = 0;

                memset(fao_list, 0, SIZEOF(fao_list));

                for (i = 0;  i < fao_count;  ++i)
                {
                        fao_list[i] = va_arg(var, int);
                        --arg_count;
                }

		/* Currently there are a max of 20 fao parms (MAX_FAO_PARMS) allowed, hence passing upto fao_list[19].
		 * An assert is added to ensure this code is changed whenever the macro MAX_FAO_PARMS is changed.
		 * The # of arguments passed below should change accordingly.
		 */
		assert(MAX_FAO_PARMS == 20);
		util_out_print(msg_string.addr, NOFLUSH, fao_list[0], fao_list[1], fao_list[2], fao_list[3], fao_list[4],
			fao_list[5], fao_list[6], fao_list[7], fao_list[8], fao_list[9], fao_list[10], fao_list[11],
			fao_list[12], fao_list[13], fao_list[14], fao_list[15], fao_list[16], fao_list[17],
			fao_list[18], fao_list[19]);

                if (arg_count < 1)
                {
                        if (caller_id_flag)
                                PRINT_CALLERID;
                        break;
                } else
                	msg_id = va_arg(var, int);
                util_out_print("!/", NOFLUSH);
        }
	va_end(var);

        util_out_print(NULL, OPER);
	RESTORE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
        /* it has been suggested that this would be a place to check a view_debugN
         * and conditionally enter a "forever" loop on wcs_sleep for unix debugging
         */
}
