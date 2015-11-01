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

#ifndef GVUSR_INCLUDED
#define GVUSR_INCLUDED

int gvusr_data(void);
int gvusr_get(mval *v);
int gvusr_lock(uint4 lock_len, unsigned char *lock_key, gd_region *reg);
int gvusr_order(void);
int gvusr_query(mval *v);
int gvusr_zprevious(void);
void gvusr_init(gd_region *reg, gd_region **creg, gv_key **ckey, gv_key **akey);
void gvusr_kill(bool do_subtree);
void gvusr_put(mval *v);
void gvusr_rundown(void);
void gvusr_unlock(uint4 lock_len, unsigned char *lock_key, gd_region *reg);

#endif /* GVUSR_INCLUDED */
