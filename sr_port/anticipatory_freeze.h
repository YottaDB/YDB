/****************************************************************
 *								*
 *	Copyright 2012, 2013 Fidelity Information Services, Inc	*
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

boolean_t		is_anticipatory_freeze_needed(sgmnt_addrs *csa, int msg_id);
void			set_anticipatory_freeze(sgmnt_addrs *csa, int msg_id);
boolean_t		init_anticipatory_freeze_errors(void);

/* Define function pointers to certain functions to avoid executables like gtmsecshr from unnecessarily
 * linking with these functions (which causes the database/replication stuff to be pulled in).
 */
typedef boolean_t	(*is_anticipatory_freeze_needed_t)(sgmnt_addrs *csa, int msgid);
typedef void		(*set_anticipatory_freeze_t)(sgmnt_addrs *csa, int msg_id);

GBLREF	is_anticipatory_freeze_needed_t		is_anticipatory_freeze_needed_fnptr;
GBLREF	set_anticipatory_freeze_t		set_anticipatory_freeze_fnptr;
GBLREF	boolean_t				pool_init;
GBLREF	boolean_t				mupip_jnl_recover;
#ifdef DEBUG
GBLREF	uint4	  				lseekwrite_target;
#endif

error_def(ERR_MUINSTFROZEN);
error_def(ERR_MUINSTUNFROZEN);

error_def(ERR_MUNOACTION);
error_def(ERR_REPLINSTFREEZECOMMENT);
error_def(ERR_REPLINSTFROZEN);
error_def(ERR_REPLINSTUNFROZEN);
error_def(ERR_TEXT);


#define ENABLE_FREEZE_ON_ERROR											\
{														\
	if (ANTICIPATORY_FREEZE_AVAILABLE)									\
	{	/* Set anticipatory freeze function pointers to be used later (in send_msg and rts_error) */	\
		is_anticipatory_freeze_needed_fnptr = &is_anticipatory_freeze_needed;				\
		set_anticipatory_freeze_fnptr = &set_anticipatory_freeze;					\
	}													\
}

#define CHECK_IF_FREEZE_ON_ERROR_NEEDED(CSA, MSG_ID, FREEZE_NEEDED, FREEZE_MSG_ID)					\
{															\
	GBLREF	jnlpool_addrs		jnlpool;									\
	DCL_THREADGBL_ACCESS;												\
															\
	SETUP_THREADGBL_ACCESS;												\
	if (!FREEZE_NEEDED && ANTICIPATORY_FREEZE_AVAILABLE && (NULL != is_anticipatory_freeze_needed_fnptr))		\
	{	/* NOT gtmsecshr */											\
		if (IS_REPL_INST_UNFROZEN && (*is_anticipatory_freeze_needed_fnptr)((sgmnt_addrs *)CSA, MSG_ID))	\
		{													\
			FREEZE_NEEDED = TRUE;										\
			FREEZE_MSG_ID = MSG_ID;										\
		}													\
	}														\
}

#define FREEZE_INSTANCE_IF_NEEDED(CSA, FREEZE_NEEDED, FREEZE_MSG_ID)								\
{																\
	GBLREF	jnlpool_addrs		jnlpool;										\
																\
	if (FREEZE_NEEDED)													\
	{															\
		assert(NULL != set_anticipatory_freeze_fnptr);									\
		(*set_anticipatory_freeze_fnptr)((sgmnt_addrs *)CSA, FREEZE_MSG_ID);						\
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_REPLINSTFROZEN, 1,							\
				jnlpool.repl_inst_filehdr->inst_info.this_instname);						\
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_REPLINSTFREEZECOMMENT, 1, jnlpool.jnlpool_ctl->freeze_comment);	\
	}															\
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

#define REPORT_INSTANCE_UNFROZEN(FREEZE_CLEARED)										\
{																\
	GBLREF	jnlpool_addrs		jnlpool;										\
																\
	if (FREEZE_CLEARED)													\
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_REPLINSTUNFROZEN, 1,						\
				jnlpool.repl_inst_filehdr->inst_info.this_instname);						\
}

#define AFREEZE_MASK				0x01
#define ANTICIPATORY_FREEZE_AVAILABLE		(0 != (TREF(gtm_custom_errors)).len)
#define INSTANCE_FREEZE_HONORED(CSA)		(DBG_ASSERT(NULL != CSA)							\
							((NULL != jnlpool.jnlpool_ctl)						\
								&& ((REPL_ALLOWED(((sgmnt_addrs *)CSA)->hdr))			\
							    		|| mupip_jnl_recover	/* recover or rollback */	\
									|| ((sgmnt_addrs *)CSA)->nl->onln_rlbk_pid )))
#define ANTICIPATORY_FREEZE_ENABLED(CSA)	(INSTANCE_FREEZE_HONORED(CSA)				\
							&& ANTICIPATORY_FREEZE_AVAILABLE		\
							&& (((sgmnt_addrs *)CSA)->hdr->freeze_on_fail))
#define IS_REPL_INST_FROZEN			((NULL != jnlpool.jnlpool_ctl) && jnlpool.jnlpool_ctl->freeze)
#define IS_REPL_INST_UNFROZEN			((NULL != jnlpool.jnlpool_ctl) && !jnlpool.jnlpool_ctl->freeze)

#define INST_FROZEN_COMMENT			"PID %d encountered %s; Instance frozen"

#define MSGID_TO_ERRMSG(MSG_ID, ERRMSG)				\
{								\
	const err_ctl		*ctl;				\
								\
	ctl = err_check(MSG_ID);				\
	assert(NULL != ctl);					\
	GET_MSG_INFO(MSG_ID, ctl, ERRMSG);			\
}

#define GENERATE_INST_FROZEN_COMMENT(BUF, BUF_LEN, MSG_ID)			\
{										\
	GBLREF uint4		process_id;					\
	const err_msg		*msginfo;					\
										\
	MSGID_TO_ERRMSG(MSG_ID, msginfo);					\
	SNPRINTF(BUF, BUF_LEN, INST_FROZEN_COMMENT, process_id, msginfo->tag);	\
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
	if (INSTANCE_FREEZE_HONORED(CSA))											\
	{															\
		reg = ((sgmnt_addrs *)CSA)->region;										\
		if (!IS_GTM_IMAGE)												\
		{														\
			GET_CUR_TIME;												\
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_MUINSTFROZEN, 5, CTIME_BEFORE_NL, time_ptr,		\
					jnlpool.repl_inst_filehdr->inst_info.this_instname, DB_LEN_STR(reg));			\
		}														\
		WAIT_FOR_REPL_INST_UNFREEZE_NOCSA;										\
		if (!IS_GTM_IMAGE)												\
		{														\
			GET_CUR_TIME;												\
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_MUINSTUNFROZEN, 5, CTIME_BEFORE_NL, time_ptr,		\
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
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) forced_exit_err);	\
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) forced_exit_err);	\
			exit(-exi_condition);						\
		}									\
		SHORT_SLEEP(SLEEP_INSTFREEZEWAIT);					\
		DEBUG_ONLY(CLEAR_FAKE_ENOSPC_IF_MASTER_DEAD);				\
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

#define	LSEEKWRITE_IS_TO_NONE		0
#define	LSEEKWRITE_IS_TO_DB		1
#define	LSEEKWRITE_IS_TO_JNL		2

#ifdef DEBUG
#define	FAKE_ENOSPC(CSA, FAKE_WHICH_ENOSPC, LSEEKWRITE_TARGET, LCL_STATUS)						\
{															\
	GBLREF	jnlpool_addrs		jnlpool;									\
	if (NULL != CSA)												\
	{														\
		if (WBTEST_ENABLED(WBTEST_RECOVER_ENOSPC))								\
		{	/* This test case is only used by mupip */							\
			gtm_wbox_input_test_case_count++;								\
			if ((0 != gtm_white_box_test_case_count)							\
			    && (gtm_white_box_test_case_count <= gtm_wbox_input_test_case_count))			\
			{												\
				LCL_STATUS = ENOSPC;									\
				if (gtm_white_box_test_case_count == gtm_wbox_input_test_case_count)			\
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2,				\
					     LEN_AND_LIT("Turning on fake ENOSPC for exit status test"));		\
			}												\
		} else if (!IS_DSE_IMAGE /*DSE does not freeze so let it work as normal */				\
			   && ((NULL != jnlpool.jnlpool_ctl) && (NULL != ((sgmnt_addrs *)CSA)->nl))			\
			   && ((sgmnt_addrs *)CSA)->nl->FAKE_WHICH_ENOSPC)						\
		{													\
			LCL_STATUS = ENOSPC;										\
			lseekwrite_target = LSEEKWRITE_TARGET;								\
		}													\
	}														\
}

void clear_fake_enospc_if_master_dead(void);

#define CLEAR_FAKE_ENOSPC_IF_MASTER_DEAD	clear_fake_enospc_if_master_dead()

#else
#define	FAKE_ENOSPC(CSA, FAKE_ENOSPC, LSEEKWRITE_TARGET, LCL_STATUS) {}
#endif


#define	DB_LSEEKWRITE(csa, db_fn, fd, new_eof, buff, size, status)							\
	DO_LSEEKWRITE(csa, db_fn, fd, new_eof, buff, size, status, fake_db_enospc, LSEEKWRITE_IS_TO_DB)

#define	JNL_LSEEKWRITE(csa, jnl_fn, fd, new_eof, buff, size, status)							\
	DO_LSEEKWRITE(csa, jnl_fn, fd, new_eof, buff, size, status, fake_jnl_enospc, LSEEKWRITE_IS_TO_JNL)

#define DO_LSEEKWRITE(csa, fnptr, fd, new_eof, buff, size, status, FAKE_WHICH_ENOSPC, LSEEKWRITE_TARGET)			\
{																\
	int	lcl_status;													\
																\
	if (NULL != csa)													\
		WAIT_FOR_REPL_INST_UNFREEZE_SAFE(csa);										\
	LSEEKWRITE(fd, new_eof, buff, size, lcl_status);									\
	FAKE_ENOSPC(csa, FAKE_WHICH_ENOSPC, LSEEKWRITE_TARGET, lcl_status);							\
	if (ENOSPC == lcl_status)												\
	{															\
		wait_for_disk_space(csa, (char *)fnptr, fd, (off_t)new_eof, (char *)buff, (size_t)size, &lcl_status);		\
		assert((NULL == csa) || (NULL == ((sgmnt_addrs *)csa)->nl) || !((sgmnt_addrs *)csa)->nl->FAKE_WHICH_ENOSPC	\
		       || (ENOSPC != lcl_status));										\
	}															\
	status = lcl_status;													\
}

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
