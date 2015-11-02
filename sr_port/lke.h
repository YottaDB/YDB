/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __LKE_H__
#define __LKE_H__

#include "error.h"

bool lke_get_answ(char *prompt);
bool lke_showlock(struct CLB *lnk, mlk_shrblk_ptr_t tree, mstr *name, bool all, bool wait,
	bool interactive, int4 pid, mstr one_lock, boolean_t exact);
bool lke_showtree(struct CLB *lnk, mlk_shrblk_ptr_t tree, bool all, bool wait, pid_t pid,
		  mstr one_lock, bool memory, int *shr_sub_size);
void lke_exit(void);
void lke_clear(void);
void lke_help(void);
void lke_show(void);
void lke_show_memory(mlk_shrblk_ptr_t bhead, char *prefix);

#ifdef VMS
void lke(void);
#else
CONDITION_HANDLER(lke_ctrlc_handler);
#endif
void lke_setgdr(void);

#endif /* __LKE_H__ */
