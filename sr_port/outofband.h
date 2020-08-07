/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef OUTOFBAND_H_INCLUDED
#define OUTOFBAND_H_INCLUDED

/*
 *  Should change bool to boolean_t together on Unix and VMS
 */

#define	OUTOFBAND_MSK 0x02000008
#define CTRLC_MSK 0x00000008
#define CTRLY_MSK 0x02000000
#define CTRLC     3
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
	deferred_signal
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
