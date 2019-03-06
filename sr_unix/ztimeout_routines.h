/****************************************************************
 *								*
 * Copyright (c) 2018 Fidelity National Information		*
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
void get_ztimeout(mval *result);
