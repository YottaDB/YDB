/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
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
	int4	count;		/* caveat: this should be the same size as a pointer */
	mval	*args[MAX_GVSUBSCRIPTS + 1];
} gvargs_t;

boolean_t str2gvargs(char *cp, int len, gvargs_t *gvargs);

#endif /* STR2GVARGS_H_INCLUDED */
