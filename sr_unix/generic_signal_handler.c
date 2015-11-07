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

/* Standard signal processor
 *
 * If we are nesting our handlers in an improper way, this routine will
 * not return but will immediately invoke core/termination processing.
 *
 * Returns if some condition makes it inadvisable to exit now else invokes the system exit() system call.
 * For GTMSECSHR it unconditionally returns to gtmsecshr_signal_handler() which later invokes gtmsecshr_exit().
 */

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"		/* for exit() */
#include "gtm_inet.h"

#include <signal.h>
#ifdef GTM_PTHREAD
#  include <pthread.h>
#endif

#include "gtm_stdio.h"
#include "error.h"
#include "gtmsiginfo.h"
#include "gtmimagename.h"
#include "gt_timer.h"
#include "send_msg.h"
#include "generic_signal_handler.h"
#include "gtmmsg.h"
#include "gtmio.h"
#include "have_crit.h"
#include "util.h"

#define	DEFER_EXIT_PROCESSING	((EXIT_PENDING_TOLERANT >= exit_state) &&	\
	(exit_handler_active || !OK_TO_INTERRUPT))

/* Combine send_msg and gtm_putmsg into one macro to conserve space. */
#define SEND_AND_PUT_MSG(...)					\
{								\
	if (OK_TO_SEND_MSG)					\
		send_msg_csa(CSA_ARG(NULL) __VA_ARGS__);	\
	gtm_putmsg_csa(CSA_ARG(NULL) __VA_ARGS__);		\
}

/* These fields are defined as globals not because they are used globally but
 * so they will be easily retrievable even in 'pro' cores.
 */
GBLDEF siginfo_t		exi_siginfo;

GBLDEF gtm_sigcontext_t 	exi_context;

GBLREF	int4			forced_exit_err;
GBLREF	int4			exi_condition;
GBLREF	boolean_t		dont_want_core;
GBLREF	boolean_t		created_core;
GBLREF	boolean_t		need_core;
GBLREF	uint4			process_id;
GBLREF	volatile int4		exit_state;
GBLREF	volatile unsigned int	core_in_progress;
GBLREF	gtmsiginfo_t		signal_info;
GBLREF	boolean_t		exit_handler_active;
GBLREF	void			(*call_on_signal)();
GBLREF	boolean_t		gtm_quiet_halt;
GBLREF	volatile int4           gtmMallocDepth;         /* Recursion indicator */

LITREF	gtmImageName		gtmImageNames[];

error_def(ERR_FORCEDHALT);
error_def(ERR_GTMSECSHRSHUTDN);
error_def(ERR_KILLBYSIG);
error_def(ERR_KILLBYSIGSINFO1);
error_def(ERR_KILLBYSIGSINFO2);
error_def(ERR_KILLBYSIGSINFO3);
error_def(ERR_KILLBYSIGUINFO);
error_def(ERR_KRNLKILL);

void generic_signal_handler(int sig, siginfo_t *info, void *context)
{
	gtm_sigcontext_t	*context_ptr;
	void			(*signal_routine)();
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig);
	/* Save parameter value in global variables for easy access in core */
	dont_want_core = FALSE;		/* (re)set in case we recurse */
	created_core = FALSE;		/* we can deal with a second core if needbe */
	exi_condition = sig;
	if (NULL != info)
		exi_siginfo = *info;
	else
		memset(&exi_siginfo, 0, SIZEOF(*info));
#	if defined(__ia64) && defined(__hpux)
	context_ptr = (gtm_sigcontext_t *)context;	/* no way to make a copy of the context */
	memset(&exi_context, 0, SIZEOF(exi_context));
#	else
	if (NULL != context)
		exi_context = *(gtm_sigcontext_t *)context;
	else
		memset(&exi_context, 0, SIZEOF(exi_context));
	context_ptr = &exi_context;
#	endif
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
				if (core_in_progress)
				{
					if (exit_handler_active)
						_exit(sig);
					else
						exit(sig);
				}
				++core_in_progress;
				DUMP_CORE;
				GTMASSERT;
			default:
				;
		}
	}
	switch(sig)
	{
		case SIGTERM:
			if (!IS_GTMSECSHR_IMAGE)
			{
				forced_exit_err = ERR_FORCEDHALT;
				/* If nothing pending AND we have crit or in wcs_wtstart() or already in exit processing, wait to
				 * invoke shutdown. wcs_wtstart() manipulates the active queue that a concurrent process in crit
				 * in bt_put() might be waiting for. interrupting it can cause deadlocks (see C9C11-002178).
				 */
				if (DEFER_EXIT_PROCESSING)
				{
					SET_FORCED_EXIT_STATE;
					exit_state++;		/* Make exit pending, may still be tolerant though */
					assert(!IS_GTMSECSHR_IMAGE);
					if (exit_handler_active && !gtm_quiet_halt)
						SEND_AND_PUT_MSG(VARLSTCNT(1) forced_exit_err);
					return;
				}
				exit_state = EXIT_IMMED;
				SET_PROCESS_EXITING_TRUE; 	/* Set this BEFORE cancelling timers as wcs_phase2_commit_wait
								 * relies on this.
								 */
				if (ERR_FORCEDHALT != forced_exit_err || !gtm_quiet_halt)
					SEND_AND_PUT_MSG(VARLSTCNT(1) forced_exit_err);
			} else
			{	/* Special case for gtmsecshr - no deferral just exit */
				forced_exit_err = ERR_GTMSECSHRSHUTDN;
				if (OK_TO_SEND_MSG)
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) forced_exit_err);
			}
			dont_want_core = TRUE;
			break;
		case SIGQUIT:	/* Handle SIGQUIT specially which we ALWAYS want to defer if possible as it is always sent */
			dont_want_core = TRUE;
			extract_signal_info(sig, &exi_siginfo, context_ptr, &signal_info);
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
					GTMASSERT;
			}
			/* If nothing pending AND we have crit or already in exit processing, wait to invoke shutdown */
			if (DEFER_EXIT_PROCESSING)
			{
				SET_FORCED_EXIT_STATE;
				exit_state++;		/* Make exit pending, may still be tolerant though */
				assert(!IS_GTMSECSHR_IMAGE);
				return;
			}
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
#		ifdef _AIX
		case SIGDANGER:
			forced_exit_err = ERR_KRNLKILL;
			/* If nothing pending AND we have crit or already in exit processing, wait to invoke shutdown */
			if (DEFER_EXIT_PROCESSING)
			{
				SET_FORCED_EXIT_STATE;
				exit_state++;		/* Make exit pending, may still be tolerant though */
				assert(!IS_GTMSECSHR_IMAGE);
				return;
			}
			exit_state = EXIT_IMMED;
			SET_PROCESS_EXITING_TRUE;
			SEND_AND_PUT_MSG(VARLSTCNT(1) forced_exit_err);
			dont_want_core = TRUE;
			break;
#		endif
		default:
			extract_signal_info(sig, &exi_siginfo, context_ptr, &signal_info);
			switch(signal_info.infotype)
			{
				case GTMSIGINFO_NONE:
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
					{
						assert(!IS_GTMSECSHR_IMAGE);
						SET_FORCED_EXIT_STATE;
						exit_state++;		/* Make exit pending, may still be tolerant though */
						need_core = TRUE;
						gtm_fork_n_core();	/* Generate "virgin" core while we can */
						return;
					}
					exit_state = EXIT_IMMED;
					SET_PROCESS_EXITING_TRUE;
					SEND_AND_PUT_MSG(VARLSTCNT(8) ERR_KILLBYSIGUINFO, 6, GTMIMAGENAMETXT(image_type),
						process_id, sig, signal_info.send_pid, signal_info.send_uid);
					break;
				case GTMSIGINFO_ILOC + GTMSIGINFO_BADR:
					exit_state = EXIT_IMMED;
					SET_PROCESS_EXITING_TRUE;
					SEND_AND_PUT_MSG(VARLSTCNT(8) ERR_KILLBYSIGSINFO1, 6, GTMIMAGENAMETXT(image_type),
						process_id, sig, signal_info.int_iadr, signal_info.bad_vadr);
					break;
				case GTMSIGINFO_ILOC:
					exit_state = EXIT_IMMED;
					SET_PROCESS_EXITING_TRUE;
					SEND_AND_PUT_MSG(VARLSTCNT(7) ERR_KILLBYSIGSINFO2, 5, GTMIMAGENAMETXT(image_type),
						process_id, sig, signal_info.int_iadr);
					break;
				case GTMSIGINFO_BADR:
					exit_state = EXIT_IMMED;
					SET_PROCESS_EXITING_TRUE;
					SEND_AND_PUT_MSG(VARLSTCNT(7) ERR_KILLBYSIGSINFO3, 5, GTMIMAGENAMETXT(image_type),
						process_id, sig, signal_info.bad_vadr);
					break;
				default:
					exit_state = EXIT_IMMED;
					SET_PROCESS_EXITING_TRUE;
					GTMASSERT;
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
	sys_canc_timer();
	FFLUSH(stdout);
	if (!dont_want_core)
	{
		need_core = TRUE;
		gtm_fork_n_core();
	}
	/* If any special routines are registered to be driven on a signal, drive them now */
	if (0 != exi_condition)
	{
		/* If this is a GTM-ish process in runtime mode, call the routine to generate the zshow dump. This separate
		 * routine necessary because (1) generic_signal_handler() no longer calls condition handlers and (2) we couldn't
		 * use call_on_signal because it would already be in use in updproc() where it is also possible this routine
		 * needs to be called. Bypass this code if we didn't create a core since that means it is not a GTM
		 * issue that forced its demise (and since this is an uncaring interrupt, we could be in any number of
		 * situations that would cause a ZSHOW dump to explode). Better for user to use jobexam to cause a dump prior
		 * to terminating the process in a deferrable fashion.
		 */
		if (!dont_want_core && IS_MCODE_RUNNING && (NULL != (signal_routine = RFPTR(create_fatal_error_zshow_dmp_fptr))))
		{	/* note assignment of signal_routine above */
			SFPTR(create_fatal_error_zshow_dmp_fptr, NULL);
			(*signal_routine)(exi_condition, FALSE);
		}
		/* Some mupip functions define an entry point to drive on signals. Make sure to do this AFTER we create the
		 * dump file above as it may detach things (like the recvpool) we need to create the above dump.
		 */
		if (NULL != (signal_routine = call_on_signal))	/* Note assignment */
		{
			call_on_signal = NULL;		/* So we don't recursively call ourselves */
			(*signal_routine)();
		}
	}
	if (!IS_GTMSECSHR_IMAGE)
	{
		assert((EXIT_IMMED <= exit_state) || !exit_handler_active);
		exit(-exi_condition);
	} else
		return;
}
