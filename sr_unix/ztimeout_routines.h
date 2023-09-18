/****************************************************************
 *								*
 * Copyright (c) 2018-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include <mdefsp.h>

typedef struct
{
	mval ztimeout_vector;
	mval ztimeout_seconds;
	ABS_TIME end_time;
} dollar_ztimeout_struct;


void check_and_set_ztimeout(mval *inp_val);
void ztimeout_action(void);
void ztimeout_expired(void);
void ztimeout_clear_timer(void);
int get_ztimeout(mval *result);

/* TODO: should this go back in (say) op_tcommit? */
#define CALL_ZTIMEOUT_IF_DEFERRED								\
MBSTART	{											\
	GBLREF	boolean_t		ztrap_explicit_null;					\
	GBLREF	dollar_ecode_type	dollar_ecode;						\
	GBLREF	volatile boolean_t	dollar_zininterrupt;					\
												\
	int4		event_type, param_val;							\
												\
	if (ztimeout == (TREF(save_xfer_root_ptr))->ev_que.fl->outofband)			\
	{	/*If ztimeout , check conditions before popping out */				\
		if (!dollar_zininterrupt && ((0 == dollar_ecode.index) || !(ETRAP_IN_EFFECT)) 	\
			&& (!have_crit(CRIT_HAVE_ANY_REG | CRIT_IN_COMMIT)))			\
		{										\
			POP_XFER_QUEUE_ENTRY(&event_type, &param_val);	 			\
			xfer_set_handlers(event_type, param_val, TRUE);				\
		}										\
	}											\
} MBEND

/* Need to use this macro instead of "op_commarg()" whenever a $ztimeout vector is about to be compiled.
 * This is because that memory is in the heap (see "malloc()" call in "check_and_set_ztimeout()") and can
 * cause "heap-use-after-free" ASAN error.
 */
#define	OP_COMMARG_S2POOL(ZTIMEOUT_VECTOR)				\
{									\
	mval	mv_copy;						\
									\
	mv_copy = *ZTIMEOUT_VECTOR;					\
	s2pool(&mv_copy.str);						\
	op_commarg(&mv_copy, indir_linetail);				\
}

