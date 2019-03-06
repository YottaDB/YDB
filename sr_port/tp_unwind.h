/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TP_UNWIND_H_INCLUDED
#define TP_UNWIND_H_INCLUDED

enum tp_unwind_invocation
{
	COMMIT_INVOCATION = 1,
	ROLLBACK_INVOCATION,
	RESTART_INVOCATION
};

void tp_unwind(uint4 newlevel, enum tp_unwind_invocation, int *tprestart_rc);
int tp_unwind_restlv(lv_val *curr_lv, lv_val *save_lv, tp_var *restore_ent, boolean_t clearTStartCycle, int *tprestart_lc);

#endif
