/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
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

#include "gtm_unistd.h"
#include "gtm_inet.h"
#include "gtm_signal.h"
#include "gtm_stdio.h"
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
#include "trace_table.h"
#include "libyottadb_int.h"
#include "gtmio.h"
#include "caller_id.h"

GBLREF	int4			exi_condition;
GBLREF	uint4			dollar_tlevel;
GBLREF	boolean_t		need_core;			/* Core file should be created */
GBLREF	boolean_t		created_core;			/* Core file was created */
GBLREF	unsigned int		core_in_progress;
GBLREF	boolean_t		dont_want_core;
GBLREF	boolean_t		exit_handler_active;
GBLREF	boolean_t		exit_handler_complete;
GBLREF	volatile int4		fast_lock_count;
GBLREF	boolean_t		skip_exit_handler;
<<<<<<< HEAD
GBLREF 	boolean_t		is_tracing_on;
GBLREF	int			fork_after_ydb_init;
=======
GBLREF	boolean_t		is_tracing_on;
GBLREF	uint4			process_id;
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
#ifdef DEBUG
GBLREF 	boolean_t		stringpool_unusable;
GBLREF 	boolean_t		stringpool_unexpandable;
GBLREF	int			process_exiting;
#endif
GBLREF	pthread_t		ydb_engine_threadsafe_mutex_holder[];

LITREF	mval			literal_notimeout;

enum rundown_state
{
	rundown_state_mprof,
	rundown_state_lock,
	rundown_state_statsdb,
	rundown_state_db,
	rundown_state_io,
	rundown_state_last,
};

static	enum rundown_state	attempting;

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
	assert(exit_handler_active);	/* From here onwards, we do not want to start any new timers	\
					 * as there won't be any more call to CANCEL_TIMERS. This	\
					 * assert ensures no new timers will be started as this		\
					 * variable is checked before "start_timer()" call in various	\
					 * places.							\
					 */								\
	/* Note we call secshr_db_clnup() with the flag NORMAL_TERMINATION even in an error condition	\
	 * here because we know at this point that we aren't in the middle of a transaction commit but	\
	 * crit	may be held in one or more regions and/or other odds/ends to cleanup.			\
	 */												\
	secshr_db_clnup(NORMAL_TERMINATION);								\
	zcall_halt();											\
	op_unlock();											\
	op_zdeallocate((mval *)&literal_notimeout);							\
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
error_def(ERR_PIDMISMATCH);

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
	if (fork_after_ydb_init)
	{	/* This process had SimpleAPI or SimpleThreadAPI active when a "fork" happened to create this child process.
		 * The YottaDB engine has not been initialized in the child process yet (or else "fork_after_ydb_init" would
		 * have been cleared) so skip any YottaDB cleanup as part of exit handling.
		 */
		return;
<<<<<<< HEAD
	}
	/* Skip exit handling if specified or if exit handler already active */
	if (exit_handler_active || skip_exit_handler)
	{
		DBGSIGHND_ONLY(fprintf(stderr, "gtm_exit_handler: Entered but already active/complete (%d/%d) - nothing to do\n",
				       exit_handler_active, exit_handler_complete); fflush(stderr));
		return;
	}
	if (simpleThreadAPI_active)
	{	/* This is a SimpleThreadAPI environment and the thread that is invoking the exit handler does not hold
		 * the YottaDB engine mutex lock. In that case, go through "ydb_exit" which gets that lock and comes back
		 * again here.
		 */
		if (!pthread_equal(ydb_engine_threadsafe_mutex_holder[0], pthread_self()))
		{
			ydb_exit();
			return;
		}
		DBGSIGHND_ONLY(fprintf(stderr, "gtm_exit_handler: Entered in simpleThreadAPI mode.. (thread %p) from %p\n",
				       (void *)pthread_self(), (void *)caller_id(0)); fflush(stderr));
	}
	DEBUG_ONLY(ydb_dmp_tracetbl());
	attempting = rundown_state_mprof;
=======
	if (process_id != getpid())
	{	/* DE476408 - Skip exit handling when there is a process_id mismatch(after FORK) to avoid a child
		 * process from removing the statsdb entry(gvcst_remove_statsDB_linkage) of its parent, which might
		 * cause database damage.
		 */
		SHORT_SLEEP(100);
		if (process_id != getpid())
		{	/* gtm8518 - a retry in order to make sure the mismatch is consistant before avoiding rundowns */
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_PIDMISMATCH, 2, process_id, getpid());
			return;
		}
	}
	exit_handler_active = TRUE;
	attempting = rundown_state_lock;
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
	actual_exi_condition = 0;
	ESTABLISH_NORET(exi_ch, error_seen);	/* "error_seen" is initialized inside this macro */
#	ifdef DEBUG
	if (WBTEST_ENABLED(WBTEST_CRASH_SHUTDOWN_EXPECTED) && (NO_STATS_OPTIN != TREF(statshare_opted_in)))
	{	/* Forced to FALSE when killing processes and we may need to rundown statsdbs */
		stringpool_unusable = FALSE;
		stringpool_unexpandable = FALSE;
		fast_lock_count = 0;
	}
#	endif
	/* Need to do this before "exit_handler_active" is set to TRUE as this step can start timers etc. (as part of opening
	 * the database file corresponding to the traced global name) and that is disallowed once "exit_handler_active" is TRUE.
	 */
	RUNDOWN_STEP(rundown_state_mprof, rundown_state_lock, ERR_MPROFRUNDOWN, MPROF_RUNDOWN_MACRO);
	exit_handler_active = TRUE;
	RUNDOWN_STEP(rundown_state_lock, rundown_state_statsdb, ERR_LKRUNDOWN, LOCK_RUNDOWN_MACRO);
	/* The condition handler used in the gvcst_remove_statsDB_linkage_all() path takes care of sending errors */
	RUNDOWN_STEP(rundown_state_statsdb, rundown_state_db, 0, gvcst_remove_statsDB_linkage_all());
	RUNDOWN_STEP(rundown_state_db, rundown_state_io, ERR_GVRUNDOWN, gv_rundown());
	/* We pass 0 (not ERR_IORUNDOWN) below to avoid displaying any error if io_rundown fails. One reason we have
	 * seen an external filter M program fail is with a "SYSTEM-E-ENO32, Broken pipe" error if the source or receiver
	 * server (that is communicating with it through a pipe device) closes its end of the pipe and we do not want that
	 * to be treated as an error in rundown (it is how a pipe close happens normally).
	 */
	RUNDOWN_STEP(rundown_state_io, rundown_state_last, 0, IO_RUNDOWN_MACRO);
	/* One last step before finishing up is to drain our signal queues if we were using this. This will release any
	 * threads blocked on signals as the in-process queues are also cleaned up and their internal msems posted.
	 */
	if (USING_ALTERNATE_SIGHANDLING)
		drain_signal_queues(NULL);
	REVERT;
	exit_handler_complete = TRUE;
	print_exit_stats();
	if (need_core && !created_core && !dont_want_core)	/* We needed to core */
	{
		++core_in_progress;
		DUMP_CORE;		/* This will not return */
	}
	if (actual_exi_condition && !(MUMPS_CALLIN & invocation_mode))
		PROCDIE(actual_exi_condition);
	DBGSIGHND_ONLY(if (USING_ALTERNATE_SIGHANDLING)
		{
			fprintf(stderr, "gtm_exit_handler(): Complete - returning\n");
			fflush(stderr);
		}
	);
}
