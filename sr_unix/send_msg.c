/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

#include "gtmmsg.h"

#include "error.h"
#include "fao_parm.h"
#include "util.h"
#include "util_out_print_vaparm.h"
#include "send_msg.h"
#include "caller_id.h"
#include "gtmsiginfo.h"

GBLREF bool		caller_id_flag;
GBLREF va_list		last_va_list_ptr;
GBLREF volatile int4	exit_state;

#ifdef DEBUG
static uint4		nesting_level = 0;
#endif

#define NOFLUSH 0
#define FLUSH   1
#define RESET   2
#define OPER    4



/*
**  WARNING:    For chained error messages, all messages MUST be followed by an fao count;
**  =======     zero MUST be specified if there are no parameters.
*/

/* This routine is a variation on the unix version of rts_error, and has an identical interface */

void send_msg(int arg_count, ...)
{
        va_list var;
        int   dummy, fao_actual, fao_count, i, msg_id;
        char    msg_buffer[1024];
        mstr    msg_string;

	/* Since send_msg uses a global variable buffer, reentrant calls to send_msg will use the same buffer.
	 * Ensure we never overwrite an under-construction send_msg buffer with a nested send_msg call. The
	 * only exception to this is if the nested call to send_msg is done by exit handling code in which case
	 * the latest send_msg call prevails and it is ok since we will never return to the original send_msg()
	 * call again.  Detect if ever this assmption gets violated with an assert.
	 */
	assert((0 == nesting_level) || (EXIT_IMMED == exit_state));
	DEBUG_ONLY(nesting_level++;)
        VAR_START(var, arg_count);
        assert(arg_count > 0);
	util_out_save();
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
                        fao_actual = (int) va_arg(var, VA_ARG_TYPE);
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
		va_end(var);	/* need this before used as dest in copy */
		VAR_COPY(var, last_va_list_ptr);
		va_end(last_va_list_ptr);
		arg_count -= fao_count;

                if (0 >= arg_count)
                {
                        if (caller_id_flag)
                                PRINT_CALLERID;
                        break;
                }
                util_out_print("!/", NOFLUSH);
        }
	va_end(var);

        util_out_print(NULL, OPER);
	util_out_restore();
        /* it has been suggested that this would be a place to check a view_debugN
         * and conditionally enter a "forever" loop on wcs_sleep for unix debugging
         */
	DEBUG_ONLY(nesting_level--;)
}
