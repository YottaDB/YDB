/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef _ANTICIPATORY_FREEZE_H
#define _ANTICIPATORY_FREEZE_H

#ifdef UNIX

#include "gtm_time.h"			/* needed for GET_CUR_TIME */
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "repl_msg.h"			/* needed for gtmsource.h */
#include "gtmsource.h"			/* needed for jnlpool_addrs typedef */
#include "sleep_cnt.h"			/* needed for SLEEP_INSTFREEZEWAIT macro */
#include "wait_for_disk_space.h"	/* needed by DB_LSEEKWRITE macro for prototype */
#include "gtmimagename.h"		/* needed for IS_GTM_IMAGE */

boolean_t		is_anticipatory_freeze_needed(int msg_id);
void			set_anticipatory_freeze(int msg_id);
boolean_t		init_anticipatory_freeze_errors(void);

/* Define function pointers to certain functions to avoid executables like gtmsecshr from unnecessarily
 * linking with these functions (which causes the database/replication stuff to be pulled in).
 */
typedef boolean_t	(*is_anticipatory_freeze_needed_t)(int msgid);
typedef void		(*set_anticipatory_freeze_t)(int msg_id);

GBLREF	is_anticipatory_freeze_needed_t		is_anticipatory_freeze_needed_fnptr;
GBLREF	set_anticipatory_freeze_t		set_anticipatory_freeze_fnptr;
GBLREF	boolean_t				pool_init;

error_def(ERR_MUINSTFROZEN);
error_def(ERR_MUINSTUNFROZEN);

error_def(ERR_MUNOACTION);
error_def(ERR_REPLINSTFREEZECOMMENT);
error_def(ERR_REPLINSTFROZEN);
error_def(ERR_REPLINSTUNFROZEN);

#define SET_ANTICIPATORY_FREEZE_IF_NEEDED(MSG_ID, FREEZE_SET)							\
{														\
	GBLREF	jnlpool_addrs		jnlpool;								\
	DCL_THREADGBL_ACCESS;											\
														\
	SETUP_THREADGBL_ACCESS;											\
	if (ANTICIPATORY_FREEZE_AVAILABLE && (NULL != is_anticipatory_freeze_needed_fnptr))			\
	{	/* NOT gtmsecshr */										\
		assert(NULL != set_anticipatory_freeze_fnptr);							\
		if (IS_REPL_INST_UNFROZEN && (*is_anticipatory_freeze_needed_fnptr)(MSG_ID))			\
		{												\
			(*set_anticipatory_freeze_fnptr)(MSG_ID);						\
			FREEZE_SET = TRUE;									\
		}												\
	}													\
}

#define REPORT_INSTANCE_FROZEN(FREEZE_SET)										\
{															\
	GBLREF	jnlpool_addrs		jnlpool;									\
															\
	if (FREEZE_SET)													\
	{														\
		send_msg(VARLSTCNT(3) ERR_REPLINSTFROZEN, 1, jnlpool.repl_inst_filehdr->inst_info.this_instname);	\
		send_msg(VARLSTCNT(3) ERR_REPLINSTFREEZECOMMENT, 1, jnlpool.jnlpool_ctl->freeze_comment);		\
	}														\
}

#define CLEAR_ANTICIPATORY_FREEZE(FREEZE_CLEARED)									\
{															\
	GBLREF	jnlpool_addrs		jnlpool;									\
															\
	if (IS_REPL_INST_FROZEN)											\
	{														\
		jnlpool.jnlpool_ctl->freeze = 0;									\
		FREEZE_CLEARED = TRUE;											\
	}														\
}

#define REPORT_INSTANCE_UNFROZEN(FREEZE_CLEARED)									\
{															\
	GBLREF	jnlpool_addrs		jnlpool;									\
															\
	if (FREEZE_CLEARED)												\
		send_msg(VARLSTCNT(3) ERR_REPLINSTUNFROZEN, 1, jnlpool.repl_inst_filehdr->inst_info.this_instname);	\
}

#define AFREEZE_MASK				0x01
#define ANTICIPATORY_FREEZE_AVAILABLE		(0 != (TREF(gtm_custom_errors)).len)
#define ANTICIPATORY_FREEZE_HONORED(CSA)	(DBG_ASSERT(NULL != CSA)				\
							((NULL != jnlpool.jnlpool_ctl)			\
							&& (REPL_ALLOWED(((sgmnt_addrs *)CSA)->hdr))))
#define ANTICIPATORY_FREEZE_ENABLED(CSA)	(ANTICIPATORY_FREEZE_HONORED(CSA)			\
							&& ANTICIPATORY_FREEZE_AVAILABLE		\
							&& (((sgmnt_addrs *)CSA)->hdr->freeze_on_fail))
#define IS_REPL_INST_FROZEN			((NULL != jnlpool.jnlpool_ctl) && jnlpool.jnlpool_ctl->freeze)
#define IS_REPL_INST_UNFROZEN			((NULL != jnlpool.jnlpool_ctl) && !jnlpool.jnlpool_ctl->freeze)

#define INST_FROZEN_COMMENT			"PID %d encountered %s; Instance frozen"

#define GENERATE_INST_FROZEN_COMMENT(BUF, BUF_LEN, MSG_ID)							\
{														\
	GBLREF uint4		process_id;									\
	const err_ctl		*ctl;										\
	const err_msg		*msginfo;									\
														\
	ctl = err_check(MSG_ID);										\
	assert(NULL != ctl);											\
	GET_MSG_INFO(MSG_ID, ctl, msginfo);									\
	SNPRINTF(BUF, BUF_LEN, INST_FROZEN_COMMENT, process_id, msginfo->tag);					\
}

/* This is a version of the macro which waits for the instance freeze to be lifted off assuming the process has
 * already attached to the journal pool. We need to wait for the freeze only if the input database cares about
 * anticipatory freeze. Examples of those databases that dont care are non-replicated databases, databases with
 * "freeze_on_fail" field set to FALSE in the file header etc. Hence the use of ANTICIPATORY_FREEZE_ENABLED below.
 * Note: Do not use "hiber_start" as that uses timers and if we are already in a timer handler now, nested timers
 * wont work. Since SHORT_SLEEP allows a max of 1000, we use 500 (half a second) for now.
 */
#define WAIT_FOR_REPL_INST_UNFREEZE(CSA)											\
{																\
	gd_region	*reg;													\
	char		*time_ptr, time_str[CTIME_BEFORE_NL + 2]; /* for GET_CUR_TIME macro */					\
	now_t		now;													\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	assert(NULL != CSA);													\
	if (ANTICIPATORY_FREEZE_HONORED(CSA))											\
	{															\
		reg = ((sgmnt_addrs *)CSA)->region;										\
		if (!IS_GTM_IMAGE)												\
		{														\
			GET_CUR_TIME;												\
			gtm_putmsg(VARLSTCNT(7) ERR_MUINSTFROZEN, 5, CTIME_BEFORE_NL, time_ptr,					\
					jnlpool.repl_inst_filehdr->inst_info.this_instname, DB_LEN_STR(reg));			\
		}														\
		WAIT_FOR_REPL_INST_UNFREEZE_NOCSA;										\
		if (!IS_GTM_IMAGE)												\
		{														\
			GET_CUR_TIME;												\
			gtm_putmsg(VARLSTCNT(7) ERR_MUINSTUNFROZEN, 5, CTIME_BEFORE_NL, time_ptr,				\
					jnlpool.repl_inst_filehdr->inst_info.this_instname, DB_LEN_STR(reg));			\
		}														\
	}															\
}
/* This is a safer version of the WAIT_FOR_REPL_INST_UNFREEZE macro, which waits for the instance freeze
 * to be lifted off but is not sure if the process has access to the journal pool yet.
 * If it does not, then it assumes the instance is not frozen.
 */
#define WAIT_FOR_REPL_INST_UNFREEZE_SAFE(CSA)		\
{							\
	GBLREF	jnlpool_addrs	jnlpool;		\
							\
	assert(NULL != CSA);				\
	if (IS_REPL_INST_FROZEN)			\
		WAIT_FOR_REPL_INST_UNFREEZE(CSA);	\
}

/* Below are similar macros like the above but with no CSA to specifically check for */
#define	WAIT_FOR_REPL_INST_UNFREEZE_NOCSA						\
{											\
	GBLREF	jnlpool_addrs	jnlpool;						\
	GBLREF	volatile int4	exit_state;						\
	GBLREF	int4		exi_condition;						\
	GBLREF	int4		forced_exit_err;					\
											\
	assert(NULL != jnlpool.jnlpool_ctl);						\
	/* If this region is not replicated, do not care for instance freezes */	\
	while (jnlpool.jnlpool_ctl->freeze)						\
	{										\
		if (exit_state != 0)							\
		{									\
			send_msg(VARLSTCNT(1) forced_exit_err);				\
			gtm_putmsg(VARLSTCNT(1) forced_exit_err);			\
			exit(-exi_condition);						\
		}									\
		SHORT_SLEEP(SLEEP_INSTFREEZEWAIT);					\
	}										\
}
#define WAIT_FOR_REPL_INST_UNFREEZE_NOCSA_SAFE		\
{							\
	GBLREF	jnlpool_addrs	jnlpool;		\
							\
	if (IS_REPL_INST_FROZEN)			\
		WAIT_FOR_REPL_INST_UNFREEZE_NOCSA;	\
}

/* GTM_DB_FSYNC/GTM_JNL_FSYNC are similar to GTM_FSYNC except that we dont do the fsync
 * (but instead hang) if we detect the instance is frozen. We proceed with the fsync once the freeze clears.
 * CSA is a parameter indicating which database it is that we want to fsync.
 * GTM_REPL_INST_FSYNC is different in that we currently dont care about instance freeze for replication
 * instance file writes.
 */
#define GTM_DB_FSYNC(CSA, FD, RC)						\
{										\
	GBLREF	jnlpool_addrs		jnlpool;				\
	node_local_ptr_t		cnl;					\
										\
	assert((NULL != CSA) || (NULL == jnlpool.jnlpool_ctl));			\
	if (NULL != CSA)							\
	{									\
		WAIT_FOR_REPL_INST_UNFREEZE_SAFE(CSA);				\
		cnl = (CSA)->nl;						\
		if (NULL != cnl)						\
			INCR_GVSTATS_COUNTER((CSA), cnl, n_db_fsync, 1);	\
	}									\
	GTM_FSYNC(FD, RC);							\
}

#define GTM_JNL_FSYNC(CSA, FD, RC)						\
{										\
	GBLREF	jnlpool_addrs		jnlpool;				\
	node_local_ptr_t		cnl;					\
										\
	assert((NULL != CSA) || (NULL == jnlpool.jnlpool_ctl));			\
	if (NULL != CSA)							\
	{									\
		WAIT_FOR_REPL_INST_UNFREEZE_SAFE(CSA);				\
		cnl = (CSA)->nl;						\
		if (NULL != cnl)						\
			INCR_GVSTATS_COUNTER((CSA), cnl, n_jnl_fsync, 1);	\
	}									\
	GTM_FSYNC(FD, RC);							\
}

#define GTM_REPL_INST_FSYNC(FD, RC)	GTM_FSYNC(FD, RC)

#define	DB_LSEEKWRITE(csa, db_fn, fd, new_eof, buff, size, status)							\
{															\
	int	lcl_status;												\
															\
	if (NULL != csa)												\
		WAIT_FOR_REPL_INST_UNFREEZE_SAFE(csa);									\
	LSEEKWRITE(fd, new_eof, buff, size, lcl_status);								\
	if (ENOSPC == lcl_status)											\
		wait_for_disk_space(csa, (char *)db_fn, fd, (off_t)new_eof, (char *)buff, (size_t)size, &lcl_status);	\
	status = lcl_status;												\
}

/* Currently, writes to journal files are treated the same way as database files.
 * But the macros are defined so we have the ability to easily change them in the future in case needed.
 */
#define	JNL_LSEEKWRITE		DB_LSEEKWRITE

/* Currently, writes to replication instance files do NOT trigger instance freeze behavior.
 * Neither does a pre-existing instance freeze affect replication instance file writes.
 * Hence this is defined as simple LSEEKWRITE.
 */
#define	REPL_INST_LSEEKWRITE	LSEEKWRITE

#define REPL_INST_AVAILABLE	(repl_inst_get_name((char *)replpool_id.instfilename, &full_len, SIZEOF(replpool_id.instfilename), \
							return_on_error))

#else	/* #ifdef UNIX */
#	define ANTICIPATORY_FREEZE_AVAILABLE			FALSE
#	define ANTICIPATORY_FREEZE_ENABLED(CSA)			FALSE
#	define REPL_INST_AVAILABLE				FALSE
#	define WAIT_FOR_REPL_INST_UNFREEZE
#	define WAIT_FOR_REPL_INST_UNFREEZE_SAFE
#endif	/* #ifdef UNIX */

#endif	/* #ifndef _ANTICIPATORY_FREEZE_H */
