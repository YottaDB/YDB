/****************************************************************
 *                                                              *
 * Copyright (c) 2007-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#ifndef FIX_XFER_ENTRY_INCLUDED
#define FIX_XFER_ENTRY_INCLUDED

GBLREF xfer_entry_t     xfer_table[];

#if defined(__x86_64__)
#include "xfer_desc.i"
#endif

/* macro to change a transfer table entry */
#define FIX_XFER_ENTRY(indx, func) 				\
MBSTART { 							\
	xfer_table[indx] = (xfer_entry_t)&func; 		\
} MBEND

/* macro to insert interrupt vectors in the transfer table at appropriate places */
#define DEFER_INTO_XFER_TAB					\
MBSTART {							\
	FIX_XFER_ENTRY(xf_linefetch, op_fetchintrrpt);		\
	FIX_XFER_ENTRY(xf_linestart, op_startintrrpt);		\
	FIX_XFER_ENTRY(xf_zbfetch, op_fetchintrrpt);		\
	FIX_XFER_ENTRY(xf_zbstart, op_startintrrpt);		\
	FIX_XFER_ENTRY(xf_forchk1, op_startintrrpt);		\
	FIX_XFER_ENTRY(xf_forloop, op_forintrrpt);		\
} MBEND

/* macro to return *some* vectors to "normal" from interrupting adjustments; ret and retarg fall out side this;
 * for adjustments are unnecessary in some cases, but benign
 */
#define DEFER_OUT_OF_XFER_TAB(IS_TRACING)				\
MBSTART {								\
	if (IS_TRACING)							\
	{	/* M-profiling in effect */				\
		FIX_XFER_ENTRY(xf_linefetch, op_mproflinefetch);	\
		FIX_XFER_ENTRY(xf_linestart, op_mproflinestart);	\
		FIX_XFER_ENTRY(xf_forchk1, op_mprofforchk1);		\
	} else								\
	{								\
		FIX_XFER_ENTRY(xf_linefetch, op_linefetch);		\
		FIX_XFER_ENTRY(xf_linestart, op_linestart);		\
		FIX_XFER_ENTRY(xf_forchk1, op_forchk1);			\
	}								\
	FIX_XFER_ENTRY(xf_zbfetch, op_zbfetch);				\
	FIX_XFER_ENTRY(xf_zbstart, op_zbstart);				\
} MBEND

#endif /* FIX_XFER_ENTRY_INCLUDED */
