/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDSKILL_H_INCLUDED
#define GDSKILL_H_INCLUDED

#include <mdefsp.h>

/* Since small memory is allocated in powers of two, keep the kill_set
 * structure size about 8 bytes under 1k mark (current size of
 * header used by memory management system) */
#define BLKS_IN_KILL_SET	251

/* Note that currently GDS_MAX_BLK_BITS is 62. This 62 bit block field allows for a 4Exa GDS block database, but the
 * current actual maximum block size is limited by the size of the master map. */
typedef struct
{
#ifdef BIGENDIAN
	gtm_uint8	flag  : 1;			/* Block was created by this TP transaction (not real block yet) */
	gtm_uint8	level : 1;			/* Block level (zero or non-zero) */
	gtm_uint8	block : GDS_MAX_BLK_BITS;	/* Block number */
#else
	gtm_uint8	block : GDS_MAX_BLK_BITS;	/* Block number */
	gtm_uint8	level : 1;			/* Block level (zero or non-zero) */
	gtm_uint8	flag  : 1;			/* Block was created by this TP transaction (not real block yet) */
#endif
} blk_ident;

typedef struct kill_set_struct
{
	struct kill_set_struct	*next_kill_set;
	int4			used;
	blk_ident		blk[BLKS_IN_KILL_SET];
} kill_set;

#endif
