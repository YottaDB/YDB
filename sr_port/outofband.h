/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

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
