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

#include "gtm_unistd.h"
#include "gtm_inet.h"
#include "gtm_signal.h"

#include <sys/shm.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "io.h"
#include "iosp.h"
#include "iotimer.h"
#include "error.h"
#include "gtm_stdio.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gt_timer.h"
#include "mutex.h"
#include "fgncalsp.h"
#include "zcall_package.h"
#include "gtm_exit_handler.h"
#include "gv_rundown.h"
#include "mprof.h"
#include "print_exit_stats.h"
#include "invocation_mode.h"
#include "secshr_db_clnup.h"
#include "gtmcrypt.h"
#include "relinkctl.h"
#include "gvcst_protos.h"
#include "op.h"

GBLREF	int4			exi_condition;
GBLREF	uint4			dollar_tlevel;
GBLREF	boolean_t		need_core;			/* Core file should be created */
GBLREF	boolean_t		created_core;			/* Core file was created */
GBLREF	unsigned int		core_in_progress;
GBLREF	boolean_t		dont_want_core;
GBLREF	boolean_t		exit_handler_active;
GBLREF	volatile int4		fast_lock_count;
GBLREF	boolean_t		skip_exit_handler;
GBLREF 	boolean_t		is_tracing_on;
#ifdef DEBUG
GBLREF 	boolean_t		stringpool_unusable;
GBLREF 	boolean_t		stringpool_unexpandable;
#endif

enum rundown_state
{
	rundown_state_lock,
	rundown_state_mprof,
	rundown_state_statsdb,
	rundown_state_db,
	rundown_state_io,
	rundown_state_last
};

static	enum rundown_state	attempting;

#ifdef DEBUG
GBLREF	int			process_exiting;
#endif

LITREF	mval		literal_notimeout;

/* This macro is a framework to help perform ONE type of rundown (e.g. db or lock or io rundown etc.).
 * "gtm_exit_handler" invokes this macro for each type of rundown that is needed and passes appropriate
 * parameters to indicate the detail needed for each rundown.
 * Note: This macro uses local variables "attempting", "error_seen" and "actual_exi_condition".
 */
#define	RUNDOWN_STEP(THISSTATE, NEXTSTATE, ERRCODE, STMT)			\
{										\
	if (THISSTATE == attempting)						\
	{									\
		if (!error_seen)						\
		{								\
			STMT;							\
		} else								\
		{								\
			if (!actual_exi_condition)				\
				actual_exi_condition = exi_condition;		\
			if (0 != ERRCODE)					\
			{							\
				PRN_ERROR;					\
				dec_err(VARLSTCNT(1) ERRCODE);			\
			}							\
		}								\
		error_seen = FALSE;						\
		attempting++;							\
	}									\
	assert(NEXTSTATE == (THISSTATE + 1));					\
}

#define	MPROF_RUNDOWN_MACRO			\
{						\
	if (is_tracing_on)			\
		turn_tracing_off(NULL);		\
}

#define	LOCK_RUNDOWN_MACRO										\
{													\
	SET_PROCESS_EXITING_TRUE;									\
	CANCEL_TIMERS;			/* Cancel all unsafe timers - No unpleasant surprises */	\
	/* Note we call secshr_db_clnup() with the flag NORMAL_TERMINATION even in an error condition	\
	 * here because we know at this point that we aren't in the middle of a transaction commit but	\
	 * crit	may be held in one or more regions and/or other odds/ends to cleanup.			\
	 */												\
	secshr_db_clnup(NORMAL_TERMINATION);								\
	zcall_halt();											\
	op_unlock();											\
	op_zdeallocate((mval *)&literal_notimeout);									\
}

#define	IO_RUNDOWN_MACRO											\
{														\
	/* Invoke cleanup routines for all the shared libraries loaded during external call initialisation.	\
	 * The cleanup routines are not mandatory routines, but if defined, will be invoked before		\
	 * closing the shared library.										\
	 */													\
	for (package_ptr = TREF(extcall_package_root); package_ptr; package_ptr = package_ptr->next_package)	\
	{													\
		if (package_ptr->package_clnup_rtn)								\
			package_ptr->package_clnup_rtn();							\
		fgn_closepak(package_ptr->package_handle, INFO);						\
	}													\
	relinkctl_rundown(TRUE, TRUE);	/* decrement relinkctl-attach & rtnobj-reference counts */		\
	assert(process_exiting);										\
	if (MUMPS_CALLIN & invocation_mode)									\
	{													\
		flush_pio();											\
		io_rundown(RUNDOWN_EXCEPT_STD);									\
	} else													\
		io_rundown(NORMAL_RUNDOWN);									\
	GTMCRYPT_CLOSE;												\
}

error_def(ERR_GVRUNDOWN);
error_def(ERR_LKRUNDOWN);
error_def(ERR_MPROFRUNDOWN);

/* Function that is invoked at process exit time to do cleanup.
 * The general flow here is to do various types of rundowns (e.g. db rundown, lock rundown, io rundown etc.).
 * If one type of rundown encounters an error midway, we want to just move on to the next type of rundown.
 * This way we do as much cleanup as possible before the process exists. Towards this, we use "exi_ch" as a
 * condition handler and the RUNDOWN_STEP macro to take care of each type of rundown.
 */
void gtm_exit_handler(void)
{
	struct sigaction		act;
	struct extcall_package_list	*package_ptr;
	boolean_t			error_seen;
	int4				actual_exi_condition;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (exit_handler_active || skip_exit_handler) /* Skip exit handling if specified or if exit handler already active */
		return;
	exit_handler_active = TRUE;
	attempting = rundown_state_lock;
	actual_exi_condition = 0;
	ESTABLISH_NORET(exi_ch, error_seen);	/* "error_seen" is initialized inside this macro */
#ifdef DEBUG
	if (WBTEST_ENABLED(WBTEST_CRASH_SHUTDOWN_EXPECTED) && (TRUE == TREF(statshare_opted_in)))
	{	/* Forced to FALSE when killing processes and we may need to rundown statsdbs */
		stringpool_unusable = FALSE;
		stringpool_unexpandable = FALSE;
		fast_lock_count = 0;
	}
#endif
	RUNDOWN_STEP(rundown_state_lock, rundown_state_mprof, ERR_LKRUNDOWN, LOCK_RUNDOWN_MACRO);
	RUNDOWN_STEP(rundown_state_mprof, rundown_state_statsdb, ERR_MPROFRUNDOWN, MPROF_RUNDOWN_MACRO);
	/* The condition handler used in the gvcst_remove_statsDB_linkage_all() path takes care of sending errors */
	RUNDOWN_STEP(rundown_state_statsdb, rundown_state_db, 0, gvcst_remove_statsDB_linkage_all());
	RUNDOWN_STEP(rundown_state_db, rundown_state_io, ERR_GVRUNDOWN, gv_rundown());
	/* We pass 0 (not ERR_IORUNDOWN) below to avoid displaying any error if io_rundown fails. One reason we have
	 * seen an external filter M program fail is with a "SYSTEM-E-ENO32, Broken pipe" error if the source or receiver
	 * server (that is communicating with it through a pipe device) closes its end of the pipe and we do not want that
	 * to be treated as an error in rundown (it is how a pipe close happens normally).
	 */
	RUNDOWN_STEP(rundown_state_io, rundown_state_last, 0, IO_RUNDOWN_MACRO);
	REVERT;
	print_exit_stats();
	if (need_core && !created_core && !dont_want_core)	/* We needed to core */
	{
		++core_in_progress;
		DUMP_CORE;		/* This will not return */
	}
	if (actual_exi_condition && !(MUMPS_CALLIN & invocation_mode))
		PROCDIE(actual_exi_condition);
}
