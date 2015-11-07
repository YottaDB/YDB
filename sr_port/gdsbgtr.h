/****************************************************************
 *								*
 *	Copyright 2005, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Define macros to keep statistics of certain parts of code we pass
 * through, some only if we are in debug mode.
 *
 * Although the incremented counters are generally in shared storage,
 * we will not do interlock adds to them because even though there
 * may be some interference, most will succeed and we are only looking
 * for trends from these numbers anyway, not exact counts.
 */


#define BG_TRACE_PRO_ANY(C, X)	{C->hdr->X##_cntr++; C->hdr->X##_tn = C->hdr->trans_hist.curr_tn ;}
#define BG_TRACE_PRO(Q) 	BG_TRACE_PRO_ANY(cs_addrs, Q)

#ifdef DEBUG
#define BG_TRACE_ANY(C, X)	BG_TRACE_PRO_ANY(C, X)
#define BG_TRACE(Q)		BG_TRACE_ANY(cs_addrs, Q)
#else
#define BG_TRACE_ANY(C, X)
#define BG_TRACE(Q)
#endif
