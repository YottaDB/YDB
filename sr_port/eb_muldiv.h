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

#ifndef __EB_MULDIV_H__
#define __EB_MULDIV_H__

bool	eb_int_mul (int4 v1, int4 u1, int4 p[]);
int4	eb_mul (int4 v[], int4 u[], int4 p[]);	/* p = u*v */
bool	eb_mvint_div (int4 v, int4 u, int4 q[]);
bool	eb_int_div (int4 v1, int4 u1, int4 q[]);
int4	eb_div (int4 x[], int4 y[], int4 q[]);	/* q = y/x */
void	eb_int_mod (int4 v1, int4 u1, int4 p[]);

#endif
