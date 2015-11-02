/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __TP_UNWIND_H__
#define __TP_UNWIND_H__

enum tp_unwind_invocation
{
	COMMIT_INVOCATION = 1,
	ROLLBACK_INVOCATION,
	RESTART_INVOCATION
};

void	tp_unwind(short newlevel, enum tp_unwind_invocation);

#endif
