/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef LKE_H_INCLUDED
#define LKE_H_INCLUDED

#include "error.h"

bool lke_get_answ(char *prompt);
bool lke_showlock(mlk_pvtctl_ptr_t pctl, struct CLB *lnk, mlk_shrblk_ptr_t tree, mstr *name, bool all, bool wait,
	bool interactive, int4 pid, mstr one_lock, boolean_t exact, intrpt_state_t *prev_intrpt_state);
void lke_formatlockname(mlk_shrblk_ptr_t node, mstr* name);
bool lke_showtree(mlk_pvtctl_ptr_t pctl, struct CLB *lnk, bool all, bool wait, pid_t pid,
		  mstr one_lock, bool memory, int *shr_sub_size, intrpt_state_t *prev_intrpt_state);
void lke_exit(void);
void lke_clean(void);
void lke_clear(void);
void lke_growhash(void);
void lke_help(void);
void lke_rehash(void);
void lke_show(void);
void lke_show_memory(mlk_pvtctl_ptr_t pctl, mlk_shrblk_ptr_t bhead, char *prefix, intrpt_state_t *prev_intrpt_state);
void lke_show_hashtable(mlk_pvtctl_ptr_t pctl, intrpt_state_t *prev_intrpt_state);

CONDITION_HANDLER(lke_ctrlc_handler);

#endif /* __LKE_H__ */
