/****************************************************************
 *								*
 * Copyright (c) 2010-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef FULLBOOL_H_INCLUDED
#define FULLBOOL_H_INCLUDED

enum gtm_bool_type
{
	GTM_BOOL = 0,		/* original GT.M short-circuit Boolean evaluation with naked maintenance */
	FULL_BOOL,		/* standard behavior - evaluate everything with a side effect */
	FULL_BOOL_WARN		/* like FULL_BOOL but give compiler warnings when it makes a difference */
};

enum gtm_se_type
{
	OLD_SE = 0,		/* ignore side effect implications */
	STD_SE,			/* reorder argument processing for left-to-right side effects */
	SE_WARN			/* like STD but give compiler warnings when it makes a difference */
};

#define TRACK_JMP_TARGET(T, REF0)												\
MBSTART {	/* T is triple to tag; REF0 is the new target triple with which it's tagged */					\
	tripbp = &T->jmplist;						/* borrow jmplist to track jmp targets */		\
	assert(NULL == tripbp->bpt);												\
	assert((tripbp == tripbp->que.fl) && (tripbp == tripbp->que.bl));							\
	tripbp->bpt = REF0;						/* point to the new location */				\
	dqins(TREF(bool_targ_ptr), que, tripbp);			/* queue jmplist for clean-up */			\
} MBEND

#endif /* FULLBOOL_H_INCLUDED */
