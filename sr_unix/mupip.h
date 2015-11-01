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

typedef struct
{	bool	present;
	union
	{	char	*str;
		int	num;
	}val;
}mu_com_arg;

typedef struct mu_com_struct
{
mu_com_arg	a;	/* after */
mu_com_arg	A;	/* Adjacency */
mu_com_arg	b;	/* block, block_density, begin, before */
bool		B;	/* Backward */
bool		c;	/* comprehensive */
bool		C;	/* Full (Complete) */
mu_com_arg	e;	/* error_limit, end */
mu_com_arg	E;	/* Extract */
mu_com_arg	f;	/* file */
bool		F;	/* Fast, Freeze, Forward */
mu_com_arg	g;	/* global */
mu_com_arg	G;	/* Global_buffers */
bool		i;	/* incremental, interactive */
mu_com_arg	I;	/* ID */
mu_com_arg	j;	/* journal */
bool		k;	/* keyranges */
mu_com_arg	l;	/* log */
mu_com_arg	L;	/* Lookback limit (0 to negate) */
mu_com_arg	m;	/* map	(0 to negate) */
mu_com_arg	M;	/* Maxkeysize */
mu_com_arg	n;	/* name */
bool		N;	/* Nochecktn */
mu_com_arg	p;	/* process */
mu_com_arg	r;	/* region */
bool		R;	/* Record, Recover */
mu_com_arg	s;	/* show */
mu_com_arg	S;	/* Subscript, Since, Select */
mu_com_arg	t;	/* transaction */
bool		T;	/* Tn_reset */
mu_com_arg	u;	/* user */
bool		v;	/* verify */
}mu_com;
