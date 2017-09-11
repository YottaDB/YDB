/****************************************************************
 *								*
 * Copyright (c) 2013-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef RELINKCTL_H_INCLUDED
#define RELINKCTL_H_INCLUDED

# include "gtm_limits.h"
# include "parse_file.h"	/* for MAX_FBUFF */

/* Input RTNNAME is derived from an object file name and we need to convert it to a proper routine name.
 * a) It is possible we got a '_' as the first character in case of a % routine. But the actual routine name stored in
 *	that object file would have '%' as the first character. Fix that.
 * b) Also limit routine name length to MAX_MIDENT_LEN (internal design max). Ignore the rest of the file name.
 */
#define	CONVERT_FILENAME_TO_RTNNAME(RTNNAME)		\
{							\
	if ('_' == RTNNAME.addr[0])			\
		RTNNAME.addr[0] = '%';			\
	if (MAX_MIDENT_LEN < RTNNAME.len)		\
		RTNNAME.len = MAX_MIDENT_LEN;		\
}

#define	COMPUTE_RELINKCTL_HASH(RTNNAME, RTNHASH, RELINKCTL_HASH_BUCKETS)	\
{										\
	STR_HASH((RTNNAME)->addr, (RTNNAME)->len, RTNHASH, 0);			\
	RTNHASH = RTNHASH % RELINKCTL_HASH_BUCKETS;				\
}

/* One relinkctl file can contain at most this many # of routines */
#  define	RELINKCTL_MAX_ENTRIES		16000000
#  define	RELINKCTL_MIN_ENTRIES		(WBTEST_ENABLED(WBTEST_RELINKCTL_MAX_ENTRIES) ? 1 : 1000)
#  define	RELINKCTL_DEFAULT_ENTRIES	(WBTEST_ENABLED(WBTEST_RELINKCTL_MAX_ENTRIES) ? 100 : 50000)

#define	RELINKCTL_MMAP_SZ	((size_t)SIZEOF(relinkctl_data))
#define	RELINKSHM_HDR_SIZE	((size_t)SIZEOF(relinkshm_hdr_t))
/* We are guaranteed relinkctl_hash_buckets is an odd prime number and since we want at least 8-byte alignment between different
 * sections of shared memory, we add a 4-byte filler to the RELINKSHM_RTNHASH_SIZE macro computation (that's why we are adding + 1).
 */
#define	RELINKSHM_RTNHASH_SIZE(RELINKCTL_HASH_BUCKETS)	(((size_t)RELINKCTL_HASH_BUCKETS + 1) * SIZEOF(uint4))
#define	RELINKSHM_RECARRAY_SIZE(RELINKCTL_MAX_RTN_ENTRIES)	(((size_t)RELINKCTL_MAX_RTN_ENTRIES) * SIZEOF(relinkrec_t))
#define	RELINKCTL_SHM_SIZE(RELINKCTL_HASH_BUCKETS, RELINKCTL_MAX_RTN_ENTRIES)	(RELINKSHM_HDR_SIZE	\
	+ RELINKSHM_RTNHASH_SIZE(RELINKCTL_HASH_BUCKETS) + RELINKSHM_RECARRAY_SIZE(RELINKCTL_MAX_RTN_ENTRIES))

#define	GET_RELINK_SHM_HDR(LINKCTL)	(relinkshm_hdr_t *)((sm_uc_ptr_t)LINKCTL->shm_hashbase - SIZEOF(relinkshm_hdr_t))

error_def(ERR_RLNKRECLATCH);	/* needed for the RELINKCTL_CYCLE_INCR macro */

/* Macro to bump the cycle# of a given relinkctl record. Make sure shared cycle never becomes 0 after the bump since a process
 * initializes its private cycle to 0 at relinkctl file open time and we want to make sure the private-cycle == shared-cycle
 * check fails always the first time for a process.
 */
#define RELINKCTL_CYCLE_INCR(RELINKREC, LINKCTL)						\
{												\
	if (!grab_latch(&(RELINKREC)->rtnobj_latch, RLNKREC_LATCH_TIMEOUT_SEC))			\
	{											\
		assert(FALSE);									\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_RLNKRECLATCH, 3,			\
			RELINKREC->rtnname_fixed.c, RTS_ERROR_MSTR(&LINKCTL->zro_entry_name));	\
	}											\
	if (0 == ++(RELINKREC)->cycle)								\
		++(RELINKREC)->cycle;								\
	rel_latch(&(RELINKREC)->rtnobj_latch);							\
}

/* A routine buffer is stored a shared memory segment. Since shm sizes are fixed, if ever we cannot fit in a given routine
 * buffer in a given shm, we create another shm with double the size and keep doing this until we come with a shm that can
 * fit in the given routine buffer. We allow for a max of MAX_RTNOBJ_SHM_INDEX such shmids to be allocated per relinkctl file.
 * Since we need 6 bits to represent MAX_RTNOBJ_SHM_INDEX, we treat the array of shmids to be on huge 2**64 sized shmid. Where
 * the top 6 bits represent the index in the shmid array and the remaining 58 bits correspond to the offset within that shmid
 * where the routine buffer can be found. Such an offset into a routine buffer in shared memory is typed as rtnobj_sm_off_t.
 */
#define	RTNOBJ_SHMID_INDEX_MAXBITS	6	/* Max # of bits needed to store NUM_RTNOBJ_SHM_INDEX */
#define	MIN_RTNOBJ_SHM_INDEX	20	/* Minimum size of shared memory segment created to store .o (rtnobj) files is 2**20.
					 * Do not change this macro as gtm_autorelink_shm env var is specified in a multiple of
					 * 2**MIN_RTNOBJ_SHM_INDEX bytes and will have user doc implications.
					 */
#define	MAX_RTNOBJ_SHM_INDEX	58	/* Maximum size of shared memory segment created to store .o (rtnobj) files is 2**57 */
#define NUM_RTNOBJ_SHM_INDEX	(MAX_RTNOBJ_SHM_INDEX - MIN_RTNOBJ_SHM_INDEX)

#define	NULL_RTNOBJ_SM_OFF_T	((sm_off_t)MAXUINT8)	/* Is (2**64 - 1) i.e. 0xFFFF FFFF FFFF FFFF */

#define	RTNOBJ_GET_SHM_INDEX(SHM_OFF)	(SHM_OFF >> MAX_RTNOBJ_SHM_INDEX)
#define	RTNOBJ_GET_SHM_OFFSET(SHM_OFF)	(SHM_OFF & 0x03FFFFFFFFFFFFFFULL)
#define	RTNOBJ_SET_SHM_INDEX_OFF(SHM_INDEX, SHM_OFF)	(((rtnobj_sm_off_t)SHM_INDEX << MAX_RTNOBJ_SHM_INDEX) | (SHM_OFF))

/* For the latch timeouts below, we believe most are likely done within 1 minute but since IO can be done and a failure in one
 * of these locks is a hard-error, the max is set to 4 mins.
 */
#define	RLNKSHM_LATCH_TIMEOUT_SEC	(4 * 60)	/* 4 min */
#define	RLNKREC_LATCH_TIMEOUT_SEC	(4 * 60)	/* 4 min */

#define	MIN_RTNOBJ_SIZE_BITS	8		      /* Minimum object file size (including SIZEOF(rtnobj_hdr_t)) is 2**8 = 256 */
#define	MAX_RTNOBJ_SIZE_BITS	MAX_RTNOBJ_SHM_INDEX  /* Maximum object file size (including SIZEOF(rtnobj_hdr_t)) is 2**32
						       * but when we allocate a rtnobj shared memory segment of size
						       * (2**MAX_RTNOBJ_SHM_INDEX) we want to add that as one element to the
						       * "rtnobjshm_hdr_t->freeList[]" array and hence this definition.
						       */
#define	NUM_RTNOBJ_SIZE_BITS	(MAX_RTNOBJ_SIZE_BITS + 1 - MIN_RTNOBJ_SIZE_BITS)

#define	IS_INSERT		0
#define	IS_DELETE		1

error_def(ERR_REQRLNKCTLRNDWN);	/* needed for the ISSUE_REQRLNKCTLRNDWN_SYSCALL macro */

#define ISSUE_REQRLNKCTLRNDWN_SYSCALL(LINKCTL, ERRSTR, ERRNO)							\
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(13)								\
		ERR_REQRLNKCTLRNDWN, 3, LINKCTL->relinkctl_path, RTS_ERROR_MSTR(&LINKCTL->zro_entry_name), 	\
		ERR_SYSCALL, 5, LEN_AND_STR(ERRSTR), CALLFROM, DEBUG_ONLY(saved_errno = )ERRNO)

#define ISSUE_RELINKCTLERR_SYSCALL(ZRO_ENTRY_NAME, ERRSTR, ERRNO)						\
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(12) ERR_RELINKCTLERR, 2, RTS_ERROR_MSTR(ZRO_ENTRY_NAME),		\
		      ERR_SYSCALL, 5, LEN_AND_STR(ERRSTR), CALLFROM, DEBUG_ONLY(saved_errno = )ERRNO)

#define ISSUE_RELINKCTLERR_TEXT(ZRO_ENTRY_NAME, ERRORTEXT, ERRNO)						\
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_RELINKCTLERR, 2, RTS_ERROR_MSTR(ZRO_ENTRY_NAME),		\
		      ERR_TEXT, 2, LEN_AND_STR(ERRORTEXT), DEBUG_ONLY(saved_errno = )ERRNO)

typedef	gtm_uint64_t	rtnobj_sm_off_t;

/* Shared structure - relink record corresponding to a relinkctl file (resides in shared memory) */
typedef struct relinkrec_struct
{
	mident_fixed	rtnname_fixed;
	uint4		cycle;
	uint4		hashindex_fl;	/* Forward link ptr to linked list of relinkrec_t structures that have same rtnhash value */
	uint4		numvers;		/* Number of versions of this .o file currently in this relinkctl shared memory */
	uint4		filler_8byte_align;
	gtm_uint64_t	objLen;			/* Total object file length of various versions of this .o file in shared memory */
	gtm_uint64_t	usedLen;		/* Total length used up in shared memory for various versions of this .o file.
						 * Due to rounding up of objLen to nearest 2-power, usedLen >= objLen always.
						 */
	rtnobj_sm_off_t	rtnobj_shm_offset;	/* offset into shared memory where this object file can be found for linking.
						 * If object is not loaded, set to NULL_RTNOBJ_SM_OFF_T.
						 */
	global_latch_t	rtnobj_latch;	/* Lock used to search/insert/delete entries in the
					 * object file linked list (starting at rtnobj_shm_offset)
					 */
	gtm_uint64_t	objhash;	/* Hash of the object file last touched here */
	CACHELINE_PAD_COND(88, 1)	/* Add 40 bytes (on top of 88 bytes) to make this structure 128 bytes to avoid cacheline
					 * interference (between successive relinkrec_t structures in shared memory) on the RS6000
					 * (where the cacheline is 128 bytes currently). Other platforms have smaller
					 * cachelines (i.e. 64 bytes or lesser) and so this structure (88 bytes currently)
					 * taking up more than one cacheline dont require this padding.
					 */
} relinkrec_t;

#define	ZRO_DIR_PATH_MAX	MAX_FBUFF	/* since "zro_load" which parses $zroutines uses MAX_FBUFF */

/* Shared structure - relinkctl file header */
typedef struct relinkctl_data_struct
{
	uint4		n_records;
	int4		nattached;	/* Number of processes currently attached. this is approximate, because if a process
					 * is kill 9'd, nattached is not decrememented.
					 * If nattached is 0 upon exiting, we can remove the file.
					 * TODO: Provide fancier cleanup scheme for kill 9. two options:
					 * 	1. SYSV semaphore (as with the db). increment in open_relinkctl
					 * 	2. When we want to cleanup (say mupip routine -rundown), execute 'fuser'
					 */
	int4		relinkctl_shmid;/* ID of primary shared memory segment corresponding to the mmaped relinkctl file.
					 * This contains the array of relinkrec_t structures as well as hash buckets to
					 * speed up search of routine names.
					 */
	uint4		relinkctl_shmlen;/* size of shared memory */
	int4		file_deleted;	/* Way of signaling processes in relinkctl_open about a concurrent delete so they
					 * close their fd and reopen the file.
					 */
	uint4		initialized;	/* relinkctl file has been successfully initialized */
	char		zro_entry_name[ZRO_DIR_PATH_MAX + 1];	/* null-terminated full path of the directory in $zroutines
								 * whose relinkctl file is this. Given a relinkctl file, one can
								 * use this to find the corresponding directory.
								 */
	int		zro_entry_name_len;	/* strlen of the null-terminated "zro_entry_name" */
	int		relinkctl_max_rtn_entries;	/* One relinkctl file can contain at most this many of routines */
	int		relinkctl_hash_buckets;		/* The first prime # above relinkctl_max_rtn_entries */
} relinkctl_data;

/* Process private structure - describes a relinkctl file. Process private so can be linked into a list in $ZROUTINES order */
typedef struct open_relinkctl_struct
{
	struct open_relinkctl_struct	*next;			/* List of open ctl structures, sorted by zro_entry_name */
	mstr				zro_entry_name;		/* object directory name from $zroutines */
	char				*relinkctl_path;	/* full path of the relinkctl file corresponding to this objdir */
	uint4				n_records;		/* Private copy */
	boolean_t			locked;			/* TRUE if this process owns exclusive lock */
	relinkctl_data			*hdr;			/* Base of mapped file */
	relinkrec_t			*rec_base;
	sm_uint_ptr_t			shm_hashbase;		/* base of hash table in shared memory */
	sm_uc_ptr_t			rtnobj_shm_base[NUM_RTNOBJ_SHM_INDEX];
	int				rtnobj_shmid[NUM_RTNOBJ_SHM_INDEX];
	int				fd;
	int				rtnobj_min_shm_index;	/* Copied over from relinkshm_hdr->rtnobj_min_shm_index */
	int				rtnobj_max_shm_index;	/* Copied over from relinkshm_hdr->rtnobj_max_shm_index */
} open_relinkctl_sgm;

typedef struct rtnobjshm_hdr_struct
{
	que_ent		freeList[NUM_RTNOBJ_SIZE_BITS];
	int		rtnobj_min_free_index;	/* minimum 'i' where freeList[i] has non-zero fl,bl links */
	int		rtnobj_max_free_index;	/* maximum 'i' where freeList[i-1] has non-zero fl,bl links
						 * if no freeList[i] has non-zero fl,bl links, this will be set to  0.
						 */
	int		rtnobj_shmid;
	gtm_uint64_t	real_len;	/* sum of realLen of .o files used currently in this rtnobj shared memory segment */
	gtm_uint64_t	used_len;	/* sum of space occupied by .o files currently in this rtnobj shared memory segment */
	gtm_uint64_t	shm_len;	/* size of shared memory segment */
} rtnobjshm_hdr_t;

/* Shared memory header structure corresponding to relinkctl file. This is followed by a hash-array for speedy routine name
 * access.
 */
typedef struct relinkshm_hdr
{
	char		relinkctl_fname[GTM_PATH_MAX];	/* full path of the relinkctl file (mmr hash is in this name) */
	int		min_shm_index;
	int		rtnobj_min_shm_index;		/* Minimum 'i' where rtnobj_shmhdr[i].rtnobj_shmid is a valid shmid */
	int		rtnobj_max_shm_index;		/* Maximum 'i' where rtnobj_shmhdr[i-1].rtnobj_shmid is a valid shmid.
							 * If no rtnobj_shmhdr[i] has valid shmid, this will be set to 0.
							 */
	boolean_t	rndwn_adjusted_nattch;		/* MUPIP RUNDOWN -RELINKCTL did adjust nattached */
	boolean_t	skip_rundown_check;		/* TRUE if at least one process with gtm_autorelink_keeprtn=1 opened this */
	rtnobjshm_hdr_t	rtnobj_shmhdr[NUM_RTNOBJ_SHM_INDEX];
	/* CACHELINE_PAD macro usages surrounding the actual latch below provides spacing so updates to the latch do not interfere
	 * with updates to adjoining fields which can happen if they fall in the same data cacheline of a processor. No
	 * CACHELINE_PAD before the latch as adjoining fields before the latch are updated only if we hold the latch so no
	 * interference possible.
	 */
	global_latch_t	relinkctl_latch;	/* latch for insertions/deletions of buddy list structurs in any rtnobj shmids */
	CACHELINE_PAD(SIZEOF(global_latch_t), 1)
} relinkshm_hdr_t;

#define	STATE_FREE	0
#define	STATE_ALLOCATED	1

/* "refcnt" is a signed 4-byte integer so the max it can go to is 2**31-1.
 * Once it reaches this value, we can no longer accurately maintain refcnt (rollover issues).
 * So keep it there thereby never removing the corresponding object from shared memory.
 */
#define	REFCNT_INACCURATE	MAXPOSINT4

/* Header structure for each .o file in the "rtnobj" shared memory */
typedef struct rtnobj_hdr_struct
{
	unsigned short	queueIndex;			/* 2**queueIndex is the size of this element (including this header) */
	unsigned char	state;				/* State of this block */
	unsigned char	initialized;			/* Has the .o file been read from disk into this slot */
	int4		refcnt;				/* # of processes that are currently using this .o file */
	gtm_uint64_t	objhash;			/* 8-byte checksum of object file contents.
							 * Used to differentiate multiple versions of the .o file with the same
							 * routine name; Each gets a different routine buffer in shared memory.
							 */
	rtnobj_sm_off_t	next_rtnobj_shm_offset;		/* Offset into shared memory where the routine buffer of the .o file with
							 * the same name as this one can be found but with a different checksum.
							 * Basically a linked list. Null pointer set to NULL_RTNOBJ_SM_OFF_T.
							 */
	uint4		relinkctl_index;		/* Index into reclinkrec_t[] array where the rtnname corresponding to this
							 * .o file can be found; Note multiple versions of .o file with different
							 * <src_cksum_8byte,objLen> values will have different routine buffers in
							 * shared memory each of them pointing back to the same relinkctl_index.
							 */
	uint4		objLen;				/* Size of the allocated .o file. Currently not allowed to go > 4Gb.
							 * <src_cksum_8byte,objLen> together are used to differentiate multiple
							 * versions of the same .o file name; Each of them with a different value
							 * of either src_cksum_8byte or objLen gets a different routine buffer in
							 * shared memory. Note that it is theoretically possible (though rare) that
							 * two different .o files have the same 8-byte src checksum and same objLen.
							 * In that case they will use the same routine buffer. But since we expect
							 * this to be very rare in practice, we consider this acceptable for now.
							 */
	union
	{
		que_ent		freePtr;		/* Pointer to next and previous storage element on free queue */
		unsigned char	userStart;		/* First byte of user useable storage */
	} userStorage;
} rtnobj_hdr_t;

/*
 * Prototypes
 */
open_relinkctl_sgm	*relinkctl_attach(mstr *obj_container_name, mstr *objpath, int objpath_alloc_len);
void			relinkctl_incr_nattached(void);
int			relinkctl_get_key(char key[GTM_PATH_MAX], mstr *zro_entry_name);
relinkrec_t		*relinkctl_find_record(open_relinkctl_sgm *linkctl, mstr *rtnname, uint4 hash, uint4 *prev_hash_index);
relinkrec_t		*relinkctl_insert_record(open_relinkctl_sgm *linkctl, mstr *rtnname);
int			relinkctl_open(open_relinkctl_sgm *linkctl, boolean_t obj_file_missing);
void			relinkctl_lock_exclu(open_relinkctl_sgm *linkctl);
void			relinkctl_unlock_exclu(open_relinkctl_sgm *linkctl);
void			relinkctl_rundown(boolean_t decr_attached, boolean_t do_rtnobj_shm_free);

#endif /* RELINKCTL_H_INCLUDED */
