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

#ifndef __MLKDEF_H__
#define __MLKDEF_H__

/* mlkdef.h */

#include <sys/types.h>

typedef struct	mlk_tp_struct
{
	struct mlk_tp_struct	*next;		/* next stacking struct */
	unsigned int		level : 9;	/* incremental lock level */
	unsigned int		zalloc : 1;	/* if true, there is a ZALLOC posted for this lock */
	unsigned int		tplevel : 8;	/* $TLEVEL when this was stacked */
	unsigned int		unused : 14;	/* ** Reserved ** */
} mlk_tp;

typedef struct				/* One of these nodes is required for each process which is blocked on a lock */
{
        ptroff_t	next;		/* relative pointer to the next mlk_prcblk.  If the entry is in the free
					 * list, then this is a relative pointer to the next free entry. */
	uint4		process_id;	/* the pid of the blocked process */
	short		ref_cnt;	/* number of times process references prcblk */
	unsigned short	unlock;		/* boolean to indicate activity on lock */
} mlk_prcblk;

typedef struct				/* lock node.  The member descriptions below are correct if the entry
					 * is in the tree.  If the entry is in the free list, then all
					 * of the members are meaningless, except for rsib, which is a
					 * relative pointer to the next item in the free list */
{
	ptroff_t	value;		/* relative pointer to the shrsub for this node (always present) */
	ptroff_t	parent;		/* relative pointer to the parent node (zero if name level) */
	ptroff_t	children;	/* relative pointer to the eldest child of this node (zero if none) */
	ptroff_t	lsib;		/* relative pointers to the "left sibling" and "right sibling" */
	ptroff_t	rsib;		/* note that each level of the tree is sorted, for faster lookup */
	ptroff_t	pending;	/* relative pointer to a mlk_prcblk, or zero if no entries are are blocked on this node */
	int4		owner;		/* process id of the owner, if zero, this node is un-owned, and there
					 * must, by defintion, be either a non-zero 'pending' entry, or a
					 * non-zero 'children' entry and in the latter case, at least one
					 * child must have a 'pending' entry */
	uint4		sequence;	/* The sequence number at the time that this node was created.  If
					 * during a direct re-access via pointer from a mlk_pvtblk, the
					 * sequence numbers do not match, then we must assume that the
					 * lock was stolen from us by LKE or some other abnormal event. */
	UINTPTR_T	auxowner;	/* For gt.cm, this contains information on the remote owner of the lock.*/
	int4		image_count;	/* the number of image activiations since login */
	int4		login_time;	/* the low-order 32 bits of the time that the process logged in.  login_time
					 * and image_count can be used together to determine whether the process has
					 * abnormally terminated. */
	int4		auxpid;		/* If non-zero auxowner, this is the pid of the client that is holding the lock */
	unsigned char	auxnode[16];	/* If non-zero auxowner, this is the nodename of the client that is holding the lock */
} mlk_shrblk;

typedef struct				/* the subscript value of a single node in a tree.  Stored separately so that
					 * the mlk_shrblk's can all have fixed positions, and yet we can
					 * efficiently store variable length subscripts. Each entry is rounded to
					 * a length of 4 bytes to eliminiate unaligned references. */
{
	ptroff_t	backpointer;	/* relative pointer to mlk_shrblk which owns this entry, so that we can
					 * efficiently compress the space.  If this is zero, then the item is
					 * an abandon entry. */
	unsigned char	length;		/* length of the data */
	unsigned char	data[1];	/* the data itself, actually data[length] */
} mlk_shrsub;

/* WARNING:  GT.CM and GT.CX rely on the fact that this structure is at the start of the lock space
 * and that the array of pids is at the top of this structure */
#define NUM_CLST_LCKS 64

typedef struct	mlk_ctldata_struct	/* this describes the entire shared lock section */
{
	ptroff_t	prcfree;		/* relative pointer to the first empty mlk_prcblk */
	ptroff_t	blkfree;		/* relative pointer to the first free mlk_shrblk.
						 * if zero, the blkcnt must also equal zero */
	ptroff_t	blkroot;		/* relative pointer to the first name level mlk_shrblk.
						 * if zero, then there are no locks in this section */
	ptroff_t	subbase;		/* relative pointer to the base of the mlk_shrsub area */
	ptroff_t	subfree;		/* relative pointer to the first free cell in the shrsub area */
	ptroff_t	subtop;			/* relative pointer to the top of the shrsub area */
	uint4		max_prccnt;		/* maximum number of entries in the prcfree chain */
	uint4		max_blkcnt;		/* maximum number of entires in the blkfree chain */
	int4		prccnt;			/* number of entries in the prcfree chain */
	int4		blkcnt;			/* number of entires in the blkfree chain */
	uint4		clus_pids[NUM_CLST_LCKS];	/* Pids of processes on other machines in the cluster to be woken up */
	unsigned int	wakeups;			/* lock wakeup counter */
} mlk_ctldata;

/* Define types for shared memory resident structures */

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef mlk_prcblk	*mlk_prcblk_ptr_t;
typedef mlk_shrblk	*mlk_shrblk_ptr_t;
typedef mlk_shrsub	*mlk_shrsub_ptr_t;
typedef mlk_ctldata	*mlk_ctldata_ptr_t;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

/* Now define main private lock structure */

typedef struct	mlk_pvtblk_struct	/* one of these entries exists for each nref which is locked or being processed */
{
	mlk_ctldata_ptr_t	ctlptr;		/* pointer to the mlk_ctldata for the data base region, duplicated to save
							 * recalculating it each time. */
	mlk_shrblk_ptr_t	nodptr;			/* pointer to the node in the shared data structure which corresponds to
							 * this nref */
	mlk_shrblk_ptr_t	blocked;		/* pointer to the node in the shared data structure which blocked this
							 * operation */
	struct mlk_pvtblk_struct
				*next;			/* pointer to the next mlk_pvtblk in this chain.  The chain may be temporary
							 * if lock processing is underway, or permanent, in which case the chain
							 * represents owned locks. */
	struct gd_region_struct	*region;		/* pointer to the database region in which the lock belongs */
	uint4			sequence;		/* shrblk sequence for nodptr node (node we want) */
	uint4			blk_sequence;		/* shrblk sequence for blocked node (node preventing our lock) */
	mlk_tp			*tp;			/* pointer to saved tp information */
	uint4			total_length;		/* the total length of the 'value' string. */
	uint4			total_len_padded;	/* Length with padding to 4 bytes for each substring */
	unsigned short		subscript_cnt;		/* the number of subscripts (plus one for the name) in this nref */
	unsigned		level : 9;		/* incremental lock level */
	unsigned		zalloc : 1;		/* if true, there is a ZALLOC posted for this lock */
	unsigned		granted : 1;		/* if true, the lock has been granted in the database */
	unsigned		unused : 5;		/* ** Unused ** the number of bits in the bit-fields add up to
								only 16, therefore although they have type unsigned,
								they are accommodated within 2 bytes. Since there is
								no type for bit-fields, unsigned should be the only
								type specified and alignment works according to the
								total number of bits */
	unsigned char		trans;			/* boolean indicating whether already in list */
	unsigned char		translev;		/* level for transaction (accounting for redundancy) */
	unsigned char		old;			/* oldness boolean used for backing out zallocates */
	unsigned char		filler[1];		/* Fill out to align data on word boundary */

	unsigned char		value[1];		/* actually, an array unsigned char value[total_length].  This string
							 * consists of the nref's subscripts each preceeded by the length of
							 * the subscript, held as a single byte.  For example, ^A(45), would be
							 * represented as 02 5E 41 02 34 35, and total length would be 5. */
} mlk_pvtblk;

/* convert relative pointer to absolute pointer */
#define R2A(X) (((sm_uc_ptr_t) &(X)) + (X))

/* store absolute pointer Y in X as a relative pointer */
#define A2R(X, Y) ((X) = (ptroff_t)(((sm_uc_ptr_t)(Y)) - ((sm_uc_ptr_t) &(X))))

/* compute the true size of a mlk_pvtblk */
#define MLK_PVTBLK_SIZE(DLEN, SCNT) (SIZEOF(mlk_pvtblk) - 1 + (DLEN) + (SCNT))

/* compute the true size of a mlk_shrsub include stddef.h*/
#define MLK_SHRSUB_SIZE(X) (ROUND_UP(OFFSETOF(mlk_shrsub, data[0]) + (X)->length, SIZEOF(ptroff_t)))
/* SIZEOF(ptroff_t) - 1 is used for padding because mlk_shrblk_create() ROUND_UPs to SIZEOF(ptroff_t) aligned boundary */
#define MLK_PVTBLK_SHRSUB_SIZE(PVTBLK, SHRSUBNEED) \
(SHRSUBNEED * (OFFSETOF(mlk_shrsub, data[0]) + SIZEOF(ptroff_t) - 1) + PVTBLK->total_length)

#define DEF_LOCK_SIZE OS_PAGELET_SIZE * 200

typedef struct mlk_stats_struct
{
	gtm_uint64_t	n_user_locks_success;
	gtm_uint64_t	n_user_locks_fail;
} mlk_stats_t;

#define MLK_FAIRNESS_DISABLED	((uint4)-1)

#endif
