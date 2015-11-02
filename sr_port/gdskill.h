/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

/* Note the currently GDS_MAX_BLK_BITS is 28. This 28 bit block field allows for a 256M GDS block database.
 * The three byte level field is the minimum required to contain the level. However, if more addressability
 * is needed, the level field can likely be shrunk to a single bit to indicate a non-zero level and then the
 * actual level obtained when the blocks are read in. Note that if this changes, a comment in gvcst_init()
 * also needs adjustment.
 */
typedef struct
{
#ifdef BIGENDIAN
	unsigned int    flag  : 1;			/* Block was created by this TP transaction (not real block yet) */
	unsigned int	level : 3;			/* Block level (0 to 6) */
	unsigned int	block : GDS_MAX_BLK_BITS;	/* Block number */
#else
	unsigned int	block : GDS_MAX_BLK_BITS;	/* Block number */
	unsigned int	level : 3;			/* Block level (0 to 6) */
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

