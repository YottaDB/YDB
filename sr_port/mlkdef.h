/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MLKDEF_H_INCLUDED
#define MLKDEF_H_INCLUDED

/* mlkdef.h */

#include <stddef.h>	/* For offsetof() */
#include <sys/types.h>

#include "mmrhash.h"

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
	short		filler_4byte;
} mlk_prcblk;

/* Types and structures for shrblk lookup hash table - These configuration options are not set by a header file so
 * these definitions supply the value types and formatting macros for when the values are printed. If we (or anyone)
 * want to do a quick build with a 64 bit subhash map (max neighborhood size of 32 instead of 64) for testing purposes,
 * we need only include it as a build option to get it to build that way. Alternatively, if we want to change the size
 * of one of these blocks permanently, changing 32 to 64 or vice versa is all we need to do in the macros below.
 */
#if !defined(MLK_SHRHASH_MAP_32) && !defined(MLK_SHRHASH_MAP_64)
#	define	MLK_SHRHASH_MAP_32
#endif
#if !defined(MLK_SUBHASH_VAL_32) && !defined(MLK_SUBHASH_VAL_64)
#	define	MLK_SUBHASH_VAL_64
#endif

/* Note these macro use PRI prefixed macros defined by [g]cc as printf() format types for however large the
 * fields are being defined. This allows the fields to be printed by [f]printf() calls.
 */
#ifdef MLK_SHRHASH_MAP_32
	typedef uint4				mlk_shrhash_map_t;
#	define MLK_SHRHASH_MAP_MAX		MAXUINT4
#	define PRIUSEDMAP			PRIx32
#elif defined(MLK_SHRHASH_MAP_64)
	typedef gtm_uint8			mlk_shrhash_map_t;
#	define MLK_SHRHASH_MAP_MAX		MAXUINT8
#	define PRIUSEDMAP			PRIx64
#endif
#ifdef MLK_SUBHASH_VAL_32
	typedef uint4				mlk_subhash_val_t;
#	define PRIUSEDHASH			PRIx32
#elif defined(MLK_SUBHASH_VAL_64)
	typedef gtm_uint8			mlk_subhash_val_t;
#	define PRIUSEDHASH			PRIx64
#endif

/* Types and macros for computing subscript hash values */
typedef hash128_state_t				mlk_subhash_state_t;
typedef gtm_uint8				mlk_subhash_seed_t;
typedef gtm_uint16				mlk_subhash_res_t;
#define MLK_SUBHASH_INIT(PVTBLK, STATEVAR)							\
MBSTART {											\
	assert(NULL != (PVTBLK)->pvtctl.ctl);							\
	(PVTBLK)->hash_seed = (PVTBLK)->pvtctl.ctl->hash_seed;					\
	HASH128_STATE_INIT(STATEVAR, (PVTBLK)->hash_seed);					\
} MBEND
#define MLK_SUBHASH_INIT_PVTCTL(PVTCTL, STATEVAR)	HASH128_STATE_INIT(STATEVAR, (PVTCTL)->ctl->hash_seed)
#define MLK_SUBHASH_INGEST(STATEVAR, DATA, SIZE)	ydb_mmrhash_128_ingest(&(STATEVAR), (DATA), (SIZE))
#define MLK_SUBHASH_FINALIZE(STATEVAR, LENGTH, RESVAR)	ydb_mmrhash_128_result(&(STATEVAR), LENGTH, &(RESVAR))
/* Macro to extract 64 bits of hash from a 128 bit hash value */
#define MLK_SUBHASH_RES_VAL(RES)			((mlk_subhash_val_t)(RES).one)

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
	uint4		owner;		/* process id of the owner, if zero, this node is un-owned, and there
					 * must, by defintion, be either a non-zero 'pending' entry, or a
					 * non-zero 'children' entry and in the latter case, at least one
					 * child must have a 'pending' entry */
	uint4		sequence;	/* The sequence number at the time that this node was created.  If
					 * during a direct re-access via pointer from a mlk_pvtblk, the
					 * sequence numbers do not match, then we must assume that the
					 * lock was stolen from us by LKE or some other abnormal event. */
	mlk_subhash_val_t	hash;	/* The hash value associated with this node, based on the shrsubs of this node
					 * and its parents. Copied from a mlk_pvtblk via MLK_PVTBLK_SUBHASH().
					 */
	int4		auxpid;		/* If non-zero auxowner, this is the pid of the client that is holding the lock */
#	if defined(MLK_SUBHASH_VAL_64) && defined(GTM64)
	uint4		unused;		/* Add for alignment when the hash is 8 bytes and next field is 8 byte aligned */
#	endif
	UINTPTR_T	auxowner;	/* For gt.cm, this contains information on the remote owner of the lock.*/
	unsigned char	auxnode[16];	/* If non-zero auxowner, this is the nodename of the client that is holding the lock */
} mlk_shrblk;

typedef struct mlk_shrhash_struct
{
	uint4			shrblk_idx;	/* Index to shrblk referenced by this hash bucket, or zero for empty. */
#	ifdef MLK_SHRHASH_MAP_64
	uint4			unused1;	/* Alignment for 64 bit field following (if it is 64 bit) */
#	endif
	mlk_shrhash_map_t	usedmap;	/* Bitmap representing the bucket neighborhood, with bit N set
						 * if (bucket+N) % nbuckets is associated with this bucket, i.e.,
						 * part of this bucket's neighborhood.
						 */
	mlk_subhash_val_t	hash;		/* Hash value associated with the shrblk referenced by this hash bucket.
						 * Compare the hash value before comparing the pvtblk value against the
						 * shrblk/shrsub chain
						 */
#	if defined(MLK_SUBHASH_VAL_32) && defined(MLK_SHRHASH_MAP_64)
	uint4			unused2;	/* Alignment for 64 bit field following (if it is 64 bit) */
#	endif
} mlk_shrhash;

#define MLK_SHRHASH_NEIGHBORS		(SIZEOF(mlk_shrhash_map_t) * BITS_PER_UCHAR)
#define IS_NEIGHBOR(MAP, OFFSET)	(0 != ((MAP) & (((mlk_shrhash_map_t)1) << (OFFSET))))
#define SET_NEIGHBOR(MAP, OFFSET)	(MAP) |= (((mlk_shrhash_map_t)1) << (OFFSET))
#define CLEAR_NEIGHBOR(MAP, OFFSET)	(MAP) &= ~(((mlk_shrhash_map_t)1) << (OFFSET))
/* The number of buckets the hash table is based on the number of shrblks, plus some extra.
 * The number of extra buckets is specified as a fraction of the number of shrblks by the following.
 */
#define MLK_HASH_EXCESS		(1.0/2.0)
/* From the time we attempt to get a lock until the time we actually get it, we are allowed to immediately retry a
 * failed lock attempt calling mlk_lock() under the following circumstances:
 *  - The lock attempt set ctl->gc_needed or ctl->rehash_needed or ctl->resize_needed
 *  - There have been fewer than MAX_LOCK_GC_REHASH_RESIZE_RETRYS defined below.
 */
#define MAX_LOCK_GC_REHASH_RESIZE_RETRYS 2

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

/* WARNING:  GT.CM relies on the fact that this structure is at the start of the lock space */
#define NUM_CLST_LCKS 64

#define MLK_CTL_BLKHASH_EXT (0)

typedef struct	mlk_ctldata_struct	/* this describes the entire shared lock section */
{
	ptroff_t	prcfree;		/* relative pointer to the first empty mlk_prcblk */
	ptroff_t	blkbase;		/* relative pointer to the first mlk_shrblk array entry */
	ptroff_t	blkfree;		/* relative pointer to the first free mlk_shrblk.
						 * if zero, the blkcnt must also equal zero */
	ptroff_t	blkhash;		/* relative pointer to the first mlk_shrhash, or MLK_CTL_BLKHASH_EXT if external */
	ptroff_t	blkroot;		/* relative pointer to the first name level mlk_shrblk.
						 * if zero, then there are no locks in this section */
	ptroff_t	subbase;		/* relative pointer to the base of the mlk_shrsub area */
	ptroff_t	subfree;		/* relative pointer to the first free cell in the shrsub area */
	ptroff_t	subtop;			/* relative pointer to the top of the shrsub area */
	uint4		max_prccnt;		/* maximum number of entries in the prcfree chain */
	uint4		max_blkcnt;		/* maximum number of entries in the blkfree chain */
	uint4		num_blkhash;		/* number of hash buckets */
	int4		prccnt;			/* number of entries in the prcfree chain */
	int4		blkcnt;			/* number of entries in the blkfree chain */
	unsigned int	wakeups;		/* lock wakeup counter */
	boolean_t	lockspacefull_logged;	/* whether LOCKSPACEFULL has been issued since last being below threshold */
	boolean_t	gc_needed;		/* whether we've determined that a garbage collection is needed */
	boolean_t	resize_needed;		/* whether we've determined that a hash table resize is needed */
	boolean_t	rehash_needed;		/* whether we've determined that a hash table rehash is needed */
	global_latch_t	lock_gc_in_progress;	/* pid of the process doing the GC, or 0 if none */
	mlk_subhash_seed_t hash_seed;		/* seed value to use to initialize hash */
	int		hash_shmid;		/* shared memory id of hash table, or undefined if internal (see blkhash) */
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
typedef mlk_shrhash	*mlk_shrhash_ptr_t;

/* Uncomment the below line if you want to turn on lock hash debugging */
//#define MLK_SHRHASH_DEBUG
#ifdef MLK_SHRHASH_DEBUG
#	define SHRHASH_DEBUG_ONLY(x) x
	void mlk_shrhash_validate(mlk_ctldata_ptr_t ctl);
#else
#	define SHRHASH_DEBUG_ONLY(x)
#endif

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

/* Now define main private lock structures */

typedef struct mlk_pvtctl_struct
{
	struct gd_region_struct		*region;		/* pointer to the database region */
	struct sgmnt_addrs_struct	*csa;			/* pointer to the database cs_addrs */
	mlk_ctldata_ptr_t		ctl;			/* pointer to the shared mlk_ctldata */
	mlk_shrblk_ptr_t		shrblk;			/* pointer to the base of the shrblk array (indexed from one) */
	mlk_shrhash_ptr_t		shrhash;		/* pointer to the hash array */
	uint4				shrhash_size;		/* number of hash buckets */
	uint4				hash_fail_cnt;		/* number of consecutive hash insert failures */
	unsigned int			gc_needed : 1;		/* Copy of gc_needed/rehash_needed/resize_needed from mlk_ctldata */
	unsigned int			rehash_needed : 1;	/* .. copied here so can test these flags outside of lock to avoid */
	unsigned int			resize_needed : 1;	/* .. missing a gc/rehash/resize due to concurent action. */
	unsigned int			unused : 29;		/* ** Unused ** available for use */
	GTM64_ONLY(uint4		unused2;)		/* Align the block to 8 bytes */
} mlk_pvtctl;

typedef mlk_pvtctl	*mlk_pvtctl_ptr_t;

#define MLK_PVTCTL_SET_CTL(PCTL, CTL)											\
		mlk_pvtctl_set_ctl(&(PCTL), CTL)
#define MLK_PVTCTL_INIT(PCTL, REG)											\
		mlk_pvtctl_init(&(PCTL), REG)

typedef struct	mlk_pvtblk_struct	/* one of these entries exists for each nref which is locked or being processed */
{
	mlk_pvtctl		pvtctl;			/* Non lock specific control information */
	mlk_shrblk_ptr_t	nodptr;			/* pointer to the node in the shared data structure which corresponds to
							 * this nref */
	mlk_shrblk_ptr_t	blocked;		/* pointer to the node in the shared data structure which blocked this
							 * operation */
	struct mlk_pvtblk_struct
				*next;			/* pointer to the next mlk_pvtblk in this chain.  The chain may be temporary
							 * if lock processing is underway, or permanent, in which case the chain
							 * represents owned locks. */
	uint4			sequence;		/* shrblk sequence for nodptr node (node we want) */
	uint4			blk_sequence;		/* shrblk sequence for blocked node (node preventing our lock) */
	mlk_tp			*tp;			/* pointer to saved tp information */
	mlk_subhash_val_t	*hash_array;		/* Hash table pointer for this lock's subscripts */
	mlk_subhash_seed_t	hash_seed;		/* seed used to initialize hash */
	uint4			nref_length;		/* the length of the nref portion of the 'value' string. */
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
	unsigned char		filler[5];		/* Fill out to align data on address boundary */
#	ifdef DEBUG
	size_t			alloc_size;
	uint4			alloc_nref_len;
	uint4			alloc_sub_cnt;
	uint4			alloc_aux_size;
	GTM64_ONLY(uint4	filler2;)
#	endif
	unsigned char		value[1];		/* Actually, an array unsigned char value[N], where N is based on the
							 * nref_length, subscript_cnt, and possibly a client id. The initial
							 * portion consists of the nref's subscripts, each preceded by the length
							 * of the subscript, held as a single byte. For example, ^A(45), would be
							 * represented as 02 5E 41 02 34 35, and total length would be 5.
							 * Following that (aligned to caddr_t) is an array of subscript_cnt
							 * hash values (mlk_shrhash_val_t).
							 * For GT.CM servers, the hash values are followed by a client id length
							 * as a single byte followed by server-specific data of that length.
							 * The MLK_PVTBLK_SIZE() macro determines the total size of the block,
							 * excluding any GT.CM portion.
							 * The MLK_PVTBLK_TAIL() macro may be used by GT.CM servers to locate
							 * their client id information.
							 */
} mlk_pvtblk;

/* convert relative pointer to absolute pointer */
#define R2A(X) (DBG_ASSERT(X) (((sm_uc_ptr_t) &(X)) + (X)))

/* store absolute pointer Y in X as a relative pointer */
#define A2R(X, Y) (DBG_ASSERT(Y) ((X) = (ptroff_t)(((sm_uc_ptr_t)(Y)) - ((sm_uc_ptr_t) &(X)))))

/* get the index for use in a shrhash */
#define MLK_SHRBLK_IDX(PCTL, SHRBLK)		((SHRBLK) - (PCTL).shrblk)
/* get a mlk_shrblk pointer from an mlk_shrhash pointer */
#define MLK_SHRHASH_SHRBLK(PCTL, SHRHASH)	((PCTL).shrblk + (SHRHASH)->shrblk_idx)
#define MLK_SHRHASH_SHRBLK_CHECK(PCTL, SHRHASH)	((SHRHASH)->shrblk_idx ? MLK_SHRHASH_SHRBLK(PCTL, SHRHASH) : NULL)

/* compute the true size of a mlk_pvtblk, excluding any GT.CM id */
#define MLK_PVTBLK_SIZE(NREF_LEN, SUBCNT) (ROUND_UP(OFFSETOF(mlk_pvtblk, value) + (NREF_LEN), SIZEOF(caddr_t))	\
					   + SIZEOF(mlk_subhash_val_t) * (SUBCNT))

/* Allocates 3 pieces of storage in one block:
 *  1. mlk_pvtblk
 *  2. buffer (starting at mlk_pvtblk.value) for text subscripts each preceded by a
 *     one byte length.
 *  3. array of 0 or more mlk_subhash_val_t blocks containing subscript hash values.
 */
#define MLK_PVTBLK_ALLOC(NREF_LEN, SUBCNT, AUX_LEN, RET)				\
MBSTART {										\
	mlk_pvtblk	*ret;								\
	size_t		alloc_size;							\
	uint4		alloc_nref_len = (NREF_LEN);					\
	uint4		alloc_sub_cnt = (SUBCNT);					\
	uint4		alloc_aux_size = (AUX_LEN);					\
											\
	alloc_size = MLK_PVTBLK_SIZE(NREF_LEN, SUBCNT) + (AUX_LEN);			\
	ret = (mlk_pvtblk *)malloc(alloc_size);						\
	memset(ret, 0, OFFSETOF(mlk_pvtblk, value));					\
	ret->hash_array = (mlk_subhash_val_t *)((caddr_t)ret				\
		+ ROUND_UP(OFFSETOF(mlk_pvtblk, value) + (NREF_LEN), SIZEOF(caddr_t)));	\
	DEBUG_ONLY(ret->alloc_size = alloc_size);					\
	DEBUG_ONLY(ret->alloc_nref_len = alloc_nref_len);				\
	DEBUG_ONLY(ret->alloc_sub_cnt = alloc_sub_cnt);					\
	DEBUG_ONLY(ret->alloc_aux_size = alloc_aux_size);				\
	(RET) = ret;									\
} MBEND

/* compute the location of the Nth subscript's hash */
#define MLK_PVTBLK_SUBHASH(PVTBLK, N) ((PVTBLK)->hash_array[N])

#ifdef DEBUG
# define	DBG_LOCKHASH_N_BITS(HASH)						\
{											\
	DCL_THREADGBL_ACCESS;								\
											\
	SETUP_THREADGBL_ACCESS;								\
	/* In dbg code, restrict hash to N bits where N can be anywhere from 0 to 32.	\
	 * This lets us test codepaths where there are lots of hash collisions.		\
	 */										\
	if (TREF(ydb_lockhash_n_bits))							\
	{										\
		assert(32 >= TREF(ydb_lockhash_n_bits));				\
		HASH = HASH & (((gtm_uint8)1 << TREF(ydb_lockhash_n_bits)) - 1);	\
		/* If HASH ends up being 0, then set it to a non-zero value as		\
		 * 0 hash value has other issues.					\
		 */									\
		if (0 == HASH)								\
			HASH = 1;							\
	}										\
}
#else
# define	DBG_LOCKHASH_N_BITS(X)
#endif

/* populate hash data from nref data - keep in sync with the versions in mlk_pvtblk_create() and mlk_shrhash_delete() */
#define MLK_PVTBLK_SUBHASH_GEN(PVTBLK)										\
MBSTART {													\
	unsigned char		*cp;										\
	int			hi;										\
	mlk_subhash_state_t	accstate, tmpstate;								\
	mlk_subhash_res_t	hashres;									\
														\
	MLK_SUBHASH_INIT(PVTBLK, accstate);									\
	for (cp = (PVTBLK)->value, hi = 0; hi < (PVTBLK)->subscript_cnt; hi++)					\
	{													\
		MLK_SUBHASH_INGEST(accstate, cp, *cp + 1);							\
		cp += *cp + 1;											\
		tmpstate = accstate;										\
		MLK_SUBHASH_FINALIZE(tmpstate, (cp - (PVTBLK)->value), hashres);				\
		DBG_LOCKHASH_N_BITS(hashres.one);								\
		MLK_PVTBLK_SUBHASH(PVTBLK, hi) = MLK_SUBHASH_RES_VAL(hashres);					\
	}													\
} MBEND

/* ensure that the seed is correct before using pvtblk hashes */
#define MLK_PVTBLK_SUBHASH_SYNC(PVTBLK)							\
MBSTART {										\
	assert(NULL != (PVTBLK)->pvtctl.ctl);						\
	if ((PVTBLK)->hash_seed != (PVTBLK)->pvtctl.ctl->hash_seed)			\
	{										\
		MLK_PVTBLK_SUBHASH_GEN(PVTBLK);						\
		assert((PVTBLK)->hash_seed == (PVTBLK)->pvtctl.ctl->hash_seed);		\
	}										\
} MBEND

/* compute the address immediately after the pvtblk */
#define MLK_PVTBLK_TAIL(PVTBLK) ((unsigned char *)&MLK_PVTBLK_SUBHASH(PVTBLK, (PVTBLK)->subscript_cnt))

#ifdef DEBUG
#define MLK_PVTBLK_VALIDATE(PVTBLK)										\
MBSTART {													\
	unsigned char	*tail = MLK_PVTBLK_TAIL(PVTBLK);							\
														\
	assert((PVTBLK)->nref_length == (PVTBLK)->alloc_nref_len);						\
	assert((PVTBLK)->subscript_cnt == (PVTBLK)->alloc_sub_cnt);						\
	assert((PVTBLK)->alloc_size == (MLK_PVTBLK_SIZE((PVTBLK)->nref_length, (PVTBLK)->subscript_cnt)		\
						+ (PVTBLK)->alloc_aux_size));					\
	if(0 != (PVTBLK)->alloc_aux_size)									\
		assert(tail[0] + 1 == (PVTBLK)->alloc_aux_size);						\
} MBEND
#else
#define MLK_PVTBLK_VALIDATE(PVTBLK)
#endif

/* compute the true size of a mlk_shrsub include stddef.h*/
#define MLK_SHRSUB_SIZE(X) (ROUND_UP(OFFSETOF(mlk_shrsub, data[0]) + (X)->length, SIZEOF(ptroff_t)))
/* SIZEOF(ptroff_t) - 1 is used for padding because mlk_shrblk_create() ROUND_UPs to SIZEOF(ptroff_t) aligned boundary */
#define MLK_PVTBLK_SHRSUB_SIZE(PVTBLK, SHRSUBNEED) \
(SHRSUBNEED * (OFFSETOF(mlk_shrsub, data[0]) + SIZEOF(ptroff_t) - 1) + PVTBLK->nref_length)

/* Default lock size if a 0 lock space size is specified - GDE prevents this from happening but it can happen that lock
 * space size can be zeroed in which case, this much is allocated. This value (currently 220) should be kept in sync
 * with the value GDE uses in sr_port/gdeinit.m when it sets defseg("LOCK_SPACE").
 */
#define DEF_LOCK_SIZE OS_PAGELET_SIZE * 220

typedef struct mlk_stats_struct
{
	gtm_uint64_t	n_user_locks_success;
	gtm_uint64_t	n_user_locks_fail;
} mlk_stats_t;

#define MLK_FAIRNESS_DISABLED	((uint4)-1)

#define CHECK_SHRBLKPTR(RPTR, PCTL)											\
	assert((RPTR == 0)												\
		|| (((mlk_shrblk_ptr_t)R2A(RPTR) > (PCTL).shrblk) 							\
			&& ((mlk_shrblk_ptr_t)R2A(RPTR) < ((PCTL).shrblk + (PCTL).ctl->max_blkcnt + 1))))

#define INVALID_LSIB_MARKER	0x1ee4c0de

#define LOCK_SPACE_FULL_SYSLOG_THRESHOLD	0.25	/* Minimum free space percentage to write LOCKSPACEFULL to syslog again.
							 * MIN(free_prcblk_ratio, free_shr_blk_ratio) must be greater than this
							 * value to print syslog again (see also: gdsbt.h,
							 * lockspacefull_logged definition
							 */

/* macros to grab and release a critical section (either shared with DB or not) for LOCK operations */
#define GRAB_LOCK_CRIT_AND_SYNC(PCTL, RET_WAS_CRIT)								\
		grab_lock_crit_and_sync(&(PCTL), &(RET_WAS_CRIT))
#define GRAB_LOCK_CRIT_INTL(PCTL, RET_WAS_CRIT)							\
		grab_lock_crit_intl(&(PCTL), &(RET_WAS_CRIT))
#define REL_LOCK_CRIT(PCTL, WAS_CRIT)								\
		rel_lock_crit(&(PCTL), WAS_CRIT)
#define LOCK_CRIT_OWNER(CSA)									\
		((CSA)->lock_crit_with_db ? -1 : (CSA)->nl->lock_crit.u.parts.latch_pid)
#define LOCK_CRIT_HELD(CSA)									\
		((CSA)->lock_crit_with_db ? ((CSA)->now_crit) : (process_id == (CSA)->nl->lock_crit.u.parts.latch_pid))

#endif
