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

#ifndef __ZBREAK_H__
#define __ZBREAK_H__

#include "zbreaksp.h"
#include "cache.h"

typedef struct
{
	char		*mpc;		/* MUMPS address for ZBREAK */
	mident		rtn;
	mident		lab;
	int		offset;
	int		count;		/* # of time ZBREAK encountered */
	cache_entry	*action;	/* action associated with ZBREAK (indirect cache entry) */
	zb_code 	m_opcode;	/* MUMPS op_code replaced */
}zbrk_struct;

typedef struct
{
	char	*beg;
	char	*free;
	char	*end;
	short	rec_size;
}z_records;

#ifndef ZB_CODE_SHIFT
#define ZB_CODE_SHIFT 0
#endif

#define INIT_NUM_ZBREAKS 1
#define CANCEL_ONE -1
#define CANCEL_ALL -2


char *zr_find(z_records *z, char *addr);
char *zr_get_free(z_records *z_ptr, char *addr);
char *zr_get_last(z_records *z);
void zr_init(z_records *z, int4 count, int4 rec_size);
void zr_put_free(z_records *zptr, char *cptr);
zb_code *find_line_call(void *addr);

#endif
