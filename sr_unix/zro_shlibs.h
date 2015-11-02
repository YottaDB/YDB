/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ZRO_SHLIBS_INCLUDED
#define ZRO_SHLIBS_INCLUDED

typedef struct open_shlib_struct
{
	struct open_shlib_struct *next;
	void		*shlib_handle;
	char		shlib_name[MAX_FBUFF + 1];
} open_shlib;


void *zro_shlibs_find(char *shlib_name);
void zro_shlibs_unlink_all(void);

#endif
