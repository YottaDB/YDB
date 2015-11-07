/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef DDP_TRACE_OUTPUT_INCLUDED
#define DDP_TRACE_OUTPUT_INCLUDED

#define DDP_TRACE_ENV	"GTMDDP$TRACE"

enum
{
	DDP_SEND = 0,
	DDP_RECV = 1
};

int ddp_trace_output(unsigned char *cp, int len, int code);

#endif /* DDP_TRACE_OUTPUT_INCLUDED */
