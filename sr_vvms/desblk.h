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

typedef struct {
	int4	link;
	int4	*exit_hand;
	int4	arg_cnt;
	int4	*cond_val;
} desblk;

#define	SET_EXIT_HANDLER(exi_blk, exit_handler, exit_condition)		\
{									\
	exi_blk.exit_hand = &exit_handler;				\
	exi_blk.arg_cnt = 1;						\
	exi_blk.cond_val = &exit_condition;				\
	EXIT_HANDLER(&exi_blk);						\
}
