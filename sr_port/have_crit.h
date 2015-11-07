/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
	INTRPT_IN_TRIGGER_NOMANS_LAND,	/* State where have trigger base frame but no trigger (exec) frame */
	INTRPT_IN_MUR_OPEN_FILES,
	INTRPT_IN_TRUNC,
	INTRPT_IN_SET_NUM_ADD_PROCS,
	INTRPT_IN_SYSCONF,
	INTRPT_NO_TIMER_EVENTS,		/* State where primary reason for deferral is to avoid timer pops */
	INTRPT_IN_FFLUSH,		/* Deferring interrupts during fflush */
	INTRPT_IN_SHMDT,		/* Deferring interrupts during SHMDT */
	INTRPT_IN_WAIT_FOR_DISK_SPACE,	/* Deferring interrupts during wait_for_disk_space.c */
	INTRPT_IN_WCS_WTSTART,		/* Deferring interrupts until cnl->intent_wtstart is decremented and dbsync timer is
					 * started */
	INTRPT_IN_REFORMAT_BUFFER_USE,	/* Deferring interrupts until buffer is reformatted */
	INTRPT_IN_X_TIME_FUNCTION,	/* Deferring interrupts in non-nesting functions, such as localtime, ctime, and mktime. */
	INTRPT_IN_FUNC_WITH_MALLOC,	/* Deferring interrupts while in libc- or system functions that do a malloc internally. */
	INTRPT_IN_FDOPEN,		/* Deferring interrupts in fdopen. */
	INTRPT_IN_LOG_FUNCTION,		/* Deferring interrupts in openlog, syslog, or closelog. */
	INTRPT_IN_FORK_OR_SYSTEM,	/* Deferring interrupts in fork or system. */
	INTRPT_IN_FSTAT,		/* Deferring interrupts in fstat. */
	INTRPT_NUM_STATES		/* Should be the *last* one in the enum */
} intrpt_state_t;

GBLREF	intrpt_state_t	intrpt_ok_state;
GBLREF	boolean_t	deferred_timers_check_needed;

/* Macro to check if we are in a state that is ok to interrupt (or to do deferred signal handling). We do not want to interrupt if
 * the global variable intrpt_ok_state indicates it is not ok to interrupt, if we are in the midst of a malloc, if we are holding
 * crit, if we are in the midst of commit, or in wcs_wtstart. In the last case, we could be causing another process HOLDING CRIT on
 * the region to wait in bg_update_phase1 if we hold the write interlock. Hence it is important for us to finish that as soon as
 * possible and not interrupt it.
 */
#define	OK_TO_INTERRUPT	((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) && (0 == gtmMallocDepth)			\
				&& (0 == have_crit(CRIT_HAVE_ANY_REG | CRIT_IN_COMMIT | CRIT_IN_WTSTART)))

/* Set the value of forced_exit to 1. This should indicate that we want a deferred signal handler to be invoked first upon leaving
 * the current deferred window. Since we do not want forced_exit state to ever regress, and there might be several signals delivered
 * within the same deferred window, assert that forced_exit is either 0 or 1 before setting it to 1.
 */
#define SET_FORCED_EXIT_STATE						\
{									\
	GBLREF VSIG_ATOMIC_T forced_exit;				\
									\
	assert((0 == forced_exit) || (1 == forced_exit));		\
	forced_exit = 1;						\
}

/* Set the value of forced_exit to 2. This should indicate that we are already in the exit processing, and do not want to handle any
 * deferred events, from timers or other interrupts, anymore. Ensure that forced_exit state does not regress by asserting that the
 * current value is 0 or 1 before setting it to 2. Note that on UNIX forced_exit can progress to 2 only from 1, while on VMS it is
 * possible to generic_exit_handler or dbcertify_exit_handler to change forced_exit from 0 to 2 directly. This design ensures that
 * on VMS we will not invoke sys$exit from DEFERRED_EXIT_HANDLING_CHECK after we started exit processing; on UNIX process_exiting
 * flag servs the same purpose (and is also checked by DEFERRED_EXIT_HANDLING_CHECK), so it is not necessary for either
 * generic_signal_handler or dbcertify_signal_handler to set forced_exit to 2.
 */
#define SET_FORCED_EXIT_STATE_ALREADY_EXITING				\
{									\
	GBLREF VSIG_ATOMIC_T forced_exit;				\
									\
	assert(VMS_ONLY((0 == forced_exit) || ) (1 == forced_exit));	\
	forced_exit = 2;						\
}

/* Macro to be used whenever we want to handle any signals that we deferred handling and exit in the process.
 * In VMS, we dont do any signal handling, only exit handling.
 */
#define	DEFERRED_EXIT_HANDLING_CHECK									\
{													\
	VMS_ONLY(GBLREF	int4	exi_condition;)								\
	GBLREF	int		process_exiting;							\
	GBLREF	VSIG_ATOMIC_T	forced_exit;								\
	GBLREF	volatile int4	gtmMallocDepth;								\
													\
	/* The forced_exit state of 2 indicates that the exit is already in progress, so we do not	\
	 * need to process any deferred events.								\
	 */												\
	if (2 > forced_exit)										\
	{	/* If forced_exit was set while in a deferred state, disregard any deferred timers and	\
		 * invoke deferred_signal_handler directly.						\
		 */											\
		if (forced_exit)									\
		{											\
			if (!process_exiting && OK_TO_INTERRUPT)					\
			{										\
				UNIX_ONLY(deferred_signal_handler();)					\
				VMS_ONLY(sys$exit(exi_condition);)					\
			}										\
		} 											\
		UNIX_ONLY(										\
		else if (deferred_timers_check_needed)							\
		{											\
			if (!process_exiting && OK_TO_INTERRUPT)					\
				check_for_deferred_timers();						\
		}											\
		)											\
	}												\
}

/* Macro to cause deferrable interrupts to be deferred recording the cause.
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
 */
#define DEFER_INTERRUPTS(NEWSTATE)									\
{													\
	if (INTRPT_OK_TO_INTERRUPT == intrpt_ok_state)							\
		/* Only reset state if we are in "OK" state */						\
		intrpt_ok_state = NEWSTATE;								\
	else												\
		assert((NEWSTATE) != intrpt_ok_state);	/* Make sure not nesting same code */		\
}

/* Re-enable deferrable interrupts if the expected state is found. If expected state is not found, then
 * we must have nested interrupt types. Avoid state changes in that case. When the nested state pops,
 * interrupts will be restored.
 */
#define ENABLE_INTERRUPTS(OLDSTATE)									\
{													\
	assert(((OLDSTATE) == intrpt_ok_state) || (INTRPT_OK_TO_INTERRUPT != intrpt_ok_state));		\
	if ((OLDSTATE) == intrpt_ok_state)								\
	{	/* Only reset state if in expected state - othwise state must be non-zero which is	\
		 * asserted above.									\
		 */											\
		intrpt_ok_state = INTRPT_OK_TO_INTERRUPT;						\
		DEFERRED_EXIT_HANDLING_CHECK;	/* check if signals were deferred while held lock */	\
	}												\
}

#define	OK_TO_SEND_MSG	((INTRPT_IN_X_TIME_FUNCTION != intrpt_ok_state) 				\
			&& (INTRPT_IN_LOG_FUNCTION != intrpt_ok_state)					\
			&& (INTRPT_IN_FORK_OR_SYSTEM != intrpt_ok_state))

uint4 have_crit(uint4 crit_state);

#endif /* HAVE_CRIT_H_INCLUDED */
