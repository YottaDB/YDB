/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GDSKILL_H__
#define __GDSKILL_H__

/* Since small memory is allocated in powers of two, keep the kill_set
   structure size about 8 bytes under 1k mark (current size of
   header used by memory management system) */
#define BLKS_IN_KILL_SET	251

/* Note that currently GDS_MAX_BLK_BITS is 30. This 30 bit block field allows for a 1G GDS block database. */
typedef struct
{
#ifdef BIGENDIAN
	unsigned int    flag  : 1;			/* Block was created by this TP transaction (not real block yet) */
	unsigned int	level : 1;			/* Block level (zero or non-zero) */
	unsigned int	block : GDS_MAX_BLK_BITS;	/* Block number */
#else
	unsigned int	block : GDS_MAX_BLK_BITS;	/* Block number */
	unsigned int	level : 1;			/* Block level (zero or non-zero) */
	unsigned int    flag  : 1;			/* Block was created by this TP transaction (not real block yet) */
#endif
} blk_ident;

typedef struct kill_set_struct
{
	struct kill_set_struct
			*next_kill_set;
	int4		used;
	blk_ident	blk[BLKS_IN_KILL_SET];
} kill_set;

#endif

