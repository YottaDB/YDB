/****************************************************************
 *								*
 *	Copyright 2002, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef STR2GVARGS_H_INCLUDED
#define STR2GVARGS_H_INCLUDED

typedef struct
{
#ifndef __osf__
	ssize_t	count;
#else
	int4	count;
#endif
	mval	*args[MAX_GVSUBSCRIPTS + 1];
} gvargs_t;

boolean_t str2gvargs(char *cp, int len, gvargs_t *gvargs);

#endif /* STR2GVARGS_H_INCLUDED */
