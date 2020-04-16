/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_syslog.h"
#include "gtm_stdio.h"
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
#include "get_syslog_flags.h"
#include "libyottadb_int.h"
#include "have_crit.h"

GBLREF	VSIG_ATOMIC_T		forced_exit;
GBLREF	boolean_t		caller_id_flag;
GBLREF	gd_region		*gv_cur_region;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	volatile boolean_t	timer_in_handler;
GBLREF	int4			exit_state;
GBLREF	boolean_t		first_syslog;

#ifdef DEBUG
static	uint4		nesting_level = 0;
GBLREF	boolean_t	in_fake_enospc;	/* used by an assert in "send_msg.c" */
#endif

/* Skip frame for send_msg/send_msg_csa */
#define PRINT_CALLERID util_out_print(" -- generated from 0x!XJ.", NOFLUSH_OUT, caller_id(1))

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
        int		dummy, fao_count, i, msg_id, freeze_msg_id;
        char    	msg_buffer[1024];
        mstr    	msg_string;
	char		*save_util_outptr;
	va_list		save_last_va_list_ptr;
	boolean_t	util_copy_saved = FALSE;
	boolean_t	freeze_needed = FALSE, was_holder;
	jnlpool_addrs_ptr_t	local_jnlpool;	/* used by CHECK_IF_FREEZE_ON_ERROR_NEEDED and FREEZE_INSTANCE_IF_NEEDED */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (in_os_signal_handler)
	{	/* We are inside an OS signal handler and so should not make any "syslog()" calls as it has the potential
		 * of causing a hang (YDB#464). Hence return right away without sending the message to the syslog
		 * even if it means loss of potentially crucial information. We don't expect this code path to get exercised
		 * unless
		 *   a) This process was sent 3 consecutive SIGTERM/SIGINT etc. causing it to go to the exit handler right away OR
		 *   b) A debug-only timer handler "fake_enospc()" invokes this.
		 * Assert that too.
		 */
		assert((EXIT_IMMED == exit_state) || in_fake_enospc);
		return;
	}
	/* Like in rts_error_csa() do some checking we aren't nesting too deep due to error loop */
	if (MAX_RTS_ERROR_DEPTH < ++(TREF(rts_error_depth)))
	{	/* Too many errors nesting - stop it here - fatally. What we do is the following:
		 *   1. Put a message in syslog (not using send_msg_csa()) about what happened.
		 *   2. Put same message on stderr (straight shot - not through YottaDBs gtm_fprintf() routine)
		 *   3. Terminate this process with a core - not a fork as this process cannot continue.
		 *
		 * Note this counter is not an exact science. Normally when we drive condition handlers, we don't return here
		 * as some sort of error mitigation code runs and resets things. So the counter is typically ever increasing
		 * but there is a reset in mdb_condition_handler and ydb_simpleapi_ch - both handlers that "recover" and continue
		 * that clear this depth counter. So we are only detecting error loops in rts_error.c itself really (they typically
		 * occur in the PTHREAD_MUTEX_LOCK_IF_NEEDED() macro)) when/if they occur.
		 */
		if (first_syslog)
		{
			OPENLOG("YottaDB", get_syslog_flags(), LOG_USER);
			first_syslog = FALSE;
		}
		SYSLOG(LOG_USER | LOG_INFO, "%s", "%YDB-F-MAXRTSERRDEPTH Error loop detected - aborting image with core");
		fprintf(stderr, "%%YDB-F-MAXRTSERRDEPTH Error loop detected - aborting image with core");
		/* It is possible "ydb_dmp_tracetbl" gets a recursive error. Therefore have a safeguard to skip that step. */
		if (MAX_RTS_ERROR_DEPTH < (2 *(TREF(rts_error_depth))))
			ydb_dmp_tracetbl();
		DUMP_CORE;			/* Terminate *THIS* thread/process and produce a core */
	}
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
                util_out_print("!/", NOFLUSH_OUT);
        }
        util_out_print(NULL, OPER);
	RESTORE_UTIL_OUT_BUFFER(save_util_outptr, save_last_va_list_ptr, util_copy_saved);
        /* it has been suggested that this would be a place to check a view_debugN
         * and conditionally enter a "forever" loop on wcs_sleep for unix debugging
         */
	DEBUG_ONLY(nesting_level--);
	FREEZE_INSTANCE_IF_NEEDED(csa, freeze_needed, freeze_msg_id, local_jnlpool);
	PTHREAD_MUTEX_UNLOCK_IF_NEEDED(was_holder);	/* release exclusive thread lock if needed */
	--(TREF(rts_error_depth));			/* All done, remove our bump */
}
