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

#ifndef __GDSKILL_H__
#define __GDSKILL_H__

/* Since small memory is allocated in powers of two, keep the kill_set
   structure size about 8 bytes under 1k mark (current size of
   header used by memory management system) */
#define BLKS_IN_KILL_SET	251

typedef struct
{
#ifdef BIGENDIAN
	unsigned int    flag  : 1;
	unsigned int	level : 5;
	unsigned int	block : 26;
#else
	unsigned int	block : 26;
	unsigned int	level : 5;
	unsigned int	flag  : 1;
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

