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

#define STARVED -1
#define BLOCKED 0
#define GRANTED 1

/* One for each blocked client process waiting for a lock */
typedef struct cm_lckblkprc_struct
{
	struct cm_lckblkprc_struct	*next;
	struct cm_lckblkprc_struct	*last;
	struct cs_struct		*user;
	mlk_shrblk_ptr_t		blocked;
	uint4				blk_sequence;
} cm_lckblkprc;

typedef cm_lckblkprc *cm_lckblkprc_ptr_t;

/* One for each blocked client lock being waited on */
typedef struct cm_lckblklck_struct
{
	struct cm_lckblklck_struct	*next;
	struct cm_lckblklck_struct	*last;
	mlk_shrblk_ptr_t		node;
	cm_lckblkprc_ptr_t		prc;
	uint4				sequence;	/* Used to determine when we should retry lock */
	ABS_TIME			blktime;
} cm_lckblklck;

typedef cm_lckblklck *cm_lckblklck_ptr_t;

/* One for each region having a blocked lock */
typedef struct cm_lckblkreg_struct
{
	struct cm_region_head_struct	*region;
	struct cm_lckblkreg_struct	*next;
	cm_lckblklck_ptr_t		lock;
	uint4				pass;
} cm_lckblkreg;

typedef cm_lckblkreg *cm_lckblkreg_ptr_t;

#define CM_LKBLK_TIME		16000 /* ms */
#define CM_LKSTARVE_TIME	500 /* ms */
