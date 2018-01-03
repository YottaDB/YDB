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

#ifndef HAVE_CRIT_H_INCLUDED
#define HAVE_CRIT_H_INCLUDED

#include <signal.h>				/* needed for VSIG_ATOMIC_T */
#ifdef UNIX
#include <deferred_signal_handler.h>
#endif

/* states of CRIT passed as argument to have_crit() */
#define CRIT_HAVE_ANY_REG	0x00000001
#define CRIT_IN_COMMIT		0x00000002
#define CRIT_NOT_TRANS_REG	0x00000004
#define CRIT_RELEASE		0x00000008
#define CRIT_ALL_REGIONS	0x00000010
#define	CRIT_IN_WTSTART		0x00000020	/* check if csa->in_wtstart is true */

#ifdef DEBUG
#include "wbox_test_init.h"
#endif
#ifdef UNIX
#include "gt_timer.h"
#include "gtm_multi_thread.h"
#include "gtm_multi_proc.h"
#endif

typedef enum
{
	INTRPT_OK_TO_INTERRUPT = 0,
	INTRPT_IN_GTCMTR_TERMINATE,
	INTRPT_IN_TP_UNWIND,
	INTRPT_IN_TP_CLEAN_UP,
	INTRPT_IN_CRYPT_SECTION,
	INTRPT_IN_DB_CSH_GETN,
	INTRPT_IN_GVCST_INIT,
	INTRPT_IN_GDS_RUNDOWN,
	INTRPT_IN_SS_INITIATE,
	INTRPT_IN_ZLIB_CMP_UNCMP,
	INTRPT_IN_TRIGGER_NOMANS_LAND,	/* State where have trigger base frame but no trigger (exec) frame. */
	INTRPT_IN_MUR_OPEN_FILES,
	INTRPT_IN_TRUNC,
	INTRPT_IN_SET_NUM_ADD_PROCS,
	INTRPT_IN_SYSCONF,
	INTRPT_NO_TIMER_EVENTS,		/* State where primary reason for deferral is to avoid timer pops. */
	INTRPT_IN_FFLUSH,		/* Deferring interrupts during fflush. */
	INTRPT_IN_SHMDT,		/* Deferring interrupts during SHMDT. */
	INTRPT_IN_WAIT_FOR_DISK_SPACE,	/* Deferring interrupts during wait_for_disk_space.c. */
	INTRPT_IN_WCS_WTSTART,		/* Deferring interrupts until cnl->intent_wtstart is decremented and dbsync timer is
					 * started. */
	INTRPT_IN_X_TIME_FUNCTION,	/* Deferring interrupts in non-nesting functions, such as localtime, ctime, and mktime. */
	INTRPT_IN_FUNC_WITH_MALLOC,	/* Deferring interrupts while in libc- or system functions that do a malloc internally. */
	INTRPT_IN_FDOPEN,		/* Deferring interrupts in fdopen. */
	INTRPT_IN_LOG_FUNCTION,		/* Deferring interrupts in openlog, syslog, or closelog. */
	INTRPT_IN_FORK_OR_SYSTEM,	/* Deferring interrupts in fork or system. */
	INTRPT_IN_FSTAT,		/* Deferring interrupts in fstat. */
	INTRPT_IN_TLS_FUNCTION,		/* Deferring interrupts in TLS functions. */
	INTRPT_IN_CONDSTK,		/* Deferring interrupts during condition handler stack manipulations. */
	INTRPT_IN_GTMIO_CH_SET,		/* Deferring interrupts during the setting of gtmio_ch condition handler. */
	INTRPT_IN_OBJECT_FILE_COMPILE,	/* Deferring interrupts during the creation of an object file. */
	INTRPT_IN_IO_READ,		/* Deferring interrupts for async signal unsafe calls in iorm_readfl.c. */
	INTRPT_IN_IO_WRITE,		/* Deferring interrupts for async signal unsafe calls in iorm_write.c. */
	INTRPT_IN_PTHREAD_NB,		/* Deferring interrupts for non-blocking async signal unsafe calls. */
	INTRPT_IN_GTM_MULTI_PROC,	/* Deferring interrupts while inside "gtm_multi_proc" function */
	INTRPT_IN_EINTR_WRAPPERS,	/* Deferring interrupts while inside "eintr_wrappers.h" macros */
	INTRPT_IN_MKSTEMP,		/* Deferring interrupts while in mkstemp */
	INTRPT_IN_CRYPT_RECONFIG,	/* Deferring interrupts during reconfiguration of the encryption state. */
	INTRPT_IN_UNLINK_AND_CLEAR,	/* Deferring interrupts around unlink and clearing the filename being unlinked */
	INTRPT_IN_GETC,			/* Deferring interrupts around GETC() call */
	INTRPT_IN_AIO_ERROR,		/* Deferring interrupts around aio_error() call */
	INTRPT_NUM_STATES		/* Should be the *last* one in the enum. */
} intrpt_state_t;

GBLREF	intrpt_state_t	intrpt_ok_state;
GBLREF	boolean_t	deferred_timers_check_needed;

/* Macro to check if we are in a state that is ok to interrupt (or to do deferred signal handling). We do not want to interrupt if
 * the global variable intrpt_ok_state indicates it is not ok to interrupt, if we are in the midst of a malloc, if we are holding
 * crit, or if we are in the midst of a commit.
 */
#define	OK_TO_INTERRUPT	((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) && (0 == gtmMallocDepth)			\
				&& (0 == have_crit(CRIT_HAVE_ANY_REG | CRIT_IN_COMMIT)))

/* Set the value of forced_exit to 1. This should indicate that we want a deferred signal handler to be invoked first upon leaving
 * the current deferred window. Since we do not want forced_exit state to ever regress, and there might be several signals delivered
 * within the same deferred window, assert that forced_exit is either 0 or 1 before setting it to 1.
 */
#define SET_FORCED_EXIT_STATE									\
{												\
	char			*rname;								\
	GBLREF VSIG_ATOMIC_T	forced_exit;							\
												\
	/* Below code is not thread safe as it modifies global variable "forced_exit" */	\
	assert(!INSIDE_THREADED_CODE(rname));							\
	assert((0 == forced_exit) || (1 == forced_exit));					\
	forced_exit = 1;									\
	SET_FORCED_THREAD_EXIT; /* Signal any running threads to stop */			\
	SET_FORCED_MULTI_PROC_EXIT; /* Signal any parallel processes to stop */			\
}

/* Set the value of forced_exit to 2. This should indicate that we are already in the exit processing, and do not want to handle any
 * deferred events, from timers or other interrupts, anymore. Ensure that forced_exit state does not regress by asserting that the
 * current value is 0 or 1 before setting it to 2. Note that on UNIX forced_exit can progress to 2 only from 1, while on VMS it is
 * possible to generic_exit_handler or dbcertify_exit_handler to change forced_exit from 0 to 2 directly. This design ensures that
 * on VMS we will not invoke sys$exit from DEFERRED_EXIT_HANDLING_CHECK after we started exit processing; on UNIX process_exiting
 * flag servs the same purpose (and is also checked by DEFERRED_EXIT_HANDLING_CHECK), so it is not necessary for either
 * generic_signal_handler or dbcertify_signal_handler to set forced_exit to 2.
 */
#define SET_FORCED_EXIT_STATE_ALREADY_EXITING							\
{												\
	char			*rname;								\
	GBLREF VSIG_ATOMIC_T	forced_exit;							\
	GBLREF boolean_t	forced_thread_exit;						\
												\
	/* Below code is not thread safe as it modifies global variable "forced_exit" */	\
	assert(!INSIDE_THREADED_CODE(rname));							\
	assert(1 == forced_exit);								\
	assert(forced_thread_exit);								\
	forced_exit = 2;									\
}

/* Macro to be used whenever we want to handle any signals that we deferred handling and exit in the process.
 * In VMS, we dont do any signal handling, only exit handling.
 */
#define	DEFERRED_EXIT_HANDLING_CHECK									\
{													\
	char			*rname;									\
													\
	GBLREF	int		process_exiting;							\
	GBLREF	VSIG_ATOMIC_T	forced_exit;								\
	GBLREF	volatile int4	gtmMallocDepth;								\
													\
	/* The forced_exit state of 2 indicates that the exit is already in progress, so we do not	\
	 * need to process any deferred events. Note if threads are running, check if forced_exit is	\
	 * non-zero and if so exit the thread (using pthread_exit) otherwise skip deferred event	\
	 * processing. A similar check will happen once threads stop running.				\
	 */												\
	if (INSIDE_THREADED_CODE(rname))								\
	{												\
		PTHREAD_EXIT_IF_FORCED_EXIT;								\
	} else if (2 > forced_exit)									\
	{	/* If forced_exit was set while in a deferred state, disregard any deferred timers and	\
		 * invoke deferred_signal_handler directly.						\
		 */											\
		if (forced_exit)									\
		{											\
			if (!process_exiting && OK_TO_INTERRUPT)					\
				deferred_signal_handler();						\
		} else if (deferred_timers_check_needed)						\
		{											\
			if (!process_exiting && OK_TO_INTERRUPT)					\
				check_for_deferred_timers();						\
		}											\
	}												\
}

GBLREF	boolean_t	multi_thread_in_use;		/* TRUE => threads are in use. FALSE => not in use */

/* Macro to cause deferrable interrupts to be deferred, recording the cause.
 * If interrupt is already deferred, state is not changed.
 *
 * The normal usage of the below macros is
 *	DEFER_INTERRUPTS
 *	non-interruptible code
 *	ENABLE_INTERRUPTS
 * We want the non-interruptible code to be executed AFTER the SAVE_INTRPT_OK_STATE macro.
 * To enforce this ordering, one would think a read memory barrier is needed in between.
 * But it is not needed. This is because we expect the non-interruptible code to have
 *	a) pointer dereferences OR
 *	b) function calls
 * Either of these will prevent the compiler from reordering the non-interruptible code.
 * Any non-interruptible code that does not have either of the above usages (for e.g. uses C global
 * variables) might be affected by compiler reordering. As of now, there is no known case of such
 * usage and no such usage is anticipated in the future.
 *
 * We dont need to worry about machine reordering as well since there is no shared memory variable
 * involved here (intrpt_ok_state is a process private variable) and even if any reordering occurs
 * they will all be in-flight instructions when the interrupt occurs so the hardware will guarantee
 * all such instructions are completely done or completely discarded before servicing the interrupt
 * which means the interrupt service routine will never see a reordered state of the above code.
 *
 * If we are currently executing in a thread then interrupts will not be delivered to us but instead
 * will go to the master process (that spawned off threads like us) and therefore the thread flow is
 * not expected to be interrupted. So skip the macro processing in that case. This is necessary because
 * it manipulates the global variable "intrpt_ok_state" which is a no-no inside threaded code.
 */

/* NEWSTATE is an Input parameter. OLDSTATE is an Output parameter (later used as NEWSTATE parameter to
 * ENABLE_INTERRUPTS macro). Callers, e.g. ESTABLISH_RET, rely on OLDSTATE being set irrespective of
 * whether multi_thread_in_use is set or not.
 */
#define DEFER_INTERRUPTS(NEWSTATE, OLDSTATE)								\
{													\
	OLDSTATE = intrpt_ok_state;									\
	if (!multi_thread_in_use)									\
	{												\
		assert(INTRPT_OK_TO_INTERRUPT != NEWSTATE);						\
		intrpt_ok_state = NEWSTATE;								\
	}												\
}

/* Restore deferrable interrupts back to the state it was at time of corresponding DEFER_INTERRUPTS call */
#define ENABLE_INTERRUPTS(OLDSTATE, NEWSTATE)									\
{														\
	if (!multi_thread_in_use)										\
	{													\
		assert(OLDSTATE == intrpt_ok_state);								\
		intrpt_ok_state = NEWSTATE;									\
		if (INTRPT_OK_TO_INTERRUPT == intrpt_ok_state)							\
			DEFERRED_EXIT_HANDLING_CHECK;	/* check if signals were deferred in deferred zone */	\
	}													\
}
#define	OK_TO_SEND_MSG	((INTRPT_IN_X_TIME_FUNCTION != intrpt_ok_state) 				\
			&& (INTRPT_IN_LOG_FUNCTION != intrpt_ok_state)					\
			&& (INTRPT_IN_FUNC_WITH_MALLOC != intrpt_ok_state)				\
			&& (INTRPT_IN_FORK_OR_SYSTEM != intrpt_ok_state))

uint4 have_crit(uint4 crit_state);

#endif /* HAVE_CRIT_H_INCLUDED */
