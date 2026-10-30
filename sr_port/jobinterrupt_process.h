/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef JOBINTR_PROCESS_INCLUDED
#define JOBINTR_PROCESS_INCLUDED

/* At the end of an outermost transaction, $ZINTERRUPT needs to run if $ZTEXIT is true, $ZINTERRUPT is not already
 * running, and the process is not exiting. For a TCOMMIT or TROLLBACK command, issue JOBINTRRETHROW.
 *
 * For an implicit transaction (IMPLICIT_TP), i.e. one that the runtime started on its own for a non-TP update or read
 * (e.g. "gvcst_put", "gvcst_kill", "op_ztrigger", "$ZTRIGGER()"), issuing JOBINTRRETHROW does not work. The M frame's
 * mpc does not point past the M command that did the update (that command did not go through an "opp_*" routine that
 * would save it), so after $ZINTERRUPT, that command would run again, end another implicit transaction and issue
 * JOBINTRRETHROW again, without end. Instead, set up the job interrupt event with "xfer_set_handlers()", as the signal
 * handler does when a job interrupt signal arrives, so $ZINTERRUPT runs once, at the next point where the M code can be
 * interrupted.
 *
 * The second parameter of "xfer_set_handlers()" keeps $ZYINTRSIG inside $ZINTERRUPT as it would be after the
 * JOBINTRRETHROW of a TCOMMIT or TROLLBACK command, i.e. the signal of the last job interrupt the process received
 * (see "op_svget()").
 *
 * A caller needs "gtm_signal.h", "deferred_events_queue.h" and "deferred_events.h".
 */
#define JOBINTR_TP_RETHROW(IMPLICIT_TP)									\
{													\
	GBLREF  boolean_t		dollar_ztexit_bool;						\
	GBLREF  int			jobinterrupt_sig_num;						\
	GBLREF  int			process_exiting;						\
	GBLREF  volatile boolean_t	dollar_zininterrupt;						\
													\
	error_def(ERR_JOBINTRRETHROW);				/* BYPASSOK */				\
													\
	if (dollar_ztexit_bool && !dollar_zininterrupt && !process_exiting)				\
	{												\
		if (IMPLICIT_TP)									\
			(void)xfer_set_handlers(jobinterrupt,						\
				((SIGUSR1 == jobinterrupt_sig_num) ? SIGUSR1 : SIGUSR2), FALSE);	\
		else											\
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_JOBINTRRETHROW);			\
	}												\
}
void jobinterrupt_event(int sig, siginfo_t *info, void *context);
void jobinterrupt_init(void);
void jobintrpt_ztime_process(int4 intrpt_type);	/* jobinterrupt, ztimeout or sigwinch (see enum outofbands) */

#endif
