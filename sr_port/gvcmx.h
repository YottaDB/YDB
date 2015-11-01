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

#ifndef GVCMX_INCLUDED
#define GVCMX_INCLUDED

int gvcmx_data(void);
int gvcmx_get(mval *v);
int gvcmx_order(void);
int gvcmx_query(void);
int gvcmx_reqremlk(unsigned char c, int4 timeout);
int gvcmx_resremlk(unsigned char c);
int gvcmx_zprevious(void);
void gvcmx_canremlk(void);
void gvcmx_kill(bool so_subtree);
void gvcmx_put(mval *v);
void gvcmx_susremlk(unsigned char rmv_locks);
void gvcmx_unlock(unsigned char rmv_locks, bool specific, char incr);

#endif /* GVCMX_INCLUDED */
