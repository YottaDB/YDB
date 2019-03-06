/****************************************************************
 *								*
 * Copyright (c) 2018-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
	mval 	ztimeout_seconds;
	ABS_TIME end_time;
} dollar_ztimeout_struct;


void check_and_set_ztimeout(mval *inp_val);
void ztimeout_action(void);
void ztimeout_set(int4 dummy_param);
void ztimeout_expire_now(void);
void ztimeout_process(void);
void ztimeout_clear_timer(void);
int get_ztimeout(mval *result);



#define CALL_ZTIMEOUT_IF_DEFERRED									\
MBSTART	{												\
	GBLREF	dollar_ecode_type	dollar_ecode;							\
	GBLREF	boolean_t		ztrap_explicit_null;						\
	GBLREF	boolean_t		dollar_zininterrupt;						\
	if ((TREF(save_xfer_root)))									\
	{												\
		if (((TREF(save_xfer_root))->set_fn == ztimeout_set))					\
		{	/*If ztimeout , check conditions before popping out */				\
			if ((TREF(ztimeout_deferred))	&&						\
				((0 == dollar_ecode.index) || !(ETRAP_IN_EFFECT)) 			\
				&& (!dollar_zininterrupt) && (!have_crit(CRIT_HAVE_ANY_REG 		\
										| CRIT_IN_COMMIT)))	\
			{										\
				POP_XFER_ENTRY(&event_type, &set_fn, &param_val);	 		\
				xfer_set_handlers(event_type, set_fn, param_val, TRUE);			\
			}										\
		}											\
	}												\
} MBEND
