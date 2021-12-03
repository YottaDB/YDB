/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

<<<<<<< HEAD
#ifndef OUTOFBAND_H_INCLUDED
#define OUTOFBAND_H_INCLUDED

/*
 *  Should change bool to boolean_t together on Unix and VMS
 */

#define OUTOFBAND_MSK	0x02000018
#define CTRLC_MSK	0x00000008
#define SIGHUP_MSK	0x00000010
#define CTRLY_MSK	0x02000000
#define CTRLC     3
#define CTRLD     4
#define CTRLY	  25
#define MAXOUTOFBAND 31
enum outofbands
{
	ctrly = 1,
	ctrlc,
	ctrap,
	tptimeout,
	jobinterrupt,
	ztimeout,
	sighup,
	deferred_signal,
};

#define OUTOFBAND_RESTARTABLE(event)	(jobinterrupt == (event))

/* Sets "outofband" global variable to an unusual event (e.g. Ctrl-C/SIGTERM etc.) and switches a few transfer table
 * entries so we handle that unusual event as soon as a logical point is reached.
 */
#define	SET_OUTOFBAND(EVENT)					\
{								\
	outofband = EVENT;					\
	FIX_XFER_ENTRY(xf_linefetch, op_fetchintrrpt);		\
	FIX_XFER_ENTRY(xf_linestart, op_startintrrpt);		\
	FIX_XFER_ENTRY(xf_zbfetch, op_fetchintrrpt);		\
	FIX_XFER_ENTRY(xf_zbstart, op_startintrrpt);		\
	FIX_XFER_ENTRY(xf_forchk1, op_startintrrpt);		\
	FIX_XFER_ENTRY(xf_forloop, op_forintrrpt);		\
}

void outofband_action(boolean_t line_fetch_or_start);

void outofband_clear(void);

#endif /* OUTOFBAND_H_INCLUDED */
=======
D_EVENT(no_event, ctrlc_set),			/* 0; ctrlc_set is just a place holder here */
D_EVENT(ctrlc, ctrlc_set),			/* 1 */
D_EVENT(ctrap, ctrap_set),			/* 2 */
D_EVENT(jobinterrupt, jobinterrupt_set),	/* 3 */
D_EVENT(tptimeout, tptimeout_set),		/* 4 */
D_EVENT(ztimeout, ztimeout_set),		/* 5 */
D_EVENT(sighup, ctrap_set),			/* 6 */
D_EVENT(neterr_action, gvcmz_neterr),		/* 7 - from here rest of the list are deferred but not outofband */
D_EVENT(zstep_pending, op_zstep),		/* 8 */
D_EVENT(zbreak_pending, op_setzbrk),		/* 9 */
D_EVENT(ttwriterr, tt_write_error_set),		/* 10 */
D_EVENT(DEFERRED_EVENTS, ctrlc_set)		/* count; ctrlc_set is just a place holder here */
>>>>>>> 52a92dfd (GT.M V7.0-001)
