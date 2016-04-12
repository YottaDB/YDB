/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GVCMX_INCLUDED
#define GVCMX_INCLUDED

mint		gvcmx_data(void);
bool		gvcmx_get(mval *v);
bool		gvcmx_order(void);
bool		gvcmx_query(mval *val);
bool		gvcmx_reqremlk(unsigned char laflag, int4 time);
bool		gvcmx_resremlk(unsigned char c);
bool		gvcmx_zprevious(void);
void		gvcmx_canremlk(void);
void		gvcmx_kill(bool so_subtree);
void		gvcmx_put(mval *v);
void		gvcmx_increment(mval *increment, mval *result);
void		gvcmx_susremlk(unsigned char rmv_locks);
void		gvcmx_unlock(unsigned char rmv_locks, bool specific, char incr);

#endif /* GVCMX_INCLUDED */
