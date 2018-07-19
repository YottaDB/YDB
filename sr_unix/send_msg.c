/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
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

#include "gtm_multi_thread.h"
#include "gtmmsg.h"
#include "error.h"
#include "fao_parm.h"
#include "util.h"
#include "util_out_print_vaparm.h"
#include "send_msg.h"
#include "caller_id.h"
#include "gtmsiginfo.h"
/* database/replication related includes due to anticipatory freeze */
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"			/* for gtmsource.h */
#include "gtmsource.h"			/* for anticipatory_freeze.h */
#include "anticipatory_freeze.h"	/* for SET_ANTICIPATORY_FREEZE_IF_NEEDED */

GBLREF	VSIG_ATOMIC_T		forced_exit;
GBLREF	boolean_t		caller_id_flag;
GBLREF	gd_region		*gv_cur_region;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	volatile boolean_t	timer_in_handler;
GBLREF	int4			exit_state;

#ifdef DEBUG
static uint4		nesting_level = 0;
#endif

void send_msg_va(void *csa, int arg_count, va_list var);

/*
**  WARNING:    For chained error messages, all messages MUST be followed by an fao count;
**  =======     zero MUST be specified if there are no parameters.
*/

/* This routine is a variation on the unix version of rts_error, and has an identical interface */

/* #GTM_THREAD_SAFE : The below function (send_msg) is thread-safe */
void send_msg(int arg_count, ...)
{
        va_list		var;
	sgmnt_addrs	*csa;
	jnlpool_addrs_ptr_t	local_jnlpool;	/* used by PTHREAD_CSA_FROM_GV_CUR_REGION */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	PTHREAD_CSA_FROM_GV_CUR_REGION(csa, local_jnlpool);
        VAR_START(var, arg_count);
	send_msg_va(csa, arg_count, var);
	va_end(var);
}

/* #GTM_THREAD_SAFE : The below function (send_msg_csa) is thread-safe */
void send_msg_csa(void *csa, int arg_count, ...)
{
	va_list		var;

        VAR_START(var, arg_count);
	send_msg_va(csa, arg_count, var);
	va_end(var);
}

/* #GTM_THREAD_SAFE : The below function (send_msg_va) is thread-safe */
void send_msg_va(void *csa, int arg_count, va_list var)
{
        int		dummy, fao_actual, fao_count, i, msg_id, freeze_msg_id;
        char    	msg_buffer[1024];
        mstr    	msg_string;
	char		*save_util_outptr;
	va_list		save_last_va_list_ptr;
	boolean_t	util_copy_saved = FALSE;
	boolean_t	freeze_needed = FALSE, was_holder;
	jnlpool_addrs_ptr_t	local_jnlpool;	/* used by CHECK_IF_FREEZE_ON_ERROR_NEEDED and FREEZE_INSTANCE_IF_NEEDED */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	PTHREAD_MUTEX_LOCK_IF_NEEDED(was_holder); /* get thread lock in case threads are in use */
	/* Since send_msg uses a global variable buffer, reentrant calls to send_msg will use the same buffer.
	 * Ensure we never overwrite an under-construction send_msg buffer with a nested send_msg call. One
	 * exception to this is if the nested call to send_msg is done by exit handling code in which case the
	 * latest send_msg call prevails and it is ok since we will never return to the original send_msg call
	 * again. The other exception is if enable interrupts in util_out_send_oper results in a new send_msg
	 * in deferred_exit_handler.
	 */
	assert((0 == nesting_level) || ((2 > nesting_level) && timer_in_handler)
		|| (EXIT_IMMED == exit_state) || (2 == forced_exit));
	DEBUG_ONLY(nesting_level++;)
        assert(arg_count > 0);
	ASSERT_SAFE_TO_UPDATE_THREAD_GBLS;
	if ((NULL != TREF(util_outptr)) && (TREF(util_outptr) != TREF(util_outbuff_ptr)))
	{
		SAVE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
	}
        util_out_print(NULL, RESET);
        for (;;)
        {
                msg_id = (int) va_arg(var, VA_ARG_TYPE);
		CHECK_IF_FREEZE_ON_ERROR_NEEDED(csa, msg_id, freeze_needed, freeze_msg_id, local_jnlpool);
                --arg_count;
                msg_string.addr = msg_buffer;
                msg_string.len = SIZEOF(msg_buffer);
                gtm_getmsg(msg_id, &msg_string);
                if (0 < arg_count)
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
		VAR_COPY(var, TREF(last_va_list_ptr));
		va_end(TREF(last_va_list_ptr));
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
	RESTORE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
        /* it has been suggested that this would be a place to check a view_debugN
         * and conditionally enter a "forever" loop on wcs_sleep for unix debugging
         */
	DEBUG_ONLY(nesting_level--;)
	FREEZE_INSTANCE_IF_NEEDED(csa, freeze_needed, freeze_msg_id, local_jnlpool);
	PTHREAD_MUTEX_UNLOCK_IF_NEEDED(was_holder);	/* release exclusive thread lock if needed */
}
