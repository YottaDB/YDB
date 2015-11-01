/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

/* gcc/LinuxIA32 needs stdio.h before varargs until removed from error.h */
/* gcc/Linux390 needs varargs before stdarg */
#ifdef EARLY_VARARGS
#include <varargs.h>
#endif
#ifdef __GNUC__
#include "gtm_stdio.h"
#endif
#ifndef EARLY_VARARGS
#include <varargs.h>
#endif

#include "gtmmsg.h"

#include "error.h"
#include "fao_parm.h"
#include "util.h"
#include "util_out_print_vaparm.h"
#include "send_msg.h"
#include "caller_id.h"

GBLREF bool caller_id_flag;
GBLREF va_list	last_va_list_ptr;

#define NOFLUSH 0
#define FLUSH   1
#define RESET   2
#define OPER    4



/*
**  WARNING:    For chained error messages, all messages MUST be followed by an fao count;
**  =======     zero MUST be specified if there are no parameters.
*/

/* This routine is a variation on the unix version of rts_error, and has an identical interface */

void send_msg(va_alist)
va_dcl
{
        va_list var;
        int     arg_count, dummy, fao_actual, fao_count, i, msg_id;
        char    msg_buffer[1024];
        mstr    msg_string;

        VAR_START(var);
        arg_count = va_arg(var, int);
        assert(arg_count > 0);
        util_out_print(NULL, RESET);

        for (;;)
        {
                msg_id = va_arg(var, int);
                --arg_count;

                msg_string.addr = msg_buffer;
                msg_string.len = sizeof(msg_buffer);
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

                util_out_print_vaparm(msg_string.addr, NOFLUSH, var, fao_count);
		VAR_COPY(var, last_va_list_ptr);
		arg_count -= fao_count;

                if (0 >= arg_count)
                {
                        if (caller_id_flag)
                                PRINT_CALLERID;
                        break;
                }
                util_out_print("!/", NOFLUSH);
        }

        util_out_print(NULL, OPER);
        /* it has been suggested that this would be a place to check a view_debugN
         * and conditionally enter a "forever" loop on wcs_sleep for unix debugging
         */
}
