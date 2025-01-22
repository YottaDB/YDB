/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Standard signal processor
 *
 * If we are nesting our handlers in an improper way, this routine will
 * not return but will immediately invoke core/termination processing.
 *
 * Returns if some condition makes it inadvisable to exit now else invokes the system EXIT() system call.
 * For GTMSECSHR it unconditionally returns to gtmsecshr_signal_handler() which later invokes gtmsecshr_exit().
 */

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"		/* for EXIT() */
#include "gtm_inet.h"
#include "gtm_signal.h"
#include "gtm_stdio.h"

#include "gtm_multi_thread.h"
#include "error.h"
#include "gtmsiginfo.h"
#include "gtmimagename.h"
#include "gt_timer.h"
#include "send_msg.h"
#include "generic_signal_handler.h"
#include "ydb_os_signal_handler.h"
#include "sig_init.h"
#include "gtmmsg.h"
#include "io.h"
#include "gtmio.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "util.h"
/* For gd_region */
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "libyottadb_int.h"
#include "invocation_mode.h"
#include "gtm_exit_handler.h"
#include "sighnd_debug.h"
#include "signal_exit_handler.h"

/* If we are in a signal handler, we want to defer exit processing as the exit handler can invoke various functions
 * that are not async-signal safe (e.g. malloc/free/syslog etc.). Hence the check for "in_os_signal_handler" below.
 * But if "exit_state" is EXIT_IMMED (happens if the user sends 3 such signals), the user is most likely wanting the
 * process to terminate right away so we skip this safety check and proceed to exit in the hope that we won't encounter
 * any issues with invoking such async-signal unsafe functions inside the signal handler. Hence the check for "exit_state"
 * before we do the "in_os_signal_handler" check.
 */
#define	DEFER_EXIT_PROCESSING	((EXIT_PENDING_TOLERANT >= exit_state)			\
				 && (exit_handler_active || multi_thread_in_use		\
				     || in_os_signal_handler || multi_proc_in_use || !OK_TO_INTERRUPT))

/* Combine send_msg and gtm_putmsg into one macro to conserve space. */
#define SEND_AND_PUT_MSG(...)					\
{								\
	if (OK_TO_SEND_MSG)					\
		send_msg_csa(CSA_ARG(NULL) __VA_ARGS__);	\
	gtm_putmsg_csa(CSA_ARG(NULL) __VA_ARGS__);		\
}

#define	ALTERNATE_SIGHANDLING_SAVE_SIGNUM(using_alternate_sighandling)					\
{													\
	if (using_alternate_sighandling)								\
	{												\
		SAVE_OS_SIGNAL_HANDLER_SIGNUM(sig_hndlr_generic_signal_handler, sig);			\
		/* Currently "info" and "context" are uninitialized when invoked from YDBGo.		\
		 * If that changes, we need to uncomment the below lines.				\
		 *	SAVE_OS_SIGNAL_HANDLER_INFO(sig_hndlr_generic_signal_handler, info);		\
		 *	SAVE_OS_SIGNAL_HANDLER_INFO(sig_hndlr_generic_signal_handler, context);		\
		 */											\
	}												\
}

/* This macro needs to be invoked while returning from "generic_signal_handler()". This is because it is possible
 * we got the "tLevel"th YottaDB engine multi-thread lock inside the FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED macro
 * call at the start of the function. If so, we need to unlock before the return to avoid deadlocks.
 * Note that "pthread_mutex_unlock()" is not async-signal-safe (i.e. it should not be used inside a signal handler
 * (which this "generic_signal_handler" is) but for the same reasons that the lock was obtained using "pthread_mutex_trylock()"
 * (which is also not async-signal-safe) we release the lock here (to avoid messier alternatives when this just works).
 */
#define	CLEANUP_AND_RETURN(GOT_TLEVEL_LOCK)									\
{														\
	GBLREF	pthread_mutex_t	ydb_engine_threadsafe_mutex[];							\
	GBLREF	pthread_t	ydb_engine_threadsafe_mutex_holder[];						\
														\
	if (0 < GOT_TLEVEL_LOCK)										\
	{													\
		int	status;											\
														\
		ydb_engine_threadsafe_mutex_holder[GOT_TLEVEL_LOCK - 1] = 0;					\
		status = pthread_mutex_unlock(&ydb_engine_threadsafe_mutex[GOT_TLEVEL_LOCK - 1]);		\
		assert(0 == status);										\
		/* Not much we can do in Release builds if this fails since we are inside a signal handler. */	\
	}													\
	return;													\
}

GBLREF	int4			forced_exit_err;
GBLREF	int4			forced_exit_sig;
GBLREF	int4			exi_condition;
GBLREF	boolean_t		dont_want_core;
GBLREF	boolean_t		created_core;
GBLREF	boolean_t		need_core;
GBLREF	uint4			process_id;
GBLREF	volatile int4		exit_state;
GBLREF	ABS_TIME		mu_stop_tm_array[EXIT_IMMED - 1]; /* Save times of previous MUPIP STOPs */
GBLREF	volatile unsigned int	core_in_progress;
GBLREF	gtmsiginfo_t		signal_info;
GBLREF	boolean_t		exit_handler_active;
GBLREF	boolean_t		exit_handler_complete;
GBLREF	void			(*exit_handler_fptr)();
GBLREF	intrpt_state_t		intrpt_ok_state;
GBLREF	int			last_sig;
GBLREF	boolean_t		ydb_quiet_halt;
GBLREF	volatile int4           gtmMallocDepth;         	  /* Recursion indicator */
GBLREF	volatile boolean_t	timer_active;
GBLREF	sigset_t		block_sigsent;
GBLREF	gd_region		*gv_cur_region;
GBLREF	boolean_t		blocksig_initialized;
GBLREF	boolean_t		mu_reorg_process;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sigset_t		blockalrm;
#ifdef DEBUG
GBLREF	boolean_t		in_nondeferrable_signal_handler;
#endif

LITREF	gtmImageName		gtmImageNames[];

/* The below array is indexed on "exit_state" and is TRUE when a signal is received directly from the OS
 * (instead of being forwarded). This is used to avoid duplicate issue of FORCEDHALT messages in case
 * "generic_signal_handler" gets called multiple times for the same external signal (say SIGTERM).
 * Below are the possibilities.
 *	(a) by the OS
 *	(b) by a "pthread_kill" in the FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED macro or
 *	(c) by a "pthread_kill" from the STAPI signal thread ("ydb_stm_thread")
 *
 * Note that when using alternate signal handling (currently, only with the Go wrapper), this mechanism is
 * still active but it is "harder" to get it to happen because each succeeding signal must be "noticed" as
 * pending before this handler gets driven.
 */
STATICDEF	boolean_t	non_forwarded_sig_seen[EXIT_IMMED + 1];

error_def(ERR_FORCEDHALT);
error_def(ERR_FORCEDHALT2);
error_def(ERR_GTMSECSHRSHUTDN);
error_def(ERR_KILLBYSIG);
error_def(ERR_KILLBYSIGSINFO1);
error_def(ERR_KILLBYSIGSINFO2);
error_def(ERR_KILLBYSIGSINFO3);
error_def(ERR_KILLBYSIGUINFO);
error_def(ERR_KRNLKILL);
error_def(ERR_STATSDBMEMERR);

static inline void check_for_statsdb_memerr()
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	if (TREF(gvcst_statsDB_open_ch_active))
	{	/* Case where we've tried to create a stats db block
		 * Issue an rts error and let the statsDB condition handler
		 * do the clean up
		 */
		TREF(statsdb_memerr) = TRUE;
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_STATSDBMEMERR, 2, gv_cur_region->dyn.addr->fname_len,
				gv_cur_region->dyn.addr->fname);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_STATSDBMEMERR, 2, gv_cur_region->dyn.addr->fname_len,
			gv_cur_region->dyn.addr->fname);
	}
}

boolean_t is_timer_initialized(ABS_TIME timer) { return (((0 == timer.tv_sec) && (0 == timer.tv_nsec)) ? FALSE : TRUE); }

void generic_signal_handler(int sig, siginfo_t *info, void *context, boolean_t is_os_signal_handler)
{
	boolean_t		signal_forwarded, is_sigterm;
	int			rc;
	sigset_t		savemask;
	boolean_t		using_alternate_sighandling;
	intrpt_state_t		prev_intrpt_state;
	ABS_TIME		mu_stop_timer;
#	ifdef DEBUG
	boolean_t		save_in_nondeferrable_signal_handler;
#	endif
	uint4			got_tlevel_lock, *got_tlevel_lock_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	using_alternate_sighandling = USING_ALTERNATE_SIGHANDLING;	/* Simpler local version */
	got_tlevel_lock = 0;
	/* Check for rethrown signal before we check forwarding. When deferred_exit_handler() processes a deferred signal,
	 * some non-M languages (specifically Golang, perhaps others) rethrow the signal so it comes through here twice.
	 * Since we run the exit handler logic just prior to invoking non-YDB signal handlers that were in place before
	 * YDB was initialized, we can check exit_handler_complete to see if it has already run. If it has, just return.
	 */
	if (exit_handler_complete)
		CLEANUP_AND_RETURN(got_tlevel_lock);		/* Nothing we can do if exit handler has run */
	if (!using_alternate_sighandling)
	{
		signal_forwarded = IS_SIGNAL_FORWARDED(sig_hndlr_generic_signal_handler, sig, info);
		/* Note down whether signal is forwarded or not */
		if (!signal_forwarded)
		{
			assert(EXIT_IMMED >= exit_state);
			assert(!non_forwarded_sig_seen[exit_state]);
			non_forwarded_sig_seen[exit_state] = TRUE;
		}
		got_tlevel_lock_ptr = &got_tlevel_lock;	/* "got_tlevel_lock_ptr" needed to avoid a [-Waddress] warning on RHEL 7 */
		FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig_hndlr_generic_signal_handler, sig, got_tlevel_lock_ptr, info, context);
		/* It is possible that the exit handler has been started but not finished when we get a signal. Check that here. If
		 * true, we just ignore it letting the earlier signal finish its processing.
		 */
		if (exit_handler_active && signal_forwarded && (sig == exi_condition))
		{       /* a) exit_handler_active is TRUE but exit_handler_complete is FALSE implies we are inside but have not
			 *    completed the exit handler record at (*exit_handler_fptr).
			 * b) signal_forwarded implies this is a forwarded signal.
			 * c) sig == exi_condition implies we already are exiting because of this very same signal.
			 * In this case, we let the original exit handler invocation continue by returning from this right away.
			 */
			CLEANUP_AND_RETURN(got_tlevel_lock);
		}
	} else
	{	/* Note signals are never considered "forwarded" in alterate sighandling mode - at least in the sense of the
		 * the code when the regular "C" signal handling is in effect. Alternate signal handling is where YDB is
		 * "notified" of signals having occurred so have no need to be "forwarded" to different threads.
		 */
		assert(!is_os_signal_handler);
		signal_forwarded = FALSE;
		assert(EXIT_IMMED >= exit_state);
		assert(!non_forwarded_sig_seen[exit_state]);
		non_forwarded_sig_seen[exit_state] = TRUE;
	}
	/* Now that we know FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED did not return, we hold the YDB engine lock in case this is a
	 * multi-threaded program. Therefore we can safely set the global variable "in_os_signal_handler".
	 */
	if (is_os_signal_handler)
		INCREMENT_IN_OS_SIGNAL_HANDLER;
	/* It is possible that the exit handler has been started but not finished when we get a signal. Check that here. If
	 * true, we just ignore it letting the earlier signal finish its processing.
	 */
	if (exit_handler_active && signal_forwarded && (sig == exi_condition))
	{       /* a) exit_handler_active is TRUE but exit_handler_complete is FALSE implies we are inside but have not
		 *    completed the exit handler record at (*exit_handler_fptr).
		 * b) signal_forwarded implies this is a forwarded signal.
		 * c) sig == exi_condition implies we already are exiting because of this very same signal.
		 * In this case, we let the original exit handler invocation continue by returning from this right away.
		 */
		DECREMENT_IN_OS_SIGNAL_HANDLER_IF_NEEDED;
		CLEANUP_AND_RETURN(got_tlevel_lock);
	}
	assert(!thread_block_sigsent || blocksig_initialized);
	/* If "thread_block_sigsent" is TRUE, it means the threads do not want the master thread to honor external signals
	 * anymore until the threads complete. Achieve that effect by returning right away from the signal handler.
	 */
	if (thread_block_sigsent && sigismember(&block_sigsent, sig))
	{
		DECREMENT_IN_OS_SIGNAL_HANDLER_IF_NEEDED;
		CLEANUP_AND_RETURN(got_tlevel_lock);
	}
#	ifdef DEBUG
	/* Note that it is possible "in_nondeferrable_signal_handler" is non-zero if we first went into timer_handler
	 * and then came here due to a nested signal (e.g. SIG-15). So save current value of global and restore it at
	 * end of this function even though we will most often not return to the caller (process will exit mostly).
	 */
	save_in_nondeferrable_signal_handler = in_nondeferrable_signal_handler;
#	endif
	/* Save parameter value in global variables for easy access in core */
	dont_want_core = FALSE;		/* (re)set in case we recurse */
	created_core = FALSE;		/* we can deal with a second core if needbe */
	exi_condition = sig;
	/* Check if we are fielding nested immediate shutdown signals */
	if (EXIT_IMMED <= exit_state)
	{
		switch(sig)
		{	/* If we are dealing with one of these three dangerous signals which we have
			 * already hit while attempting to shutdown once, die with core now.
			 */
			case SIGSEGV:
			case SIGBUS:
			case SIGILL:
				DEBUG_ONLY(in_nondeferrable_signal_handler = IN_GENERIC_SIGNAL_HANDLER);
				if (core_in_progress)
				{
					if (exit_handler_active)
						UNDERSCORE_EXIT(sig);
					else
						EXIT(sig);
				}
				++core_in_progress;
				DUMP_CORE;
				assertpro(!((SIGSEGV == sig) || (SIGBUS == sig) || (SIGKILL == sig)));
			default:
				;
		}
	}
	switch(sig)
	{
		case SIGTERM:
		case SIGINT:	/* Note: "generic_signal_handler" is SIGINT handler only in SimpleAPI mode
				 * (see "sig_init" invocation in sr_unix/gtm_startup.c).
				 */
			if (!IS_GTMSECSHR_IMAGE)
			{
				is_sigterm = (SIGTERM == sig);
				forced_exit_err = is_sigterm ? ERR_FORCEDHALT : ERR_CTRLC;
				/* If nothing pending AND we have crit or in wcs_wtstart() or already in exit processing, wait to
				 * invoke shutdown. wcs_wtstart() manipulates the active queue that a concurrent process in crit
				 * in bt_put() might be waiting for. interrupting it can cause deadlocks (see C9C11-002178).
				 */
				if (mu_reorg_process && OK_TO_INTERRUPT && cs_data && cs_data->kill_in_prog)
					DEFER_INTERRUPTS(INTRPT_IN_KILL_CLEANUP, prev_intrpt_state);	/* avoid ABANDONEDKILL */
				if (DEFER_EXIT_PROCESSING)
				{
					if (!is_timer_initialized(mu_stop_tm_array[0]))
						sys_get_curr_time(&mu_stop_tm_array[0]);
					else if (!is_timer_initialized(mu_stop_tm_array[1]))
						sys_get_curr_time(&mu_stop_tm_array[1]);
					if (OK_TO_INTERRUPT)
						drive_non_ydb_signal_handler_if_any("generic_signal_handler2",
											sig, info, context, FALSE);
					ALTERNATE_SIGHANDLING_SAVE_SIGNUM(using_alternate_sighandling);
					SET_FORCED_EXIT_STATE(sig);
					/* Before bumping "exit_state" or sending a ERR_FORCEDHALT message to syslog/console,
					 * make sure we have not sent the same message already.
					 */
					if (non_forwarded_sig_seen[exit_state])
					{
						exit_state++;		/* Make exit pending, may still be tolerant though */
						if (is_sigterm && exit_handler_active && !ydb_quiet_halt)
							SEND_AND_PUT_MSG(VARLSTCNT(1) forced_exit_err);
					}
					DECREMENT_IN_OS_SIGNAL_HANDLER_IF_NEEDED;
					CLEANUP_AND_RETURN(got_tlevel_lock);
				}
				if (is_timer_initialized(mu_stop_tm_array[1]))
				{
					sys_get_curr_time(&mu_stop_timer);
					mu_stop_tm_array[0] = sub_abs_time(&mu_stop_timer, &mu_stop_tm_array[0]);
					/* MUPIP STOP three times within a minute acts like a kill -9 */
					if (MINUTE <= mu_stop_tm_array[0].tv_sec)
					{
						mu_stop_tm_array[0] = mu_stop_tm_array[1];
						mu_stop_tm_array[1] = mu_stop_timer;
						SET_FORCED_EXIT_STATE(sig);
						assert(!IS_GTMSECSHR_IMAGE);
						if (exit_handler_active && !ydb_quiet_halt)
							SEND_AND_PUT_MSG(VARLSTCNT(1) forced_exit_err);
						return;
					}
				}
				DEBUG_ONLY(in_nondeferrable_signal_handler = IN_GENERIC_SIGNAL_HANDLER);
				exit_state = EXIT_IMMED;
				SET_PROCESS_EXITING_TRUE; 	/* Set this BEFORE cancelling timers as wcs_phase2_commit_wait
								 * relies on this.
								 */
				if (is_sigterm && (ERR_FORCEDHALT != forced_exit_err || !ydb_quiet_halt))
					SEND_AND_PUT_MSG(VARLSTCNT(1) forced_exit_err);
			} else
			{	/* Special case for gtmsecshr - no deferral just exit */
				DEBUG_ONLY(in_nondeferrable_signal_handler = IN_GENERIC_SIGNAL_HANDLER);
				forced_exit_err = ERR_GTMSECSHRSHUTDN;
				if (OK_TO_SEND_MSG)
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) forced_exit_err);
			}
			dont_want_core = TRUE;
			assert(IS_DONT_WANT_CORE_TRUE(sig));
			break;
		case SIGQUIT:	/* Handle SIGQUIT specially which we ALWAYS want to defer if possible as it is always sent */
			dont_want_core = TRUE;
			assert(IS_DONT_WANT_CORE_TRUE(sig));
			extract_signal_info(sig, info, context, &signal_info);
			switch(signal_info.infotype)
			{
				case GTMSIGINFO_NONE:
					forced_exit_err = ERR_KILLBYSIG;
					break;
				case GTMSIGINFO_USER:
					forced_exit_err = ERR_KILLBYSIGUINFO;
					break;
				case GTMSIGINFO_ILOC + GTMSIGINFO_BADR:
					forced_exit_err = ERR_KILLBYSIGSINFO1;
					break;
				case GTMSIGINFO_ILOC:
					forced_exit_err = ERR_KILLBYSIGSINFO2;
					break;
				case GTMSIGINFO_BADR:
					forced_exit_err = ERR_KILLBYSIGSINFO3;
					break;
				default:
					exit_state = EXIT_IMMED;
					assertpro(FALSE && signal_info.infotype);	/* show signal_info if there's a failure */
			}
			/* If nothing pending AND we have crit or already in exit processing, wait to invoke shutdown */
			if (DEFER_EXIT_PROCESSING)
			{
				if (OK_TO_INTERRUPT)
					drive_non_ydb_signal_handler_if_any("generic_signal_handler3", sig, info, context, FALSE);
				ALTERNATE_SIGHANDLING_SAVE_SIGNUM(using_alternate_sighandling);
				SET_FORCED_EXIT_STATE(sig);
				/* Avoid duplicate bump of "exit_state" */
				if (non_forwarded_sig_seen[exit_state])
					exit_state++;		/* Make exit pending, may still be tolerant though */
				assert(!IS_GTMSECSHR_IMAGE);
				DECREMENT_IN_OS_SIGNAL_HANDLER_IF_NEEDED;
				CLEANUP_AND_RETURN(got_tlevel_lock);
			}
			DEBUG_ONLY(in_nondeferrable_signal_handler = IN_GENERIC_SIGNAL_HANDLER);
			exit_state = EXIT_IMMED;
			SET_PROCESS_EXITING_TRUE;
			switch(signal_info.infotype)
			{
				case GTMSIGINFO_NONE:
					SEND_AND_PUT_MSG(VARLSTCNT(6) ERR_KILLBYSIG, 4, GTMIMAGENAMETXT(image_type),
							 process_id, sig);
					break;
				case GTMSIGINFO_USER:
					SEND_AND_PUT_MSG(VARLSTCNT(8) ERR_KILLBYSIGUINFO, 6, GTMIMAGENAMETXT(image_type),
							 process_id, sig, signal_info.send_pid, signal_info.send_uid);
					break;
				case GTMSIGINFO_ILOC + GTMSIGINFO_BADR:
					SEND_AND_PUT_MSG(VARLSTCNT(8) ERR_KILLBYSIGSINFO1, 6, GTMIMAGENAMETXT(image_type),
							 process_id, sig, signal_info.int_iadr, signal_info.bad_vadr);
					break;
				case GTMSIGINFO_ILOC:
					SEND_AND_PUT_MSG(VARLSTCNT(7) ERR_KILLBYSIGSINFO2, 5, GTMIMAGENAMETXT(image_type),
							 process_id, sig, signal_info.int_iadr);
					break;
				case GTMSIGINFO_BADR:
					SEND_AND_PUT_MSG(VARLSTCNT(7) ERR_KILLBYSIGSINFO3, 5, GTMIMAGENAMETXT(image_type),
							 process_id, sig, signal_info.bad_vadr);
					break;
			}
			break;
		default:
			extract_signal_info(sig, info, context, &signal_info);
			switch(signal_info.infotype)
			{
				case GTMSIGINFO_NONE:
					DEBUG_ONLY(in_nondeferrable_signal_handler = IN_GENERIC_SIGNAL_HANDLER);
					exit_state = EXIT_IMMED;
					SET_PROCESS_EXITING_TRUE;
					SEND_AND_PUT_MSG(VARLSTCNT(6) ERR_KILLBYSIG, 4, GTMIMAGENAMETXT(image_type),
							 process_id, sig);
					break;
				case GTMSIGINFO_USER:
					/* This signal was SENT to us so it can wait until we are out of crit to cause an exit */
					forced_exit_err = ERR_KILLBYSIGUINFO;
					/* If nothing pending AND we have crit or already exiting, wait to invoke shutdown */
					if (DEFER_EXIT_PROCESSING)
					{	/* Note: since Go, using alternate signal handling, does not provide information
						 * about the signal, we cannot be here since we cannot identify a "sent" signal.
						 * Assert this.
						 */
						assert(!using_alternate_sighandling);
						assert(!IS_GTMSECSHR_IMAGE);
						ALTERNATE_SIGHANDLING_SAVE_SIGNUM(using_alternate_sighandling);
						SET_FORCED_EXIT_STATE(sig);
						if (non_forwarded_sig_seen[exit_state])
							exit_state++;	/* Make exit pending, may still be tolerant though */
						need_core = TRUE;
						MULTI_THREAD_AWARE_FORK_N_CORE(signal_forwarded);
						DECREMENT_IN_OS_SIGNAL_HANDLER_IF_NEEDED;
						CLEANUP_AND_RETURN(got_tlevel_lock);
					}
					DEBUG_ONLY(in_nondeferrable_signal_handler = IN_GENERIC_SIGNAL_HANDLER);
					exit_state = EXIT_IMMED;
					SET_PROCESS_EXITING_TRUE;
					SEND_AND_PUT_MSG(VARLSTCNT(8) ERR_KILLBYSIGUINFO, 6, GTMIMAGENAMETXT(image_type),
							 process_id, sig, signal_info.send_pid, signal_info.send_uid);
					break;
				case GTMSIGINFO_ILOC + GTMSIGINFO_BADR:
					check_for_statsdb_memerr();
					DEBUG_ONLY(in_nondeferrable_signal_handler = IN_GENERIC_SIGNAL_HANDLER);
					exit_state = EXIT_IMMED;
					SET_PROCESS_EXITING_TRUE;
					/* SIGABRT is usually delivered when memory corruption is detected by glibc
					 * e.g.  *** glibc detected *** mupip: double free or corruption (!prev): 0x0983f180 ***
					 * We want to detect that right when it happens so assert fail in that case.
					 */
					assert(SIGABRT != sig);
					SEND_AND_PUT_MSG(VARLSTCNT(8) ERR_KILLBYSIGSINFO1, 6, GTMIMAGENAMETXT(image_type),
							 process_id, sig, signal_info.int_iadr, signal_info.bad_vadr);
					break;
				case GTMSIGINFO_ILOC:
					DEBUG_ONLY(in_nondeferrable_signal_handler = IN_GENERIC_SIGNAL_HANDLER);
					exit_state = EXIT_IMMED;
					SET_PROCESS_EXITING_TRUE;
					SEND_AND_PUT_MSG(VARLSTCNT(7) ERR_KILLBYSIGSINFO2, 5, GTMIMAGENAMETXT(image_type),
							 process_id, sig, signal_info.int_iadr);
					break;
				case GTMSIGINFO_BADR:
					check_for_statsdb_memerr();
					DEBUG_ONLY(in_nondeferrable_signal_handler = IN_GENERIC_SIGNAL_HANDLER);
					exit_state = EXIT_IMMED;
					SET_PROCESS_EXITING_TRUE;
					SEND_AND_PUT_MSG(VARLSTCNT(7) ERR_KILLBYSIGSINFO3, 5, GTMIMAGENAMETXT(image_type),
							 process_id, sig, signal_info.bad_vadr);
					break;
				default:
					DEBUG_ONLY(in_nondeferrable_signal_handler = IN_GENERIC_SIGNAL_HANDLER);
					exit_state = EXIT_IMMED;
					SET_PROCESS_EXITING_TRUE;
					assertpro(FALSE && signal_info.infotype);;	/* show signal_info if there's a failure */
			}
			if (0 != signal_info.sig_err)
			{
				SEND_AND_PUT_MSG(VARLSTCNT(1) signal_info.sig_err);
			}
			break;
	} /* switch (sig) */
	/* Stop the timers but do not cancel them. This allows the timer structures to appear in the core where gtmpcat can
	 * extract them allowing us to see what was going on.
	 */
	assert(in_nondeferrable_signal_handler);
	FFLUSH(stdout);
	if (!dont_want_core)
	{	/* Generate core (if we want one). We want to do this before we go through the rest of handling this signal
		 * which would potentially modify information we want to see in the core.
		 */
		/* Block SIGALRM signal before dumping core. Note that we do exactly the same thing inside "gtm_fork_n_core()"
		 * but there is a small window of code between here and there and since we are anyways going to dump a core
		 * at this point, better be safe (by avoiding timer interrupts here that can cause potential issues)
		 * than worry about unnecessary duplication of the SIGPROCMASK calls.
		 */
		SIGPROCMASK(SIG_BLOCK, &blockalrm, &savemask, rc);
		need_core = TRUE;
		MULTI_THREAD_AWARE_FORK_N_CORE(signal_forwarded);
	}
	if (IS_GTMSECSHR_IMAGE)
	{
		DEBUG_ONLY(in_nondeferrable_signal_handler = save_in_nondeferrable_signal_handler);
		DECREMENT_IN_OS_SIGNAL_HANDLER_IF_NEEDED;
		CLEANUP_AND_RETURN(got_tlevel_lock);
	}
	signal_exit_handler("generic_signal_handler", sig, info, context, IS_DEFERRED_EXIT_FALSE);	/* exits the process */
	/* Below code is unreachable since the previous step would exit the process but is there for completeness */
	assert(FALSE);
	DECREMENT_IN_OS_SIGNAL_HANDLER_IF_NEEDED;
	CLEANUP_AND_RETURN(got_tlevel_lock);
}
