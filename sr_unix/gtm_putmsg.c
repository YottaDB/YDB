/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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

#include "gtm_multi_thread.h"
#include "util.h"
#include "gtmmsg.h"
#include "gtm_putmsg_list.h"
#include "gtmimagename.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "anticipatory_freeze.h"
#include "gtm_multi_proc.h"
#include "interlock.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;

/*  WARNING:	For chained error messages, all messages MUST be followed by an fao count;
 *  =======	zero MUST be specified if there are no parameters.
 */

/* #GTM_THREAD_SAFE : The below function (gtm_putmsg) is thread-safe */
void gtm_putmsg(int argcnt, ...)
{
	boolean_t	was_holder;
	sgmnt_addrs	*csa;
	jnlpool_addrs_ptr_t	local_jnlpool;	/* needed by PTHREAD_CSA_FROM_GV_CUR_REGION */
	boolean_t	release_latch;
	va_list		var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	PTHREAD_CSA_FROM_GV_CUR_REGION(csa, local_jnlpool);
	VAR_START(var, argcnt);
	PTHREAD_MUTEX_LOCK_IF_NEEDED(was_holder); /* get thread lock in case threads are in use */
	GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);	/* get multi-process lock if needed */
	gtm_putmsg_list(csa, argcnt, var);
	va_end(var);
	util_out_print("",TRUE);
	REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);	/* release multi-process lock if needed */
	PTHREAD_MUTEX_UNLOCK_IF_NEEDED(was_holder);	/* release exclusive thread lock if needed */
}

/* #GTM_THREAD_SAFE : The below function (gtm_putmsg_csa) is thread-safe */
void gtm_putmsg_csa(void *csa, int argcnt, ...)
{
	boolean_t	was_holder;
	boolean_t	release_latch;
	va_list		var;

	VAR_START(var, argcnt);
	PTHREAD_MUTEX_LOCK_IF_NEEDED(was_holder); /* get thread lock in case threads are in use */
	GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);	/* get multi-process lock if needed */
	gtm_putmsg_list(csa, argcnt, var);
	va_end(var);
	util_out_print("",TRUE);
	REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);	/* release multi-process lock if needed */
	PTHREAD_MUTEX_UNLOCK_IF_NEEDED(was_holder);	/* release exclusive thread lock if needed */
}

/* #GTM_THREAD_SAFE : The below function (gtm_putmsg_noflush) is thread-safe */
void gtm_putmsg_noflush(int argcnt, ...)
{
	boolean_t	was_holder;
	sgmnt_addrs	*csa;
	jnlpool_addrs_ptr_t	local_jnlpool;	/* needed by PTHREAD_CSA_FROM_GV_CUR_REGION */
	boolean_t	release_latch;
	va_list		var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	PTHREAD_CSA_FROM_GV_CUR_REGION(csa, local_jnlpool);
	VAR_START(var, argcnt);
	PTHREAD_MUTEX_LOCK_IF_NEEDED(was_holder); /* get thread lock in case threads are in use */
	GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);	/* get multi-process lock if needed */
	gtm_putmsg_list(csa, argcnt, var);
	va_end(var);
	REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);	/* release multi-process lock if needed */
	PTHREAD_MUTEX_UNLOCK_IF_NEEDED(was_holder);	/* release exclusive thread lock if needed */
}

/* #GTM_THREAD_SAFE : The below function (gtm_putmsg_noflush_csa) is thread-safe */
void gtm_putmsg_noflush_csa(void *csa, int argcnt, ...)
{
	boolean_t	was_holder;
	boolean_t	release_latch;
	va_list		var;

	VAR_START(var, argcnt);
	PTHREAD_MUTEX_LOCK_IF_NEEDED(was_holder); /* get thread lock in case threads are in use */
	GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);	/* get multi-process lock if needed */
	gtm_putmsg_list(csa, argcnt, var);
	va_end(var);
	REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);	/* release multi-process lock if needed */
	PTHREAD_MUTEX_UNLOCK_IF_NEEDED(was_holder);	/* release exclusive thread lock if needed */
}
