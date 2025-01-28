/****************************************************************
 *								*
 * Copyright (c) 2018-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023-2025 YottaDB LLC and/or its subsidiaries.	*
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

