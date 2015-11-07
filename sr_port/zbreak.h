/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ZBREAK_H_INCLUDED
#define ZBREAK_H_INCLUDED

#include <zbreaksp.h>
#include "cache.h"

/* Structure for each ZBREAK set in the process */
typedef struct
{
	zb_code		*mpc;		/* MUMPS address for ZBREAK */
	mident		*rtn;		/* points to the routine_name field of the target routine header */
	mident		*lab;		/* points to the lab_name field of target's label table entry */
	int		offset;
	int		count;		/* # of time ZBREAK encountered */
	cache_entry	*action;	/* action associated with ZBREAK (indirect cache entry) */
	rhdtyp		*rtnhdr;	/* Routine header of routine containing ZBREAK */
	zb_code 	m_opcode;	/* MUMPS op_code replaced */
	char		filler[SIZEOF(char *) - SIZEOF(zb_code)];
} zbrk_struct;

/* ZBREAK anchoring structure */
typedef struct
{
	zbrk_struct	*beg;
	zbrk_struct	*free;
	zbrk_struct	*end;
} z_records;

#ifndef ZB_CODE_SHIFT
#define ZB_CODE_SHIFT 0
#endif

#define BREAKMSG	TRUE
#define NOBREAKMSG	FALSE
#define INIT_NUM_ZBREAKS 1
#define CANCEL_ONE	-1
#define CANCEL_ALL	-2

#define SIZEOF_LA	0

#define	RETURN_CLOSEST_MATCH_FALSE	FALSE
#define	RETURN_CLOSEST_MATCH_TRUE	TRUE

zbrk_struct *zr_find(z_records *zrecs, zb_code *addr, boolean_t return_closest_match);
zbrk_struct *zr_add_zbreak(z_records *zrecs, zb_code *addr);
void zr_init(z_records *zrecs, int4 count);
void zr_remove_zbreak(z_records *zrecs, zbrk_struct *z_ptr);
zb_code *find_line_call(void *addr);
void zr_remove_zbrks(rhdtyp *rtn, boolean_t notify_is_trigger);

#endif /* ZBREAK_H_INCLUDED */
