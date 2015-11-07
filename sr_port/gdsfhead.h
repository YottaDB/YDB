/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDSFHEAD_H_INCLUDED
#define GDSFHEAD_H_INCLUDED

/* gdsfhead.h */
/* this requires gdsroot.h gtm_facility.h fileinfo.h gdsbt.h */

#include <sys/types.h>
#include "gdsdbver.h"
#include "gtm_unistd.h"
#include "gtm_limits.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "send_msg.h"
#include "iosp.h"
#ifdef UNIX
#include "repl_instance.h"
#endif
#ifdef VMS
#include "iosb_disk.h"
#endif
#ifdef GTM_CRYPT
#include "gtmcrypt.h" /* for gtmcrypt_key_t */
#endif

#define CACHE_STATE_OFF SIZEOF(que_ent)

error_def(ERR_DBCRERR);
error_def(ERR_DBENDIAN);
error_def(ERR_DBFLCORRP);
error_def(ERR_GVIS);
error_def(ERR_GVSUBOFLOW);
error_def(ERR_MMFILETOOLARGE);
error_def(ERR_REPLINSTMISMTCH);
error_def(ERR_REPLREQROLLBACK);
error_def(ERR_SCNDDBNOUPD);
error_def(ERR_SRVLCKWT2LNG);
error_def(ERR_SSATTACHSHM);
error_def(ERR_SSFILOPERR);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);
error_def(ERR_TNTOOLARGE);
error_def(ERR_TNWARN);

/* Cache record */
typedef struct cache_rec_struct
{
	struct
	{
		sm_off_t 	fl;
		sm_off_t	bl;
	}
			blkque,		/* cache records whose block numbers hash to the same location */
			state_que;	/* cache records in same state (either wip or active) */
	union
	{
		short	semaphore;
		volatile int4	latch;		/* int required for atomic swap on Unix */
			/* volatile required as this value is referenced outside of the lock in db_csh_getn() */
	} interlock;
	block_id	blk;
	uint4		refer;		/* reference bit for the clock algorithm */
	enum db_ver	ondsk_blkver;	/* Actual block version from block header as it exists on disk
					   (prior to any dynamic conversion that may have occurred when read in).
					*/
	/* Keep our 64 bit fields up front */
	/* this point should be quad-word aligned */
	trans_num	dirty;		/* block has been modified since last written to disk; used by bt_put, db_csh_getn
					 * mu_rndwn_file wcs_recover, secshr_db_clnup, wr_wrtfin_all and extensively by the ccp */
	trans_num	flushed_dirty_tn;	/* value of dirty at the time of flushing */
	trans_num	tn;
	sm_off_t	bt_index;	/* offset to bt_rec */
	sm_off_t	buffaddr;	/* offset to buffer holding actual data*/
	sm_off_t	twin;		/* (VMS) offset to cache_rec of another copy of the same block from bg_update & wcs_wt_all
					 * (Unix & VMS) offset to cache_rec holding before-image for wcs_recover to backup */
#ifdef VMS
	sm_off_t	shmpool_blk_off; /* Offset to shmpool block containing the reformat buffer for this CR */
	int4		backup_cr_off;   /* Offset to backup_cr (set/used by bg_update_phase1/2 routines) */
#endif
	off_jnl_t	jnl_addr;	/* offset from bg_update to prevent wcs_wtstart from writing a block ahead of the journal */
	global_latch_t	rip_latch;	/* for read_in_progress - note contains extra 16 bytes for HPPA. Usage note: this
					   latch is used on those platforms where read_in_progress is not directly updated
					   by atomic routines/instructions. As such there needs be no cache line padding between
					   this field and read_in_progress.
					 */
	/* and now the rest */
	int4		image_count;	/* maintained with r_epid in vms to ensure that the process has stayed in gt.m */
	int4		epid;		/* set by wcs_wtstart to id the write initiator; cleared by wcs_wtfini
					 * used by t_commit_cleanup, secshr_db_clnup and wcs_recover */
	int4		cycle;		/* relative stamp indicates changing versions of the block for concurrency checking */
	int4		r_epid;		/* set by db_csh_getn, cleared by t_qread, bg_update, wcs_recover or secshr_db_clnup
					 * used to check for process leaving without releasing the buffer
					 * must be word aligned on the VAX */
#ifdef VMS
	io_status_block_disk	iosb;	/* used on VMS write */
#endif
	CNTR4DCL(read_in_progress, 10);	/* -1 for normal and 0 for rip used by t_qread and checked by others */
	uint4		in_tend;	/* non-zero pid from bg_update indicates secshr_db_clnup should finish update */
	uint4		in_cw_set;	/* non-zero pid from t_end, tp_tend or bg_update protects block from db_csh_getn;
					 * returned to 0 by t_end, tp_tend or t_commit_cleanup */
	uint4		data_invalid;	/* non-zero pid from bg_update indicates t_commit_cleanup/wcs_recover should invalidate */
	boolean_t	stopped;	/* TRUE indicates to wcs_recover that secshr_db_clnup built the block */
	boolean_t	wip_stopped;	/* TRUE indicates to wcs_recover, wcs_wtfini, wcs_get_blk and gds_rundown
					 * that secshr_db_clnup cancelled the qio */
} cache_rec;

/* A note about cache line separation of the latches contained in these blocks. Because this block is duplicated
   many (ptentially tens+ of) thousands of times in a running system, we have decided against providing cacheline
   padding so as to force each cache record into a separate cacheline (due to it containing a latch and/or atomic
   counter field) to prevent processes from causing interference with each other. We decided that the probability
   of two processes working on adjacent cache records simultaneously was low enough that the interference was
   minimal whereas increasing the cache record size to prevent that interference could cause storage problems
   on some platforms where processes are already running near the edge.
*/

/* cache_state record */
typedef struct
{
	struct
	{
		sm_off_t 	fl;
		sm_off_t	bl;
	}
			state_que;	/* WARNING from this point, this structure must be identical to a cache_rec */
	union
	{
		short	semaphore;
		volatile int4	latch;		/* int required for atomic swap on Unix */
			/* volatile required as this value is referenced outside of the lock in db_csh_getn() */
	} interlock;
	block_id	blk;
	uint4		refer; 		/* reference bit for the LRU algorithm */
	enum db_ver	ondsk_blkver;	/* Actual block version from block header as it exists on disk
					   (prior to any dynamic conversion that may have occurred when read in).
					*/
	/* Keep our 64 bit fields up front */
	/* this point should be quad-word aligned */
	trans_num	dirty;		/* block has been modified since last written to disk; used by bt_put, db_csh_getn
					 * mu_rndwn_file wcs_recover, secshr_db_clnup, wr_wrtfin_all and extensively by the ccp */
	trans_num	flushed_dirty_tn;	/* value of dirty at the time of flushing */
	trans_num	tn;
	sm_off_t	bt_index;	/* offset to bt_rec */
	sm_off_t	buffaddr;	/* offset to buffer holding actual data*/
	sm_off_t	twin;		/* (VMS) offset to cache_rec of another copy of the same block from bg_update & wcs_wt_all
					 * (Unix & VMS) offset to cache_rec holding before-image for wcs_recover to backup */
#ifdef VMS
	sm_off_t	shmpool_blk_off; /* Offset to shmpool block containing the reformat buffer for this CR */
	int4		backup_cr_off;   /* Offset to backup_cr (set/used by bg_update_phase1/2 routines) */
#endif
	off_jnl_t	jnl_addr;	/* offset from bg_update to prevent wcs_wtstart from writing a block ahead of the journal */
	global_latch_t	rip_latch;	/* for read_in_progress - note contains extra 16 bytes for HPPA. Usage note: this
					   latch is used on those platforms where read_in_progress is not directly updated
					   by atomic routines/instructions. As such there needs be no cache line padding between
					   this field and read_in_progress.
					 */
	/* and now the rest */
	int4		image_count;	/* maintained with r_epid in vms to ensure that the process has stayed in gt.m */
	int4		epid;		/* set by wcs_start to id the write initiator; cleared by wcs_wtfini
					 * used by t_commit_cleanup, secshr_db_clnup and wcs_recover */
	int4		cycle;		/* relative stamp indicates changing versions of the block for concurrency checking */
	int4		r_epid;		/* set by db_csh_getn, cleared by t_qread, bg_update, wcs_recover or secshr_db_clnup
					 * used to check for process leaving without releasing the buffer
					 * must be word aligned on the VAX */
#ifdef VMS
	io_status_block_disk	iosb;	/* used on VMS write */
#endif
	CNTR4DCL(read_in_progress, 10);	/* -1 for normal and 0 for rip used by t_qread and checked by others */
	uint4		in_tend;	/* non-zero pid from bg_update indicates secshr_db_clnup should finish update */
	uint4		in_cw_set;	/* non-zero pid from t_end, tp_tend or bg_update protects block from db_csh_getn;
					 * returned to 0 by t_end, tp_tend or t_commit_cleanup */
	uint4		data_invalid;	/* non-zero pid from bg_update indicates t_commit_cleanup/wcs_recover should invalidate */
	boolean_t	stopped;	/* TRUE indicates to wcs_recover that secshr_db_clnup built the block */
	boolean_t	wip_stopped;	/* TRUE indicates to wcs_recover, wcs_wtfini, wcs_get_blk and gds_rundown
					 * that secshr_db_clnup cancelled the qio */
} cache_state_rec;

#define		CR_BLKEMPTY		-1
#define		MBR_BLKEMPTY		-1
#define		FROZEN_BY_ROOT		(uint4)(0xFFFFFFFF)
#define		BACKUP_NOT_IN_PROGRESS	0x7FFFFFFF
#define		DB_CSH_RDPOOL_SZ	0x20	/* These many non-dirty buffers exist at all points in time in shared memory */

typedef struct
{
	cache_que_head	cacheq_wip,	/* write-in-progress queue -- unused in Unix */
			cacheq_active;	/* active queue */
	cache_rec	cache_array[1];	/*the first cache record*/
} cache_que_heads;

/* Define pointer types to some previously defined structures */

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef cache_que_head	*cache_que_head_ptr_t;
typedef cache_rec 	*cache_rec_ptr_t;
typedef cache_rec	**cache_rec_ptr_ptr_t;
typedef cache_state_rec	*cache_state_rec_ptr_t;
typedef cache_que_heads	*cache_que_heads_ptr_t;

void verify_queue_lock(que_head_ptr_t qhdr);
void verify_queue(que_head_ptr_t qhdr);

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

#ifdef DEBUG_QUEUE
#define VERIFY_QUEUE(base)      	verify_queue(base)
#define VERIFY_QUEUE_LOCK(base,latch)	verify_queue_lock(base,latch)
#else
#define VERIFY_QUEUE(base)
#define VERIFY_QUEUE_LOCK(base,latch)
#endif

#define BLK_ZERO_OFF(CSD)			((CSD->start_vbn - 1) * DISK_BLOCK_SIZE)
#ifdef UNIX
# ifdef GTM64
#  define CHECK_LARGEFILE_MMAP(REG, MMAP_SZ)
# else
#  define CHECK_LARGEFILE_MMAP(REG, MMAP_SZ)										\
{															\
	assert(SIZEOF(gtm_uint64_t) == SIZEOF(MMAP_SZ));								\
	assert(0 < MMAP_SZ);												\
	if (MAXUINT4 < (gtm_uint64_t)(MMAP_SZ))										\
		rts_error_csa(CSA_ARG(REG2CSA(REG)) VARLSTCNT(6) ERR_MMFILETOOLARGE, 4, REG_LEN_STR(REG),		\
				DB_LEN_STR(REG));									\
}
# endif
# define MMAP_FD(FD, SIZE, OFFSET, READ_ONLY)	mmap((caddr_t)NULL, SIZE, MM_PROT_FLAGS(READ_ONLY), GTM_MM_FLAGS, FD, OFFSET)
# define MSYNC(BEGPTR, ENDPTR)			(BEGPTR ? DBG_ASSERT(BEGPTR < ENDPTR) msync(BEGPTR, (ENDPTR - BEGPTR), MS_SYNC)	\
							: 0)
# define MM_PROT_FLAGS(READ_ONLY)		(READ_ONLY ? PROT_READ : (PROT_READ | PROT_WRITE))
# define MM_BASE_ADDR(CSA) 			(sm_uc_ptr_t)CSA->db_addrs[0]
# define SET_MM_BASE_ADDR(CSA, CSD)
#else
# define MM_BASE_ADDR(CSA)			(sm_uc_ptr_t)CSA->acc_meth.mm.base_addr
# define SET_MM_BASE_ADDR(CSA, CSD)											\
{															\
	CSA->acc_meth.mm.base_addr = (sm_uc_ptr_t)((sm_long_t)CSD + (off_t)(CSD->start_vbn - 1) * DISK_BLOCK_SIZE);	\
}
#endif

/* The following 3 macros were introduced while solving a problem with $view where a call to $view in */
/* mumps right after a change to $zgbldir gave the old global directory - not the new one.  On VMS it */
/* caused a core dump.  If one were to access a global variable via $data right after the change, however, */
/* the $view worked correctly.  The solution was to make sure the gd_map information matched the current */
/* gd_header in op_fnview.c.  The code used as a template for this change was in gvinit.c.  The first */
/* macro gets the gd_header using an mval.  The second macro establishes the gd_map from the gd_header. */
/* The third macro is an assert (when DEBUG_ONLY is defined) for those cases where the gd_header is already */
/* set to make sure the mapping is correct. The first 2 macros are executed when the gd_header is null, */
/* and the 3rd macro is associated with an else clause if it is not.  Therefore, they should be maintained */
/* as a group. */

#define SET_GD_HEADER(inmval)				\
{							\
	inmval.mvtype = MV_STR;				\
	inmval.str.len = 0;				\
	gd_header = zgbldir(&inmval);			\
}

#define SET_GD_MAP					\
{							\
	GBLREF	gd_binding	*gd_map, *gd_map_top;	\
							\
	gd_map = gd_header->maps;			\
	gd_map_top = gd_map + gd_header->n_maps;	\
	TREF(gd_targ_addr) = gd_header;			\
}

#define GD_HEADER_ASSERT					\
{								\
	GBLREF	gd_binding	*gd_map, *gd_map_top;		\
								\
	assert(gd_map == gd_header->maps);			\
	assert(gd_map_top == gd_map + gd_header->n_maps);	\
	assert(TREF(gd_targ_addr) == gd_header);		\
}

/* If reallocating gv_currkey/gv_altkey, preserve pre-existing values */
#define	GVKEY_INIT(GVKEY, KEYSIZE)						\
{										\
	gv_key		*new_KEY, *old_KEY;					\
	int4		keySZ;							\
										\
	old_KEY = GVKEY;							\
	keySZ = KEYSIZE;							\
	/* KEYSIZE should have been the output of a DBKEYSIZE command so	\
	 * should be a multiple of 4. Assert that.				\
	 */									\
	assert(ROUND_UP2(keySZ, 4) == keySZ);					\
	new_KEY = (gv_key *)malloc(SIZEOF(gv_key) - 1 + keySZ);			\
	if (NULL != old_KEY)							\
	{									\
		assert(KEYSIZE >= old_KEY->top);				\
		assert(old_KEY->top > old_KEY->end);				\
		memcpy(new_KEY, old_KEY, SIZEOF(gv_key) + old_KEY->end);	\
		free(old_KEY);							\
	} else									\
	{									\
		new_KEY->base[0] = '\0';					\
		new_KEY->end = 0;						\
		new_KEY->prev = 0;						\
	}									\
	new_KEY->top = keySZ;							\
	GVKEY = new_KEY;							\
}

#define	GVKEY_FREE_IF_NEEDED(GVKEY)	\
{					\
	if (NULL != GVKEY)		\
	{				\
		free(GVKEY);		\
		GVKEY = NULL;		\
	}				\
}

#define	GVKEYSIZE_INCREASE_IF_NEEDED(KEYSIZE)							\
{												\
	int		keySIZE;								\
												\
	GBLREF int4	gv_keysize;								\
	GBLREF gv_key	*gv_altkey;								\
	GBLREF gv_key	*gv_currkey;								\
												\
	keySIZE = KEYSIZE;									\
	/* Have space to store at least MAX_MIDENT_LEN bytes as otherwise name-level $order	\
	 * (see op_gvorder/op_zprevious) could have buffer overflow issues in gv_currkey->base.	\
	 * Do ROUND_UP2(x,4) to keep an assert in GVKEY_INIT macro happy.			\
	 */											\
	if ((MAX_MIDENT_LEN + 3) > keySIZE)							\
		keySIZE = ROUND_UP2(MAX_MIDENT_LEN + 3, 4);					\
	assert(keySIZE);									\
	if (keySIZE > gv_keysize)								\
	{											\
		gv_keysize = keySIZE;								\
		GVKEY_INIT(gv_currkey, keySIZE);						\
		GVKEY_INIT(gv_altkey, keySIZE);							\
	} else											\
		assert((NULL != gv_currkey) && (NULL != gv_altkey) && gv_keysize		\
			&& (gv_keysize == gv_currkey->top) && (gv_keysize == gv_altkey->top));	\
}

#define WAS_OPEN_TRUE		TRUE
#define WAS_OPEN_FALSE		FALSE

/* Below macro sets open, opening and was_open fields of a given region after the corresponding
 * database for that region is opened. Also, if the region was not already open, the macro
 * invokes GVKEYSIZE_INCREASE_IF_NEEDED to allocate gv_currkey/gv_altkey based on the region's
 * max_key_size.
 */
#define SET_REGION_OPEN_TRUE(REG, WAS_OPEN)							\
{												\
	DEBUG_ONLY(GBLREF int4	gv_keysize;)							\
												\
	assert(!REG->was_open);									\
	assert(!REG->open);									\
	REG->open = TRUE;									\
	REG->opening = FALSE;									\
	if (WAS_OPEN)										\
	{											\
		REG->was_open = TRUE;								\
		assert(DBKEYSIZE(REG->max_key_size) <= gv_keysize);				\
	}											\
	else											\
		GVKEYSIZE_INCREASE_IF_NEEDED(DBKEYSIZE(REG->max_key_size));			\
}

#define	SET_CSA_DIR_TREE(csa, keysize, reg)							\
{												\
	if (NULL == csa->dir_tree)								\
	{											\
		csa->dir_tree = targ_alloc(keysize, NULL, reg);					\
		GTMTRIG_ONLY(assert(NULL == csa->hasht_tree));					\
	} else											\
		assert((csa->dir_tree->gd_csa == csa) && (DIR_ROOT == csa->dir_tree->root));	\
}

#define	FREE_CSA_DIR_TREE(csa)							\
{										\
	sgmnt_addrs	*lcl_csa;						\
	gv_namehead	*dir_tree, *hasht_tree;					\
										\
	lcl_csa = csa;								\
	GTMTRIG_ONLY(								\
		hasht_tree = lcl_csa->hasht_tree;				\
		if (NULL != hasht_tree)						\
		{								\
			assert(hasht_tree->gd_csa == csa);			\
			hasht_tree->regcnt--;	/* targ_free relies on this */	\
			targ_free(hasht_tree);					\
			lcl_csa->hasht_tree = NULL;				\
		}								\
	)									\
	dir_tree = lcl_csa->dir_tree;						\
	assert(NULL != dir_tree);						\
	dir_tree->regcnt--;	/* targ_free relies on this */			\
	targ_free(dir_tree);							\
	lcl_csa->dir_tree = NULL;						\
}

#define	PROCESS_GVT_PENDING_LIST(GREG, CSA, GVT_PENDING_LIST)						\
{													\
	if (NULL != GVT_PENDING_LIST)									\
	{	/* Now that the region has been opened, check if there are any gv_targets that were	\
		 * allocated for this region BEFORE the open. If so, re-allocate them if necessary.	\
		 */											\
		process_gvt_pending_list(GREG, CSA);							\
	}												\
}

#define		T_COMMIT_CRIT_PHASE1	1	/* csa->t_commit_crit gets set to this in during bg_update_phase1 */
#define		T_COMMIT_CRIT_PHASE2	2	/* csa->t_commit_crit gets set to this in during bg_update_phase2 */

/* macro to check if we hold crit or are committing (with or without crit) */
#define		T_IN_CRIT_OR_COMMIT(CSA)	((CSA)->now_crit || (CSA)->t_commit_crit)

/* Macro to check if we hold crit or are committing (with or without crit) or are in wcs_wtstart for this region.
 * This is used in timer handling code to determine if it is ok to interrupt. We do not want to interrupt if holding
 * crit or in the midst of commit or in wcs_wtstart (in the last case, we could be causing another process HOLDING CRIT
 * on the region to wait in bg_update_phase1 if we hold the write interlock).
 */
#define		T_IN_CRIT_OR_COMMIT_OR_WRITE(CSA)	(T_IN_CRIT_OR_COMMIT(CSA) || (CSA)->in_wtstart)

/* macro to check if a database commit is past the point where it can be successfully rolled back */
#define		T_UPDATE_UNDERWAY(CSA)	((CSA)->t_commit_crit)

/* the file header has relative pointers to its data structures so each process will malloc
 * one of these and fill it in with absolute pointers upon file initialization.
 */
#define GDS_REL2ABS(x)	(((sm_uc_ptr_t)cs_addrs->lock_addrs[0] + (sm_off_t)(x)))
#define GDS_ABS2REL(x) (sm_off_t)(((sm_uc_ptr_t)(x) - (sm_uc_ptr_t)cs_addrs->lock_addrs[0]))
#define GDS_ANY_REL2ABS(w,x) (((sm_uc_ptr_t)(w->lock_addrs[0]) + (sm_off_t)(x)))
#define GDS_ANY_ABS2REL(w,x) (sm_off_t)(((sm_uc_ptr_t)(x) - (sm_uc_ptr_t)w->lock_addrs[0]))
#ifdef GTM_CRYPT
#define GDS_ANY_ENCRYPTGLOBUF(w,x) ((sm_uc_ptr_t)(w) + (sm_off_t)(x->nl->encrypt_glo_buff_off))
#endif
#define	ASSERT_IS_WITHIN_SHM_BOUNDS(ptr, csa)											\
	assert((NULL == (ptr)) || (((ptr) >= csa->db_addrs[0]) && ((0 == csa->db_addrs[1]) || ((ptr) < csa->db_addrs[1]))))

#ifdef DEBUG
#define DBG_ENSURE_PTR_IS_VALID_GLOBUFF(CSA, CSD, PTR)					\
{											\
	cache_rec_ptr_t			cache_start;					\
	long				bufindx;					\
	sm_uc_ptr_t			bufstart;					\
											\
	cache_start = &(CSA)->acc_meth.bg.cache_state->cache_array[0];			\
	cache_start += CSD->bt_buckets;							\
	bufstart = (sm_uc_ptr_t)GDS_ANY_REL2ABS((CSA), cache_start->buffaddr);		\
	assert((PTR) >= bufstart);							\
	bufindx = (PTR - bufstart) / CSD->blk_size;					\
	assert(bufindx < CSD->n_bts);							\
	assert((bufstart + (bufindx * CSD->blk_size)) == (PTR));			\
}
#define DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(CSA, CSD, PTR)				\
{											\
	cache_rec_ptr_t			cache_start;					\
	long				bufindx;					\
	sm_uc_ptr_t			bufstart;					\
											\
	cache_start = &(CSA)->acc_meth.bg.cache_state->cache_array[0];			\
	cache_start += CSD->bt_buckets;							\
	bufstart = (sm_uc_ptr_t)GDS_ANY_REL2ABS((CSA), cache_start->buffaddr);		\
	bufstart += (gtm_uint64_t)CSD->blk_size * CSD->n_bts;					\
	assert((PTR) >= bufstart);							\
	bufindx = (PTR - bufstart) / CSD->blk_size;					\
	assert(bufindx < CSD->n_bts);							\
	assert((bufstart + (bufindx * (gtm_uint64_t)CSD->blk_size)) == (PTR));			\
}

#define	DBG_ENSURE_OLD_BLOCK_IS_VALID(cse, is_mm, csa, csd)								\
{															\
	cache_rec_ptr_t		cache_start;										\
	long			bufindx;										\
	sm_uc_ptr_t		bufstart;										\
	GBLREF	boolean_t	dse_running, write_after_image;								\
															\
	assert((gds_t_write != cse->mode) && (gds_t_write_recycled != cse->mode) && gds_t_writemap != cse->mode		\
		|| (NULL != cse->old_block));	/* don't miss writing a PBLK */						\
	if (NULL != cse->old_block)											\
	{														\
		if (!is_mm)												\
		{													\
			cache_start = &csa->acc_meth.bg.cache_state->cache_array[0];					\
			cache_start += csd->bt_buckets;									\
			bufstart = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cache_start->buffaddr);				\
			bufindx = (cse->old_block - bufstart) / csd->blk_size;						\
			assert(bufindx < csd->n_bts);									\
			assert(cse->blk == cache_start[bufindx].blk);							\
			assert(dse_running || write_after_image || (process_id == cache_start[bufindx].in_cw_set));	\
		} else													\
			assert(cse->old_block == MM_BASE_ADDR(csa) + (off_t)cse->blk * csd->blk_size);			\
	}														\
}

/* Check if a given address corresponds to a global buffer (BG) in database shared memory AND if
 * we are in phase2 of commit. If so check whether the corresponding cache-record is pinned.
 * Used by gvcst_blk_build to ensure the update array points to valid contents even though we dont hold crit.
 */
#define	DBG_BG_PHASE2_CHECK_CR_IS_PINNED(csa, seg)								\
{														\
	cache_rec_ptr_t		cache_start;									\
	long			bufindx;									\
	sm_uc_ptr_t		bufstart, bufend, bufaddr;							\
														\
	GBLREF	uint4		process_id;									\
														\
	if ((seg)->len && (T_COMMIT_CRIT_PHASE2 == csa->t_commit_crit) && (dba_bg == csa->hdr->acc_meth))	\
	{													\
		cache_start = &csa->acc_meth.bg.cache_state->cache_array[0];					\
		cache_start += csa->hdr->bt_buckets;								\
		bufstart = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cache_start->buffaddr);				\
		bufend = bufstart + ((gtm_uint64_t)csa->hdr->n_bts * csa->hdr->blk_size);					\
		bufaddr = (sm_uc_ptr_t)(seg)->addr;								\
		/* Check if given address is within database shared memory range */				\
		if ((bufaddr >= bufstart) && (bufaddr < bufend))						\
		{												\
			bufindx = (bufaddr - bufstart) / csa->hdr->blk_size;					\
			assert(bufindx < csa->hdr->n_bts);							\
			/* Assert that we have the cache-record pinned */					\
			assert(process_id == cache_start[bufindx].in_cw_set);					\
		}												\
	}													\
}

/* Macro to check that we have not pinned any more buffers than we are updating.
 * This check is done only for BG access method and in dbg mode.
 * This is invoked by t_end/tp_tend just before beginning phase2 of commit.
 */
#define	DBG_CHECK_PINNED_CR_ARRAY_CONTENTS(is_mm, crarray, crarrayindex, bplmap)				\
{														\
	GBLREF	boolean_t	write_after_image;								\
														\
	if (!is_mm)												\
	{													\
		int4		crindex;									\
														\
		for (crindex = 0; crindex < crarrayindex; crindex++)						\
		{												\
			if (process_id == crarray[crindex]->in_cw_set)						\
			{	/* We have pinned that cache-record implies we are planning on updating it	\
				 * (so should have set in_tend).						\
				 *										\
				 * Since bitmap blocks are done with phase2 inside of crit, they should not	\
				 * show up in the pinned array list at end of phase1 for GT.M. But DSE is an	\
				 * exception as it could operate on a bitmap block as if it is updating a	\
				 * non-bitmap block (i.e. without invoking gvcst_map_build). MUPIP JOURNAL	\
				 * RECOVER also could do the same thing while applying an AIMG record.		\
				 * 										\
				 * In addition, VMS has an exception in case this is a twinned cache-record.	\
				 * In that case, for the older twin in_cw_set will be set to non-zero, but	\
				 * in_tend will be set to FALSE. Since we are outside of crit at this point,	\
				 * it is possible cr->twin field might be 0 (could have gotten cleared by	\
				 * wcs_wtfini concurrently) so we cannot assert on the twin field but		\
				 * cr->bt_index should still be 0 since we have not yet finished the		\
				 * update on the newer twin so we can check on that.				\
				 */										\
				assert(crarray[crindex]->in_tend						\
					&& ((0 != crarray[crindex]->blk % bplmap) || write_after_image)		\
					VMS_ONLY(|| !crarray[crindex]->bt_index));				\
			}											\
		}												\
	}													\
}

#else
#define DBG_ENSURE_PTR_IS_VALID_GLOBUFF(CSA, CSD, PTR)
#define DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(CSA, CSD, PTR)
#define DBG_ENSURE_OLD_BLOCK_IS_VALID(cse, is_mm, csa, csd)
#define DBG_BG_PHASE2_CHECK_CR_IS_PINNED(csa, bufaddr)
#define	DBG_CHECK_PINNED_CR_ARRAY_CONTENTS(is_mm, crarray, crarrayindex, bplmap)
#endif

/* The TP_CHANGE_REG macro is a replica of the tp_change_reg() routine to be used for performance considerations.
 * The TP_CHANGE_REG_IF_NEEDED macro tries to optimize on processing if reg is same as gv_cur_region. But it can be
 *	used only if the region passed is not NULL and if gv_cur_region, cs_addrs and cs_data are known to be in sync.
 * Note that timers can interrupt the syncing and hence any routines that are called by timers should be safe
 *	and use the TP_CHANGE_REG macro only.
 */
#define	TP_CHANGE_REG(reg)									\
{												\
	gv_cur_region = reg;									\
	if (NULL == gv_cur_region || FALSE == gv_cur_region->open)				\
	{											\
		cs_addrs = (sgmnt_addrs *)0;							\
		cs_data = (sgmnt_data_ptr_t)0;							\
	} else											\
	{											\
		switch (reg->dyn.addr->acc_meth)						\
		{										\
			case dba_mm:								\
			case dba_bg:								\
				cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;			\
				cs_data = cs_addrs->hdr;					\
				break;								\
			case dba_usr:								\
			case dba_cm:								\
				cs_addrs = (sgmnt_addrs *)0;					\
				cs_data = (sgmnt_data_ptr_t)0;					\
				break;								\
			default:								\
				GTMASSERT;							\
				break;								\
		}										\
	}											\
}

#define	TP_CHANGE_REG_IF_NEEDED(reg)								\
{												\
	assert(reg);										\
	if (reg != gv_cur_region)								\
	{											\
		gv_cur_region = reg;								\
		switch (reg->dyn.addr->acc_meth)						\
		{										\
			case dba_mm:								\
			case dba_bg:								\
				assert(reg->open);						\
				cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;			\
				cs_data = cs_addrs->hdr;					\
				break;								\
			case dba_usr:								\
			case dba_cm:								\
				cs_addrs = (sgmnt_addrs *)0;					\
				cs_data = (sgmnt_data_ptr_t)0;					\
				break;								\
			default:								\
				GTMASSERT;							\
				break;								\
		}										\
	}											\
	assert(&FILE_INFO(gv_cur_region)->s_addrs == cs_addrs && cs_addrs->hdr == cs_data);	\
}

#define PUSH_GV_CUR_REGION(reg, sav_reg, sav_cs_addrs, sav_cs_data)				\
{												\
	sav_reg = gv_cur_region;								\
	sav_cs_addrs = cs_addrs;								\
	sav_cs_data = cs_data;									\
	TP_CHANGE_REG(reg)									\
}

#define POP_GV_CUR_REGION(sav_reg, sav_cs_addrs, sav_cs_data)					\
{												\
	gv_cur_region = sav_reg;								\
	cs_addrs = sav_cs_addrs;								\
	cs_data = sav_cs_data;									\
}

/* The TP_TEND_CHANGE_REG macro is a special macro used in tp_tend.c to optimize out the unnecessary checks in
 * the TP_CHANGE_REG_IF_NEEDED macro. Also it sets cs_addrs and cs_data to precomputed values instead of recomputing
 * them from the region by dereferencing through a multitude of pointers. It does not check if gv_cur_region is
 * different from the input region. It assumes it is different enough % of times that the cost of the if check
 * is not worth the additional unconditional sets.
 */
#define	TP_TEND_CHANGE_REG(si)				\
{							\
	gv_cur_region = si->gv_cur_region;		\
	cs_addrs = si->tp_csa;				\
	cs_data = si->tp_csd;				\
}

#define	GTCM_CHANGE_REG(reghead)										\
{														\
	GBLREF cm_region_head	*curr_cm_reg_head;								\
	GBLREF gd_region	*gv_cur_region;									\
	GBLREF sgmnt_data	*cs_data;									\
	GBLREF sgmnt_addrs	*cs_addrs;									\
														\
	curr_cm_reg_head = (reghead);										\
	gv_cur_region = curr_cm_reg_head->reg;									\
	if ((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth))	\
	{													\
		cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;							\
		cs_data = cs_addrs->hdr;									\
	} else													\
		GTMASSERT;											\
}

/* Macro to be used whenever cr->data_invalid needs to be set */
#define	SET_DATA_INVALID(cr)										\
{													\
	uint4	in_tend, data_invalid;									\
													\
	DEBUG_ONLY(in_tend = cr->in_tend);								\
	DEBUG_ONLY(data_invalid = cr->data_invalid);							\
	assert((process_id == in_tend) || (0 == in_tend) && (0 == data_invalid));			\
	assert((0 == in_tend)										\
		|| (process_id == in_tend) && ((0 == data_invalid) || (process_id == data_invalid)));	\
	cr->data_invalid = process_id;									\
}

/* Macro to be used whenever cr->data_invalid needs to be re-set */
#define	RESET_DATA_INVALID(cr)				\
{							\
	uint4	data_invalid;				\
							\
	DEBUG_ONLY(data_invalid = cr->data_invalid);	\
	assert(process_id == data_invalid);		\
	cr->data_invalid = 0;				\
}

/* Macro to be used whenever cr->in_cw_set needs to be set (PIN) inside a TP transaction */
#define	TP_PIN_CACHE_RECORD(cr, si)					\
{									\
	assert(0 <= si->cr_array_index);				\
	assert(si->cr_array_index < si->cr_array_size);			\
	PIN_CACHE_RECORD(cr, si->cr_array, si->cr_array_index);		\
}

GBLREF	cache_rec_ptr_t	pin_fail_cr;			/* Pointer to the cache-record that we failed while pinning */
GBLREF	cache_rec	pin_fail_cr_contents;		/* Contents of the cache-record that we failed while pinning */
GBLREF	cache_rec_ptr_t	pin_fail_twin_cr;		/* Pointer to twin of the cache-record that we failed to pin */
GBLREF	cache_rec	pin_fail_twin_cr_contents;	/* Contents of twin of the cache-record that we failed to pin */
GBLREF	bt_rec_ptr_t	pin_fail_bt;			/* Pointer to bt of the cache-record that we failed to pin */
GBLREF	bt_rec		pin_fail_bt_contents;		/* Contents of bt of the cache-record that we failed to pin */
GBLREF	int4		pin_fail_in_crit;		/* Holder of crit at the time we failed to pin */
GBLREF	int4		pin_fail_wc_in_free;		/* Number of write cache records in free queue when we failed to pin */
GBLREF	int4		pin_fail_wcs_active_lvl;	/* Number of entries in active queue when we failed to pin */
GBLREF	int4		pin_fail_ref_cnt;		/* Reference count when we failed to pin */
GBLREF	int4		pin_fail_in_wtstart;		/* Count of processes in wcs_wtstart when we failed to pin */
GBLREF	int4		pin_fail_phase2_commit_pidcnt;	/* Number of processes in phase2 commit when we failed to pin */
/* Macro to be used whenever cr->in_cw_set needs to be set (PIN) outside of a TP transaction */
#define	PIN_CACHE_RECORD(cr, crarray, crarrayindex)							\
{													\
	uint4	in_tend, data_invalid, in_cw_set;							\
													\
	DEBUG_ONLY(in_tend = cr->in_tend);								\
	DEBUG_ONLY(data_invalid = cr->data_invalid);							\
	assert((process_id == in_tend) || (0 == in_tend));						\
	assert((process_id == data_invalid) || (0 == data_invalid));					\
	in_cw_set = cr->in_cw_set;									\
	if (0 != in_cw_set)										\
	{												\
		pin_fail_cr = cr;									\
		pin_fail_cr_contents = *cr;								\
		if (cr->bt_index)									\
		{											\
			pin_fail_bt = (bt_rec_ptr_t)GDS_ANY_REL2ABS(cs_addrs, cr->bt_index);		\
			pin_fail_bt_contents = *pin_fail_bt;						\
		}											\
		if (cr->twin)										\
		{											\
			pin_fail_twin_cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(cs_addrs, cr->twin);	\
			pin_fail_twin_cr_contents = *pin_fail_twin_cr;					\
		}											\
		pin_fail_in_crit		= cs_addrs->nl->in_crit;				\
		pin_fail_wc_in_free		= cs_addrs->nl->wc_in_free;				\
		pin_fail_wcs_active_lvl		= cs_addrs->nl->wcs_active_lvl;				\
		pin_fail_ref_cnt		= cs_addrs->nl->ref_cnt;				\
		pin_fail_in_wtstart		= cs_addrs->nl->in_wtstart;				\
		pin_fail_phase2_commit_pidcnt	= cs_addrs->nl->wcs_phase2_commit_pidcnt;		\
		GTMASSERT;										\
	}												\
	/* In VMS we should never set in_cw_set on an OLDER twin. */					\
	VMS_ONLY(assert(!cr->twin || cr->bt_index));							\
	/* stuff it in the array before setting in_cw_set */						\
	crarray[crarrayindex] = cr;									\
	crarrayindex++;											\
	cr->in_cw_set = process_id;									\
}

/* Macro to be used whenever cr->in_cw_set needs to be re-set (UNPIN) in TP or non-TP) */
#define	UNPIN_CACHE_RECORD(cr)								\
{											\
	uint4	in_tend, data_invalid, in_cw_set;					\
											\
	in_cw_set = cr->in_cw_set;							\
	if (process_id == cr->in_cw_set) /* reset in_cw_set only if we hold it */	\
	{										\
		DEBUG_ONLY(in_tend = cr->in_tend);					\
		DEBUG_ONLY(data_invalid = cr->data_invalid);				\
		assert((process_id == in_tend) || (0 == in_tend));			\
		assert((process_id == data_invalid) || (0 == data_invalid));		\
		cr->in_cw_set = 0;							\
	}										\
}

/* Macro to reset cr->in_cw_set for the entire cr_array in case of a retry (TP or non-TP) */
#define	UNPIN_CR_ARRAY_ON_RETRY(crarray, crarrayindex)				\
{										\
	int4			lcl_crarrayindex;				\
	cache_rec_ptr_ptr_t	cr_ptr;						\
	cache_rec_ptr_t		cr;						\
	uint4			in_tend, data_invalid, in_cw_set;		\
										\
	lcl_crarrayindex = crarrayindex;					\
	if (lcl_crarrayindex)							\
	{									\
		cr_ptr = (cache_rec_ptr_ptr_t)&crarray[lcl_crarrayindex-1];	\
		while (lcl_crarrayindex--)					\
		{								\
			cr = *cr_ptr;						\
			DEBUG_ONLY(in_tend = cr->in_tend);			\
			DEBUG_ONLY(data_invalid = cr->data_invalid);		\
			DEBUG_ONLY(in_cw_set = cr->in_cw_set);			\
			assert(!data_invalid);					\
			assert(!in_tend);					\
			assert(process_id == in_cw_set);			\
			UNPIN_CACHE_RECORD(cr);					\
			cr_ptr--;						\
		}								\
		crarrayindex = 0;						\
	}									\
}

/* Macro to reset cr->in_cw_set (UNPIN) for the entire cr_array in case of a commit (TP or non-TP).
 * Usually in_cw_set is set for all cache-records that we are planning on updating before we start phase1.
 * After updating each cse in phase2, we reset the corresponding cse->cr->in_cw_set.
 * Therefore on a successful commit, after completing all cses in phase2, we dont expect any pinned cr->in_cw_set at all.
 * This is true for Unix but in VMS where we could have twins, both the older and newer twins have the in_cw_set set in
 * phase1 while only the newer twin's in_cw_set gets reset in phase2 (since only this cr will be stored in cse->cr).
 * Therefore there could be a few cache-records which need to be unpinned even after all cses are done in phase2.
 * The following macro unpins those. It is structured such a way that in Unix, it only checks that all have been reset
 * while it actually does the reset only in VMS.
 */
#if defined(VMS)
#define UNPIN_CR_ARRAY_ON_COMMIT(crarray, crarrayindex)				\
{										\
	int4			lcl_crarrayindex;				\
	cache_rec_ptr_ptr_t	cr_ptr;						\
	cache_rec_ptr_t		cr;						\
										\
	lcl_crarrayindex = crarrayindex;					\
	if (lcl_crarrayindex)							\
	{									\
		cr_ptr = (cache_rec_ptr_ptr_t)&crarray[lcl_crarrayindex-1];	\
		while (lcl_crarrayindex--)					\
		{								\
			cr = *cr_ptr;						\
			UNPIN_CACHE_RECORD(cr);					\
			cr_ptr--;						\
		}								\
		crarrayindex = 0;						\
	}									\
}
#elif defined(UNIX)
#	ifdef DEBUG
#	define UNPIN_CR_ARRAY_ON_COMMIT(crarray, crarrayindex)				\
	{										\
		int4			lcl_crarrayindex;				\
		cache_rec_ptr_ptr_t	cr_ptr;						\
		cache_rec_ptr_t		cr;						\
											\
		lcl_crarrayindex = crarrayindex;					\
		if (lcl_crarrayindex)							\
		{									\
			cr_ptr = (cache_rec_ptr_ptr_t)&crarray[lcl_crarrayindex-1];	\
			while (lcl_crarrayindex--)					\
			{								\
				cr = *cr_ptr;						\
				assert(process_id != cr->in_cw_set);			\
				cr_ptr--;						\
			}								\
			crarrayindex = 0;						\
		}									\
	}
#	else
#	define UNPIN_CR_ARRAY_ON_COMMIT(crarray, crarrayindex)				\
		crarrayindex = 0;
#	endif
#endif

/* Every process that initializes journal pool has two aspects of validation.
 * 1. 	Validate whether this process can do logical updates to the journal pool that is initialized. If not, the process should
 *	issue SCNDDBNOUPD error. Examples of this would be a GT.M update that happens on a non-supplementary Receiver Side.
 *
 * 2.	Validate whether the process is attached to the correct journal pool. If not, REPLINSTMISMTCH should be issued. This check
 *	ensures that we only do updates to region that is tied to the journal pool that we initialized.
 *
 * The below macro definitions indicate which of the above 2 checks is done by the process for a particular region until now.
 * The macro VALIDATE_INITIALIZED_JNLPOOL does the actual validation.
 */
#define JNLPOOL_NOT_VALIDATED		0x00
#define REPLINSTMISMTCH_CHECK_DONE	0x01
#define SCNDDBNOUPD_CHECK_DONE		0x02
#define JNLPOOL_VALIDATED		0x03

#define SCNDDBNOUPD_CHECK_FALSE		0x00
#define SCNDDBNOUPD_CHECK_TRUE		0x01

#define VALIDATE_INITIALIZED_JNLPOOL(CSA, CNL, REG, JNLPOOL_USER, SCNDDBNOUPD_CHECK_NEEDED)					\
{																\
	GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;										\
	GBLREF	boolean_t		is_updproc;										\
																\
	unsigned char			instfilename_copy[MAX_FN_LEN + 1];							\
	sm_uc_ptr_t			jnlpool_instfilename;									\
	int4				jnlpool_shmid;										\
	uint4				jnlpool_validate_check;									\
	boolean_t			do_REPLINSTMISMTCH_check;								\
																\
	jnlpool_validate_check = CSA->jnlpool_validate_check;									\
	assert(JNLPOOL_VALIDATED >= jnlpool_validate_check);									\
	if (JNLPOOL_VALIDATED != jnlpool_validate_check)									\
	{															\
		do_REPLINSTMISMTCH_check = (!(REPLINSTMISMTCH_CHECK_DONE & jnlpool_validate_check) 				\
						UNIX_ONLY(&& ((GTMRELAXED != JNLPOOL_USER) || !IS_GTM_IMAGE)));			\
		if (!(SCNDDBNOUPD_CHECK_DONE & jnlpool_validate_check) && SCNDDBNOUPD_CHECK_NEEDED)				\
		{														\
			if (jnlpool_ctl->upd_disabled && !is_updproc)								\
			{	/* Updates are disabled in this journal pool. Detach from journal pool and issue error. */	\
				assert(NULL != jnlpool.jnlpool_ctl);								\
				jnlpool_detach();										\
				assert(NULL == jnlpool.jnlpool_ctl);								\
				assert(FALSE == pool_init);									\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_SCNDDBNOUPD);					\
			}													\
			CSA->jnlpool_validate_check |= SCNDDBNOUPD_CHECK_DONE;							\
		}														\
		if (do_REPLINSTMISMTCH_check)											\
		{														\
			UNIX_ONLY(jnlpool_instfilename = (sm_uc_ptr_t)jnlpool_ctl->jnlpool_id.instfilename;)			\
			VMS_ONLY(jnlpool_instfilename = (sm_uc_ptr_t)jnlpool_ctl->jnlpool_id.gtmgbldir;)			\
			if (STRCMP(CNL->replinstfilename, jnlpool_instfilename)							\
				UNIX_ONLY(|| (CNL->jnlpool_shmid != jnlpool.repl_inst_filehdr->jnlpool_shmid)))			\
			{													\
				/* Replication instance filename or jnlpool shmid mismatch. Two possibilities.			\
				 * (a) Database has already been bound with a replication instance file name that is different	\
				 *	from the instance file name used by the current process.				\
				 * (b) Database has already been bound with a jnlpool shmid and another jnlpool is about to	\
				 *	be bound with the same database. Disallow this mixing of multiple jnlpools.		\
				 * Note that (b) is Unix-only. In VMS, we dont check the shmids currently.			\
				 * Issue error. But before that detach from journal pool.					\
				 * Copy replication instance file name in journal pool to temporary memory before detaching.	\
				 * Actually case (b) subsumes (a) so we assert that below. But in pro we handle both cases	\
				 *	just in case.										\
				 */												\
				UNIX_ONLY(assert(CNL->jnlpool_shmid != jnlpool.repl_inst_filehdr->jnlpool_shmid);)		\
				UNIX_ONLY(assert(SIZEOF(instfilename_copy) == SIZEOF(jnlpool_ctl->jnlpool_id.instfilename)));	\
				VMS_ONLY(assert(SIZEOF(instfilename_copy) == SIZEOF(jnlpool_ctl->jnlpool_id.gtmgbldir)));	\
				memcpy(&instfilename_copy[0], jnlpool_instfilename, SIZEOF(instfilename_copy));			\
				assert(SIZEOF(jnlpool_shmid) == SIZEOF(CNL->jnlpool_shmid));					\
				UNIX_ONLY(jnlpool_shmid = jnlpool.repl_inst_filehdr->jnlpool_shmid;)				\
				VMS_ONLY(jnlpool_shmid = 0;)	/* print shmid of 0 for VMS as it is actually a string */	\
				assert(NULL != jnlpool.jnlpool_ctl);								\
				jnlpool_detach();										\
				assert(NULL == jnlpool.jnlpool_ctl);								\
				assert(FALSE == pool_init);									\
				rts_error_csa(CSA_ARG(CSA) VARLSTCNT(10) ERR_REPLINSTMISMTCH, 8,				\
						LEN_AND_STR(instfilename_copy), jnlpool_shmid, DB_LEN_STR(REG),			\
						LEN_AND_STR(CNL->replinstfilename), CNL->jnlpool_shmid);			\
			}													\
			CSA->jnlpool_validate_check |= REPLINSTMISMTCH_CHECK_DONE;						\
		}														\
	}															\
}

#define	JNLPOOL_INIT_IF_NEEDED(CSA, CSD, CNL)											\
{																\
	GBLREF	boolean_t		is_replicator;										\
	GBLREF	boolean_t		pool_init;										\
	GBLREF	gd_region		*gv_cur_region;										\
																\
	if (REPL_ALLOWED(CSD) && is_replicator)											\
	{															\
		if (!pool_init)													\
			jnlpool_init((jnlpool_user)GTMPROC, (boolean_t)FALSE, (boolean_t *)NULL);				\
		assert(pool_init);												\
		VALIDATE_INITIALIZED_JNLPOOL(CSA, CNL, gv_cur_region, GTMPROC, SCNDDBNOUPD_CHECK_TRUE);				\
	}															\
}

#define ASSERT_VALID_JNLPOOL(CSA)										\
{														\
	GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;								\
	GBLREF	jnlpool_addrs		jnlpool;								\
														\
	assert(CSA && CSA->critical && CSA->nl); /* should have been setup in mu_rndwn_replpool */		\
	assert(jnlpool_ctl && (jnlpool_ctl == jnlpool.jnlpool_ctl));						\
	assert(CSA->critical == (mutex_struct_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl + JNLPOOL_CTL_SIZE));	\
	assert(CSA->nl == (node_local_ptr_t) ((sm_uc_ptr_t)CSA->critical + JNLPOOL_CRIT_SPACE			\
		+ SIZEOF(mutex_spin_parms_struct)));								\
	assert(jnlpool_ctl->filehdr_off);									\
	assert(jnlpool_ctl->srclcl_array_off > jnlpool.jnlpool_ctl->filehdr_off);				\
	assert(jnlpool_ctl->sourcelocal_array_off > jnlpool.jnlpool_ctl->srclcl_array_off);			\
	assert(jnlpool.repl_inst_filehdr == (repl_inst_hdr_ptr_t) ((sm_uc_ptr_t)jnlpool_ctl			\
			+ jnlpool_ctl->filehdr_off));								\
	assert(jnlpool.gtmsrc_lcl_array == (gtmsrc_lcl_ptr_t)((sm_uc_ptr_t)jnlpool_ctl				\
			+ jnlpool_ctl->srclcl_array_off));							\
	assert(jnlpool.gtmsource_local_array == (gtmsource_local_ptr_t)((sm_uc_ptr_t)jnlpool_ctl		\
					+ jnlpool_ctl->sourcelocal_array_off));					\
}

/* Explanation for why we need the following macro.
 *
 * Normally a cdb_sc_blkmod check is done using the "bt". This is done in t_end and tp_tend.
 * But that is possible only if we hold crit. There are a few routines (TP only) that need
 * to do this check outside of crit (e.g. tp_hist, gvcst_search). For those, the following macro
 * is defined. This macro compares transaction numbers directly from the buffer instead of
 * going through the bt or blk queues. This is done to speed up processing. One consequence
 * is that we might encounter a situation where the buffer's contents hasn't been modified,
 * but the block might actually have been changed i.e. in VMS a twin buffer might have been
 * created or the "blk" field in the cache-record corresponding to this buffer might have
 * been made CR_BLKEMPTY etc. In these cases, we rely on the fact that the cycle for the
 * buffer would have been incremented thereby saving us in the cdb_sc_lostcr check which will
 * always FOLLOW (never PRECEDE) this check.
 *
 * Note that in case of BG, it is possible that the buffer could be in the process of being updated
 * (phase2 outside of crit). In this case we have to restart as otherwise we could incorrectly
 * validate an inconsistent state of the database as okay. For example, say our search path
 * contains a level-1 root-block and a level-0 data block. If both of these blocks were
 * concurrently being updated in phase2 (outside of crit) by another process, it is possible
 * (because of the order in which blocks are committed) that the data block contents get
 * modified first but the index block is still unchanged. If we traversed down the tree at
 * this instant, we are looking at a search path that contains a mix of pre-update and post-update
 * blocks and should never validate this traversal as okay. In this case, the cache record
 * corresponding to the index block would have its "in_tend" flag non-zero indicating update is pending.
 *
 * The order of the check should be cr->in_tend BEFORE the buffaddr->tn check. Doing it otherwise
 * would mean it is posible for the buffaddr->tn check to succeed and before the cr->in_tend
 * check is done the buffer gets rebuilt (from start to finish in phase2). This would result
 * in us falsely validating this transaction as okay when in fact we should have restarted.
 *
 * Because we rely on the fact that cr->in_tend is reset to 0 AFTER t1->buffaddr->tn is updated, and
 * since these could be updated concurrently, and since this macro is used outside of crit, we need to
 * ensure a read memory barrier is done. Currently, the only two places which use this macro are tp_hist.c
 * and gvcst_search.c. Out of this, the latter uses this only as a performance measure and not for correctness.
 * But the former uses this for correctness. In fact tp_tend.c relies on tp_hist.c doing a proper validation.
 * Therefore the read memory barrier is essential in tp_hist.c and not needed in gvcst_search.c. See tp_hist.c
 * for use of the read memory barrier and a comment describing why it is ok to do it only once per function
 * invocation (instead of using it once per block that gets validated).
 *
 * There are two variants of this macro.
 * TP_IS_CDB_SC_BLKMOD  : That calculates the blktn by doing t1->buffaddr->tn explicitly.
 * TP_IS_CDB_SC_BLKMOD3 : This is provided the blktn as input so can avoid the explicit calculation.
 */
#define	TP_IS_CDB_SC_BLKMOD(cr, t1) (((NULL != (cr)) && (cr)->in_tend) || ((t1)->tn <= ((blk_hdr_ptr_t)(t1)->buffaddr)->tn))
#define	TP_IS_CDB_SC_BLKMOD3(cr, t1, blktn) (((NULL != (cr)) && (cr)->in_tend) || ((t1)->tn <= blktn))

#define MM_ADDR(SGD)		((sm_uc_ptr_t)(((sgmnt_data_ptr_t)SGD) + 1))
#ifdef VMS
#define MASTER_MAP_BLOCKS_DFLT		64				/* 64 gives 128M possible blocks */
#else
#define MASTER_MAP_BLOCKS_DFLT		496				/* 496 gives 992M possible blocks */
#define MASTER_MAP_BLOCKS_V5		112				/* 112 gives 224M possible blocks */
#endif
#define MASTER_MAP_BLOCKS_V4		32				/* 32 gives 64M possible blocks  */
#define MASTER_MAP_BLOCKS_MAX		MASTER_MAP_BLOCKS_DFLT		/* 496 gives 992M possible blocks  */
#define MASTER_MAP_BLOCKS_V5_OLD	64				/* V5 database previous master map block size */
#define MASTER_MAP_SIZE_V5_OLD	(MASTER_MAP_BLOCKS_V5_OLD * DISK_BLOCK_SIZE)
#define MASTER_MAP_SIZE_V4	(MASTER_MAP_BLOCKS_V4 * DISK_BLOCK_SIZE)	/* MUST be a multiple of DISK_BLOCK_SIZE */
#define MASTER_MAP_SIZE_MAX	(MASTER_MAP_BLOCKS_MAX * DISK_BLOCK_SIZE)	/* MUST be a multiple of DISK_BLOCK_SIZE */
#define MASTER_MAP_SIZE_DFLT	(MASTER_MAP_BLOCKS_DFLT * DISK_BLOCK_SIZE)	/* MUST be a multiple of DISK_BLOCK_SIZE */
#define MASTER_MAP_SIZE_V5	(MASTER_MAP_BLOCKS_V5 * DISK_BLOCK_SIZE)	/* MUST be a multiple of DISK_BLOCK_SIZE */
#define MASTER_MAP_SIZE(SGD)	(((sgmnt_data_ptr_t)SGD)->master_map_len)
#define SGMNT_HDR_LEN		SIZEOF(sgmnt_data)
#define SIZEOF_FILE_HDR(SGD)	(SGMNT_HDR_LEN + MASTER_MAP_SIZE(SGD))
#define SIZEOF_FILE_HDR_DFLT	(SGMNT_HDR_LEN + MASTER_MAP_SIZE_DFLT)
#define SIZEOF_FILE_HDR_V5	(SGMNT_HDR_LEN + MASTER_MAP_SIZE_V5)
#define SIZEOF_FILE_HDR_MIN	(SGMNT_HDR_LEN + MASTER_MAP_SIZE_V4)
#define SIZEOF_FILE_HDR_MAX	(SGMNT_HDR_LEN + MASTER_MAP_SIZE_MAX)
#define MM_BLOCK		(SGMNT_HDR_LEN / DISK_BLOCK_SIZE + 1)	/* gt.m numbers blocks from 1 */
#define TH_BLOCK	 	1

#define JNL_NAME_SIZE	        256        /* possibly expanded when opened */
#define JNL_NAME_EXP_SIZE	1024       /* MAXPATHLEN, before jnl_buffer in shared memory */

#define BLKS_PER_LMAP		512
#define MAXTOTALBLKS_V4		(MASTER_MAP_SIZE_V4 * 8 * BLKS_PER_LMAP)
#define MAXTOTALBLKS_V5		(MASTER_MAP_SIZE_MAX * 8 * BLKS_PER_LMAP)
#define MAXTOTALBLKS_V6		(MASTER_MAP_SIZE_MAX * 8 * BLKS_PER_LMAP)
#define MAXTOTALBLKS_MAX	(MASTER_MAP_SIZE_MAX * 8 * BLKS_PER_LMAP)
#define MAXTOTALBLKS(SGD)	(MASTER_MAP_SIZE(SGD) * 8 * BLKS_PER_LMAP)
#define	IS_BITMAP_BLK(blk)	(ROUND_DOWN2(blk, BLKS_PER_LMAP) == blk)	/* TRUE if blk is a bitmap */
/* UNIX -
 * 	V6 - 8K fileheader (= 16 blocks) + 248K mastermap (= 496 blocks) + 1
 * 	V5 - 8K fileheader (= 16 blocks) + 56K mastermap (= 112 blocks) + 1
 * 	V4 - 8K fileheader (= 16 blocks) + 16K mastermap (= 32 blocks) + 1
 * VMS  - 8K fileheader (= 16 blocks) + 32K mastermap (= 64 blocks) + 24K padding (= 48 blocks) + 1
 */
#define START_VBN_V6		513
#define START_VBN_V5		129
#define	START_VBN_V4		 49
#define START_VBN_CURRENT	START_VBN_V6

#define	STEP_FACTOR			64		/* the factor by which flush_trigger is incremented/decremented */
#define	MIN_FLUSH_TRIGGER(n_bts)	((n_bts)/4)	/* the minimum flush_trigger as a function of n_bts */
#define	MAX_FLUSH_TRIGGER(n_bts)	((n_bts)*15/16)	/* the maximum flush_trigger as a function of n_bts */

#define MIN_FILLFACTOR 30
#define MAX_FILLFACTOR 100

#ifdef DEBUG_DYNGRD
#  define DEBUG_DYNGRD_ONLY(X) X
#else
#  define DEBUG_DYNGRD_ONLY(X)
#endif

#ifdef VMS
/* RET is a dummy that is not really used on VMS */
#define	DCLAST_WCS_WTSTART(reg, num_bufs, RET)				\
{									\
	unsigned int	status;						\
									\
	if (SS$_NORMAL != (status = sys$dclast(wcs_wtstart, reg, 0)))	\
	{								\
		assert(FALSE);						\
		status = DISABLE_AST;					\
		wcs_wtstart(reg);					\
		if (SS$_WASSET == status)				\
			ENABLE_AST;					\
	}								\
}
#elif defined(UNIX)
#define	DCLAST_WCS_WTSTART(reg, num_bufs, RET)	RET = wcs_wtstart(reg, num_bufs);
#else
#error UNSUPPORTED PLATFORM
#endif

#define SAVE_WTSTART_PID(cnl, pid, index)			\
{								\
	for (index = 0; index < MAX_WTSTART_PID_SLOTS; index++)	\
		if (0 == cnl->wtstart_pid[index])		\
			break;					\
	if (MAX_WTSTART_PID_SLOTS > index)			\
		cnl->wtstart_pid[index] = pid;			\
}

#define CLEAR_WTSTART_PID(cnl, index)				\
{								\
	if (MAX_WTSTART_PID_SLOTS > index)			\
		cnl->wtstart_pid[index] = 0;			\
}

#define	WRITERS_ACTIVE(cnl)	((0 < cnl->intent_wtstart) || (0 < cnl->in_wtstart))

#define	SIGNAL_WRITERS_TO_STOP(cnl)								\
{												\
	SET_TRACEABLE_VAR((cnl)->wc_blocked, TRUE);	/* to stop all active writers */	\
	/* memory barrier needed to broadcast this information to other processors */		\
	SHM_WRITE_MEMORY_BARRIER;								\
}

#define	WAIT_FOR_WRITERS_TO_STOP(cnl, lcnt, maxiters)						\
{	/* We need to ensure that an uptodate value of cnl->intent_wtstart is read in the	\
	 * WRITERS_ACTIVE macro every iteration of the loop hence the read memory barrier.	\
	 */											\
	SHM_READ_MEMORY_BARRIER;								\
	for (lcnt=1; WRITERS_ACTIVE(cnl) && (lcnt <= maxiters);  lcnt++)			\
	{	/* wait for any processes INSIDE or at ENTRY of wcs_wtstart to finish */	\
		wcs_sleep(lcnt);								\
		SHM_READ_MEMORY_BARRIER;							\
	}											\
}

#define	SIGNAL_WRITERS_TO_RESUME(cnl)								\
{												\
	SET_TRACEABLE_VAR((cnl)->wc_blocked, FALSE); /* to let active writers resume */	\
	/* memory barrier needed to broadcast this information to other processors */		\
	SHM_WRITE_MEMORY_BARRIER;								\
}

#define	INCR_INTENT_WTSTART(cnl)									\
{													\
	INCR_CNT(&cnl->intent_wtstart, &cnl->wc_var_lock); /* signal intent to enter wcs_wtstart */	\
	if (0 >= cnl->intent_wtstart)									\
	{	/* possible if wcs_verify had reset this flag */					\
		INCR_CNT(&cnl->intent_wtstart, &cnl->wc_var_lock);					\
		/* wcs_verify cannot possibly have reset this flag again because it does this only	\
		 * after wcs_recover waits for a maximum of 1 minute (for this flag to become zero)	\
		 * before giving up. Therefore for that to happen, we should have been context		\
		 * switched out for 1 minute after the second INCR_CNT but before the below assert)	\
		 * We believe that is an extremely unlikely condition so dont do anything about it.	\
		 * In the worst case this will get reset to 0 by the next wcs_verify or INCR_CNT	\
		 * (may need multiple INCR_CNTs depending on how negative a value this is) whichever	\
		 * happens sooner.									\
		 */											\
		assert(0 < cnl->intent_wtstart);							\
	}												\
}

#define	DECR_INTENT_WTSTART(cnl)					\
{									\
	if (0 < cnl->intent_wtstart)					\
		DECR_CNT(&cnl->intent_wtstart, &cnl->wc_var_lock);	\
	/* else possible if wcs_verify had reset this flag */		\
}

#define ENSURE_JNL_OPEN(csa, reg)                                                 					\
{                                                                               					\
	boolean_t		was_crit;                                               				\
	jnl_private_control	*jpc;											\
	sgmnt_data_ptr_t	csd;											\
	uint4			jnl_status;										\
															\
	assert(cs_addrs == csa);                                                        				\
	assert(gv_cur_region == reg);                                                   				\
	assert(FALSE == reg->read_only);                                                				\
	csd = csa->hdr;													\
	if (JNL_ENABLED(csd))												\
	{                                                                       					\
		was_crit = csa->now_crit;                                         					\
		if (!was_crit)                                                  					\
			grab_crit(reg);                                         					\
		jnl_status = JNL_ENABLED(csd) ? jnl_ensure_open() : 0;    	                             		\
		if (!was_crit)                                                  					\
			rel_crit(reg);                                          					\
		if (0 != jnl_status)											\
		{													\
			jpc = csa->jnl;											\
			assert(NULL != jpc);										\
			if (SS_NORMAL != jpc->status)									\
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(csd),		\
						DB_LEN_STR(gv_cur_region), jpc->status);				\
			else												\
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd),		\
						DB_LEN_STR(gv_cur_region));						\
		}													\
	}                                                                       					\
}

/* the RET is meaningful only on UNIX */
#define JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, num_bufs, RET)    \
{                                                               \
	ENSURE_JNL_OPEN(csa, reg);                              \
	DCLAST_WCS_WTSTART(reg, num_bufs, RET);			\
}

/* Macros to effect changes in the blks_to_upgrd field of the file-header.
 * We should hold crit on the region in all cases except for one when we are in MUPIP CREATE (but we are still standalone here).
 * Therefore we need not use any interlocks to update this field. This is asserted below.
 * Although we can derive "csd" from "csa", we pass them as two separate arguments for performance reasons.
 * Use local variables to record shared memory information doe debugging purposes in case of an assert failure.
 */
#define INCR_BLKS_TO_UPGRD(csa, csd, delta)						\
{											\
	int4	new_blks_to_upgrd;							\
	int4	cur_blks_to_upgrd;							\
	int4	cur_delta;								\
											\
	assert((csd)->createinprogress || (csa)->now_crit);				\
	cur_delta = (delta);								\
	assert((csa)->hdr == (csd));							\
	assert(0 != cur_delta);								\
	cur_blks_to_upgrd = (csd)->blks_to_upgrd;					\
	assert(0 <= (csd)->blks_to_upgrd);						\
	new_blks_to_upgrd = cur_delta + cur_blks_to_upgrd;				\
	assert(0 <= new_blks_to_upgrd);							\
	(csd)->blks_to_upgrd = new_blks_to_upgrd;					\
	if (0 >= new_blks_to_upgrd)							\
	{										\
		if (0 == new_blks_to_upgrd)						\
			(csd)->tn_upgrd_blks_0 = (csd)->trans_hist.curr_tn;		\
		else									\
		{	/* blks_to_upgrd counter in the fileheader should never hold a	\
			 * negative value. Note down the negative value in a separate	\
			 * field for debugging and set the counter to 0.		\
			 */								\
			(csd)->blks_to_upgrd = 0;					\
			(csd)->blks_to_upgrd_subzero_error -= (new_blks_to_upgrd);	\
		}									\
	} else										\
		(csd)->fully_upgraded = FALSE;						\
}
#define DECR_BLKS_TO_UPGRD(csa, csd, delta)	INCR_BLKS_TO_UPGRD((csa), (csd), -(delta))

/* Interlocked queue instruction constants ... */

#define	QI_STARVATION		3
#define EMPTY_QUEUE		0L
#define QUEUE_WAS_EMPTY		1
#define INTERLOCK_FAIL		-1L
#define QUEUE_INSERT_SUCCESS	1

typedef trans_num	bg_trc_rec_tn;
typedef int4		bg_trc_rec_cntr;

typedef struct
{
	int4	curr_count;	/* count for this invocation of shared memory */
	int4	cumul_count;	/* count from the creation of database (not including this invocation) */
} db_csh_acct_rec;

#define	TAB_DB_CSH_ACCT_REC(A,B,C)	A,
enum db_csh_acct_rec_type
{
#include "tab_db_csh_acct_rec.h"
n_db_csh_acct_rec_types
};
#undef TAB_DB_CSH_ACCT_REC

#include "gvstats_rec.h"

#define	GVSTATS_SET_CSA_STATISTIC(csa, counter, value)		\
{								\
	csa->gvstats_rec.counter = value;			\
}

#define	INCR_GVSTATS_COUNTER(csa, cnl, counter, increment)	\
{								\
	csa->gvstats_rec.counter += increment;			\
	cnl->gvstats_rec.counter += increment;			\
}

#if defined(DEBUG) || defined(DEBUG_DB_CSH_COUNTER)
#	define	INCR_DB_CSH_COUNTER(csa, counter, increment)				\
			if (csa->read_write || dba_bg == csa->hdr->acc_meth)		\
				csa->hdr->counter.curr_count += increment;
#else
#	define	INCR_DB_CSH_COUNTER(csa, counter, increment)
#endif

enum tp_blkmod_type		/* used for accounting in cs_data->tp_cdb_sc_blkmod[] */
{
	tp_blkmod_nomod = 0,
	tp_blkmod_gvcst_srch,
	tp_blkmod_t_qread,
	tp_blkmod_tp_tend,
	tp_blkmod_tp_hist,
	n_tp_blkmod_types
};

/* Below is a list of macro bitmasks used to set the global variable "donot_commit". This variable should normally be 0.
 * But in rare cases, we could end up in situations where we know it is a restartable situation but decide not to
 * restart right away (because of interface issues that the function where this is detected cannot signal a restart
 * or because we dont want to take a performance hit to check this restartable situation in highly frequented code if
 * the restart will anyway be detected before commit. In this cases, this variable will take on non-zero values.
 * The commit logic will assert that this variable is indeed zero after validation but before proceeding with commit.
 */
#define	DONOTCOMMIT_TPHIST_BLKTARGET_MISMATCH		(1 << 0) /* Restartable situation encountered in tp_hist */
#define	DONOTCOMMIT_GVCST_DELETE_BLK_CSE_TLEVEL		(1 << 1) /* Restartable situation encountered in gvcst_delete_blk */
#define	DONOTCOMMIT_JNLGETCHECKSUM_NULL_CR		(1 << 2) /* Restartable situation encountered in jnl_get_checksum.h */
#define	DONOTCOMMIT_GVCST_KILL_ZERO_TRIGGERS		(1 << 3) /* Restartable situation encountered in gvcst_kill */
#define	DONOTCOMMIT_GVCST_BLK_BUILD_TPCHAIN		(1 << 4) /* Restartable situation encountered in gvcst_blk_build */
#define	DONOTCOMMIT_T_QREAD_BAD_PVT_BUILD		(1 << 5) /* Restartable situation due to bad private build in t_qread */
#define	DONOTCOMMIT_GVCST_SEARCH_LEAF_BUFFADR_NOTSYNC	(1 << 6) /* Restartable situation encountered in gvcst_search */
#define	DONOTCOMMIT_GVCST_SEARCH_BLKTARGET_MISMATCH	(1 << 7) /* Restartable situation encountered in gvcst_search */

#define TAB_BG_TRC_REC(A,B)	B,
enum bg_trc_rec_type
{
#include "tab_bg_trc_rec.h"
n_bg_trc_rec_types
};
#undef TAB_BG_TRC_REC

#define UPGRD_WARN_INTERVAL (60 * 60 * 24)	/* Once every 24 hrs */

/* The following structure is used to determine
   the endianess of a database header.
*/

typedef union
{
	struct {
		unsigned short little_endian;
		unsigned short big_endian;
		} shorts;
	uint4 word32;
} endian32_struct;

#ifdef BIGENDIAN
#  define ENDIANCHECKTHIS	big_endian
#else
#  define ENDIANCHECKTHIS	little_endian
#endif

#define CHECK_DB_ENDIAN(CSD,FNLEN,FNNAME)								\
{													\
	endian32_struct	check_endian;									\
	check_endian.word32 = (CSD)->minor_dbver;							\
	if (!check_endian.shorts.ENDIANCHECKTHIS)							\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_DBENDIAN, 4, FNLEN, FNNAME, ENDIANOTHER,	\
				ENDIANTHIS);								\
}

/* This is the structure describing a segment. It is used as a database file header (for MM or BG access methods).
 * The overloaded fields for MM and BG are n_bts, bt_buckets. */

/* ***NOTE*** If the field minor_dbver is updated, please also update gdsdbver.h and db_auto_upgrade.c appropriately
   (see db_auto_upgrade for reasons and description). SE 5/2006
*/
typedef struct sgmnt_data_struct
{
	/************* MOSTLY STATIC DATABASE STATE FIELDS **************************/
	unsigned char	label[GDS_LABEL_SZ];
	int4		blk_size;		/* Block size for the file. Static data defined at db creation time */
	int4		master_map_len;		/* Length of master map */
	int4		bplmap;			/* Blocks per local map (bitmap). static data defined at db creation time */
	int4		start_vbn;		/* starting virtual block number. */
	enum db_acc_method acc_meth;		/* Access method (BG or MM) */
	uint4		max_bts;		/* Maximum number of bt records allowed in file */
	int4		n_bts;			/* number of cache record/blocks */
	int4		bt_buckets;		/* Number of buckets in bt table */
	int4		reserved_bytes;		/* Database blocks will always leave this many bytes unused */
	int4		max_rec_size;		/* maximum record size allowed for this file */
	int4		max_key_size;		/* maximum key size allowed for this file */
	uint4		lock_space_size;	/* Number of bytes to be used for locks (in database for bg) */
	uint4		extension_size;		/* Number of gds data blocks to extend by */
	uint4		def_coll;		/* Default collation type for new globals */
	uint4		def_coll_ver;		/* Default collation type version */
	boolean_t	std_null_coll;		/* 0 -> GT.M null collation,i,e, null subs collate between numeric and string
						 * 1-> standard null collation i.e. null subs collate before numeric and string */
	boolean_t	null_subs;
	uint4		free_space;		/* Space in file header not being used */
	mutex_spin_parms_struct	mutex_spin_parms;
	int4		max_update_array_size;	/* maximum size of update array needed for one non-TP set/kill */
	int4		max_non_bm_update_array_size;/* maximum size of update array excepting bitmaps */
	boolean_t	file_corrupt;		/* If set, it shuts the file down.  No process (except DSE) can
						 * successfully map this section after the flag is set to TRUE. Processes
						 * that already have it mapped should produce an error the next time that
						 * they use the file. The flag can only be reset by the DSE utility.
						 */
	enum mdb_ver	minor_dbver;		/* Minor DB version field that is incremented when minor changes to this
						 * file-header or API changes occur. See note at top of sgmnt_data.
						 */
	uint4		jnl_checksum;
	uint4		wcs_phase2_commit_wait_spincnt;	/* # of spin iterations before sleeping while waiting for phase2 commits */
	enum mdb_ver	last_mdb_ver;		/* Minor DB version of the GT.M version that last accessed this database.
						 * Maintained only by GT.M versions V5.3-003 and greater.
						 */
	/* The structure is 128-bytes in size at this point */
	/************* FIELDS SET AT CREATION TIME ********************************/
	char		filler_created[52];	/* Now unused .. was "file_info created" */
	boolean_t	createinprogress;	/* TRUE only if MUPIP CREATE is in progress. FALSE otherwise */
	int4		creation_time4;		/* Lower order 4-bytes of time when the database file was created */
	int4		creation_filler_8byte;

	/************* FIELDS USED BY TN WARN PROCESSING *************************/
	trans_num	max_tn;			/* Hardstop TN for this database */
	trans_num	max_tn_warn;		/* TN for next TN_RESET warning for this database */

	/************* FIELDS SET BY MUPIP BACKUP/REORG *************************/
	trans_num	last_inc_backup;
	trans_num	last_com_backup;
	trans_num	last_rec_backup;
	block_id	last_inc_bkup_last_blk;	/* Last block in the database at time of last incremental backup */
	block_id	last_com_bkup_last_blk;	/* Last block in the database at time of last comprehensive backup */
	block_id	last_rec_bkup_last_blk;	/* Last block in the database at time of last record-ed backup */
	block_id	reorg_restart_block;
	char		filler_256[8];
	/************* FIELDS SET WHEN DB IS OPEN ********************************/
	char		now_running[MAX_REL_NAME];/* for active version stamp */
#	ifdef VMS
	uint4		owner_node;		/* Node on cluster that "owns" the file -- applies to VMS only */
#	else
	uint4		filler_owner_node;	/* 4-byte filler - since owner_node is maintained on VMS only */
#	endif
	uint4		image_count;		/* for db freezing. Set to "process_id" on Unix and "image_count" on VMS */
	uint4		freeze;			/* for db freezing. Set to "getuid"     on Unix and "process_id"  on VMS */
	int4		kill_in_prog;		/* counter for multi-crit kills that are not done yet */
	int4		abandoned_kills;
	char		filler_320[8];
	/************* FIELDS USED IN V4 <==> V5 COMPATIBILITY MODE ****************/
	trans_num	tn_upgrd_blks_0;	/* TN when blks_to_upgrd becomes 0.
						 *	 Never set = 0 => we have not achieved this yet,
						 *	Always set = 1 => database was created as V5 (or current version)
						 */
	trans_num	desired_db_format_tn;	/* Database tn when last db format change occurred */
	trans_num	reorg_db_fmt_start_tn;	/* Copy of desired_db_format_tn when MUPIP REORG UPGRADE/DOWNGRADE started */
	block_id	reorg_upgrd_dwngrd_restart_block;	/* Block numbers lesser than this were last upgraded/downgraded by
								 * MUPIP REORG UPGRADE|DOWNGRADE before being interrupted */
	int4		blks_to_upgrd;			/* Blocks not at current block version level */
	int4		blks_to_upgrd_subzero_error;	/* number of times "blks_to_upgrd" potentially became negative */
	enum db_ver	desired_db_format;	/* Output version for database blocks (normally current version) */
	boolean_t	fully_upgraded;		/* Set to TRUE by MUPIP REORG UPGRADE when ALL blocks (including RECYCLED blocks)
						 * have been examined and upgraded (if necessary) and blks_to_upgrd is set to 0;
						 * If set to TRUE, this guarantees all blocks in the database are upgraded.
						 * "blks_to_upgrd" being 0 does not necessarily guarantee the same since the
						 *	counter might have become incorrect (due to presently unknown reasons).
						 * set to FALSE whenever desired_db_format changes or the database is
						 *	updated with V4 format blocks (by MUPIP JOURNAL).
						 */
	boolean_t	db_got_to_v5_once;	/* Set to TRUE by the FIRST MUPIP REORG UPGRADE (since MUPIP UPGRADE was run
						 * to upgrade the file header to V5 format) when it completes successfully.
						 * The FIRST reorg upgrade marks all RECYCLED blocks as FREE. Successive reorg
						 * upgrades keep RECYCLED blocks as they are while still trying to upgrade them.
						 * This is because ONLY the FIRST reorg upgrade could see RECYCLED blocks in V4
						 * format that are too full (lack the additional space needed by the V5 block
						 * header) to be upgraded to V5 format. Once these are marked FREE, all future
						 * block updates happen in V5 format in the database buffers so even if they
						 * are written in V4 format to disk, they are guaranteed to be upgradeable.
						 * This field marks that transition in the db and is never updated thereafter.
						 */
	boolean_t	opened_by_gtmv53;	/* Set to TRUE the first time this database is opened by GT.M V5.3-000 and higher */
	char		filler_384[12];
	/************* FIELDS RELATED TO DB TRANSACTION HISTORY *****************************/
	th_index	trans_hist;		/* transaction history - if moved from 1st filehdr block, change TH_BLOCK */
	char		filler_trans_hist[8];
	/************* FIELDS RELATED TO WRITE CACHE FLUSHING *******************************/
	int4		flush_time[2];
	int4		flush_trigger;
	int4		n_wrt_per_flu;		/* Number of writes per flush call. Overloaded for BG and MM */
	int4		wait_disk_space;        /* seconds to wait for diskspace before giving up on a db block write */
	int4		defer_time;		/* defer write
						 *	 0 => immediate,
						 *	-1 => infinite defer,
						 *	>0 => defer_time * flush_time[0] is actual defer time
						 * default value = 1 => a write-timer every csd->flush_time[0] seconds
						 */
        volatile boolean_t filler_wc_blocked;	/* Now moved to node_local */
	boolean_t	mumps_can_bypass;	/* Allow mumps processes to bypass flushing, access control, and ftok semaphore
						 * in gds_rundown(). This was done to improve shutdown performance.
						 */
	char		filler_512[16];
	/************* FIELDS Used for update process performance improvement. Some may go away in later releases ********/
	uint4		reserved_for_upd;	/* Percentage (%) of blocks reserved for update process disk read */
	uint4		avg_blks_per_100gbl;	/* Number of blocks read on average for 100 global key read */
	uint4		pre_read_trigger_factor;/* Percentage (%) of blocks  reserved for prereader disk read */
	uint4		writer_trigger_factor;	/* For update process writers flush trigger */
	/************* FIELDS USED ONLY BY UNIX ********************************/
	int4		semid;			/* Since int may not be of fixed size, int4 is used */
	int4		shmid;			/* Since int may not be of fixed size, int4 is used */
	gtm_time8	gt_sem_ctime;		/* time of creation of semaphore */
	gtm_time8	gt_shm_ctime;		/* time of creation of shared memory */
	char		filler_unixonly[40];	/* to ensure this section has 64-byte multiple size */
	/************* ACCOUNTING INFORMATION ********************************/
	int4		filler_n_retries[CDB_MAX_TRIES];/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_puts;			/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_kills;			/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_queries;		/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_gets;			/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_order;			/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_zprevs;		/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_data;			/* Now moved to TAB_GVSTATS_REC section */
	uint4		filler_n_puts_duplicate;	/* Now moved to TAB_GVSTATS_REC section */
	uint4		filler_n_tp_updates;		/* Now moved to TAB_GVSTATS_REC section */
	uint4		filler_n_tp_updates_duplicate;	/* Now moved to TAB_GVSTATS_REC section */
	char		filler_accounting_64_align[4];	/* to ensure this section has 64-byte multiple size */
	/************* CCP/RC RELATED FIELDS (CCP STUFF IS NOT USED CURRENTLY BY GT.M) *************/
	int4		staleness[2];		/* timer value */
	int4		ccp_tick_interval[2];	/* quantum to release write mode if no write occurs and others are queued
						 * These three values are all set at creation by mupip_create
						 */
	int4		ccp_quantum_interval[2];/* delta timer for ccp quantum */
	int4		ccp_response_interval[2];/* delta timer for ccp mailbox response */
	boolean_t	ccp_jnl_before;		/* used for clustered to pass if jnl file has before images */
	boolean_t	clustered;		/* FALSE (clustering is currently unsupported) */
	boolean_t	unbacked_cache;		/* FALSE for clustering. TRUE otherwise */

	int4		rc_srv_cnt;		/* Count of RC servers accessing database */
	int4		dsid;			/* DSID value, non-zero when being accessed by RC */
	int4		rc_node;
	char		filler_ccp_rc[8];	/* to ensure this section has 64-byte multiple size */
	/************* REPLICATION RELATED FIELDS ****************/
	/* VMS does not yet have multi-site replication functionality. Hence the two sets of fields in this section. */
#ifdef VMS
	seq_num		reg_seqno;		/* the jnl seqno of the last update to this region -- 8-byte aligned */
	seq_num		resync_seqno;		/* the resync-seqno to be sent to the secondary */
	trans_num	resync_tn;		/* db tn corresponding to resync_seqno - used in losttrans handling */
	seq_num		old_resync_seqno;	/* to find out if transactions were sent from primary to secondary */
	int4		repl_state;		/* state of replication whether open/closed/was_open */
	char		filler_repl[28];		/* to ensure this section has 64-byte multiple size */
#else
	seq_num		reg_seqno;		/* the jnl seqno of the last update to this region -- 8-byte aligned */
	seq_num		pre_multisite_resync_seqno;	/* previous resync-seqno field now moved to the replication instance file */
	trans_num	zqgblmod_tn;		/* db tn corresponding to zqgblmod_seqno - used in losttrans handling */
	seq_num		zqgblmod_seqno;		/* minimum resync seqno of ALL -fetchresync rollbacks that happened on a secondary
						 * (that was formerly a root primary) AFTER the most recent
						 * MUPIP REPLIC -LOSTTNCOMPLETE command */
	int4		repl_state;		/* state of replication whether open/closed/was_open */
	boolean_t	multi_site_open;	/* Set to TRUE the first time a process opens the database using
						 * a GT.M version that supports multi-site replication. FALSE until then */
	seq_num		filler_seqno;		/* formerly dualsite_resync_seqno but removed once dual-site support was dropped */
	char		filler_repl[16];	/* to ensure this section has 64-byte multiple size */
#endif
	/************* TP RELATED FIELDS ********************/
	int4		filler_n_tp_retries[12];		/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_tp_retries_conflicts[12];	/* Now moved to TAB_GVSTATS_REC section */
	int4		tp_cdb_sc_blkmod[8];	/* Notes down the number of times each place got a cdb_sc_blkmod in tp.
						 * Only first 4 array entries are updated now, but space is allocated
						 * for 4 more if needed in the future. */
	/************* JOURNALLING RELATED FIELDS ****************/
	uint4		jnl_alq;
	uint4		jnl_deq;
	int4		jnl_buffer_size;	/* in 512-byte pages */
	boolean_t	jnl_before_image;
	int4		jnl_state;		/* journaling state: same as enum jnl_state_codes in jnl.h */
	uint4		jnl_file_len;		/* journal file name length */
	uint4		autoswitchlimit;	/* limit in disk blocks (max 4GB) when jnl should be auto switched */
	int4		epoch_interval;		/* Time between successive epochs in epoch-seconds */
	uint4		alignsize;		/* alignment size for JRT_ALIGN */
	int4		jnl_sync_io;		/* drives sync I/O ('direct' if applicable) for journals, if set (UNIX) */
						/* writers open NOCACHING to bypass XFC cache, if set (VMS) */
	int4		yield_lmt;		/* maximum number of times a process yields to get optimal jnl writes */
	boolean_t	turn_around_point;
	trans_num	jnl_eovtn;		/* last tn for a closed jnl; otherwise epoch tn from the epoch before last */
	char		filler_jnl[8];		/* to ensure this section has 64-byte multiple size */
	/************* INTERRUPTED RECOVERY RELATED FIELDS ****************/
	seq_num		intrpt_recov_resync_seqno;/* resync/fetchresync jnl_seqno of interrupted rollback */
	jnl_tm_t	intrpt_recov_tp_resolve_time;/* since-time for the interrupted recover */
	boolean_t	recov_interrupted;	/* whether a MUPIP JOURNAL RECOVER/ROLLBACK on this db got interrupted */
	int4		intrpt_recov_jnl_state;	/* journaling state at start of interrupted recover/rollback */
	int4		intrpt_recov_repl_state;/* replication state at start of interrupted recover/rollback */
	/************* TRUNCATE RELATED FIELDS ****************/
	uint4		before_trunc_total_blks;	/* Used in recover_truncate to detect interrupted truncate */
	uint4		after_trunc_total_blks;		/* All these fields are used to repair interrupted truncates */
	uint4		before_trunc_free_blocks;
	uint4		filler_trunc;			/* Previously before_trunc_file_size, which is no longer used */
	char		filler_1k[24];
	/************* HUGE CHARACTER ARRAYS **************/
	unsigned char	jnl_file_name[JNL_NAME_SIZE];	/* journal file name */
	unsigned char	reorg_restart_key[OLD_MAX_KEY_SZ + 1];	/* 1st key of a leaf block where reorg was done last time.
								 * Note: In mu_reorg we don't save keys longer than OLD_MAX_KEY_SZ
								 */
	char		machine_name[MAX_MCNAMELEN];
	char            encryption_hash[GTMCRYPT_RESERVED_HASH_LEN];
	/* char filler_2k[256] was here before adding the encryption_hash. Since the GTMCRYPT_RESERVED_HASH_LEN
	 * consumes 256 bytes, filler_2k has been removed. */
	/************* BG_TRC_REC RELATED FIELDS ***********/
#	define TAB_BG_TRC_REC(A,B)	bg_trc_rec_tn	B##_tn;
#	include "tab_bg_trc_rec.h"
#	undef TAB_BG_TRC_REC
	char		bg_trc_rec_tn_filler  [1200 - (SIZEOF(bg_trc_rec_tn) * n_bg_trc_rec_types)];

#	define TAB_BG_TRC_REC(A,B)	bg_trc_rec_cntr	B##_cntr;
#	include "tab_bg_trc_rec.h"
#	undef TAB_BG_TRC_REC
	char		bg_trc_rec_cntr_filler[600 - (SIZEOF(bg_trc_rec_cntr) * n_bg_trc_rec_types)];

	/************* DB_CSH_ACCT_REC RELATED FIELDS ***********/
#	define	TAB_DB_CSH_ACCT_REC(A,B,C)	db_csh_acct_rec	A;
#	include "tab_db_csh_acct_rec.h"
#	undef TAB_DB_CSH_ACCT_REC
	char		db_csh_acct_rec_filler_4k[248 - (SIZEOF(db_csh_acct_rec) * n_db_csh_acct_rec_types)];

	/************* GVSTATS_REC RELATED FIELDS ***********/
	gvstats_rec_t	gvstats_rec;
	char		gvstats_rec_filler_4k_plus_512[512 - SIZEOF(gvstats_rec_t)];
	char		filler_4k_plus_512[368];	/* Note: this filler array should START at offset 4K+512. So any additions
							 * of new fields should happen at the END of this filler array and
							 * the filler array size correspondingly adjusted.
							 */
	/************* INTERRUPTED RECOVERY RELATED FIELDS continued ****************/
	seq_num		intrpt_recov_resync_strm_seqno[MAX_SUPPL_STRMS];/* resync/fetchresync jnl_seqno of interrupted rollback
									 * corresponding to each non-supplementary stream.
									 */
	/************* DB CREATION AND UPGRADE CERTIFICATION FIELDS ***********/
	enum db_ver	creation_db_ver;		/* Major DB version at time of creation */
	enum mdb_ver	creation_mdb_ver;		/* Minor DB version at time of creation */
	enum db_ver	certified_for_upgrade_to;	/* Version the database is certified for upgrade to */
	int4		filler_5k;
	/************* SECSHR_DB_CLNUP RELATED FIELDS (now moved to node_local) ***********/
	int4		secshr_ops_index_filler;
	int4		secshr_ops_array_filler[255];	/* taking up 1k */
	/********************************************************/
	compswap_time_field next_upgrd_warn;	/* Time when we can send the next upgrade warning to the operator log */
	boolean_t	is_encrypted;
	uint4		db_trigger_cycle;	/* incremented every MUPIP TRIGGER command that changes ^#t global contents */
	/************* SUPPLEMENTARY REPLICATION INSTANCE RELATED FIELDS ****************/
	seq_num		strm_reg_seqno[MAX_SUPPL_STRMS];	/* the jnl seqno of the last update to this region for a given
								 * supplementary stream -- 8-byte aligned */
	seq_num		save_strm_reg_seqno[MAX_SUPPL_STRMS];	/* a copy of strm_reg_seqno[] before it gets changed in
								 * "mur_process_intrpt_recov". Used only by journal recovery.
								 * See comment in "mur_get_max_strm_reg_seqno" function for
								 * purpose of this field. Must also be 8-byte aligned.
								 */
	boolean_t	freeze_on_fail;		/* Freeze instance if failure of this database observed */
	boolean_t	span_node_absent;	/* Database does not contain the spanning node */
	boolean_t	maxkeysz_assured;	/* All the keys in the database are less than MAX_KEY_SIZE */
	char		filler_7k[724];
	char		filler_8k[1024];
	/********************************************************/
	/* Master bitmap immediately follows. Tells whether the local bitmaps have any free blocks or not. */
} sgmnt_data;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef	sgmnt_data		*sgmnt_data_ptr_t;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

typedef struct
{
	FILL8DCL(cache_que_heads_ptr_t, cache_state, 1);	/* pointer to beginnings of state queues */
} sgbg_addrs;

typedef struct
{
#	ifdef VMS
	FILL8DCL(sm_uc_ptr_t, base_addr, 1);
#	else
	int 	filler;
#	endif
} sgmm_addrs;

#define MAX_NM_LEN	MAX_MIDENT_LEN
#define MIN_RN_LEN	1
#define MAX_RN_LEN	MAX_MIDENT_LEN
#define V4_MAX_RN_LEN	31	/* required for dbcertify.h */
#define MIN_SN_LEN	1
#define MAX_SN_LEN	MAX_MIDENT_LEN
#define STR_SUB_PREFIX  0x0FF
#define SUBSCRIPT_STDCOL_NULL 0x01
#define STR_SUB_ESCAPE  0X01
#define SPANGLOB_SUB_ESCAPE  0X02
#define	STR_SUB_MAXVAL	0xFF
#define SUBSCRIPT_ZERO  0x080
#define SUBSCRIPT_BIAS  0x0BE
#define NEG_MNTSSA_END  0x0FF
#define KEY_DELIMITER   0X00
#define MIN_DB_BLOCKS	10	/* this should be maintained in conjunction with the mimimum allocation in GDEINIT.M */

/* definition for NULL_SUBSCRIPTS */
#define NEVER		0
#define ALWAYS		1
#define ALLOWEXISTING 	2

#define OFFSET(x,y) ((uchar_ptr_t)x - (uchar_ptr_t)y)

#define FC_READ 0
#define FC_WRITE 1
#define FC_OPEN 2
#define FC_CLOSE 3

#define DO_BADDBVER_CHK(REG, TSD)								\
{												\
	if (MEMCMP_LIT(TSD->label, GDS_LABEL))							\
	{											\
		if (memcmp(TSD->label, GDS_LABEL, GDS_LABEL_SZ - 3))				\
			rts_error_csa(CSA_ARG(REG2CSA(REG)) VARLSTCNT(4) ERR_DBNOTGDS, 2,	\
					DB_LEN_STR(REG));					\
		else										\
			rts_error_csa(CSA_ARG(REG2CSA(REG)) VARLSTCNT(4) ERR_BADDBVER, 2,	\
					DB_LEN_STR(REG));					\
	}											\
}

#define DO_DB_HDR_CHECK(REG, TSD)								\
{												\
	GBLREF	boolean_t	mupip_jnl_recover;						\
	uint4			gtm_errcode = 0;						\
												\
	if (TSD->createinprogress)								\
		gtm_errcode = ERR_DBCREINCOMP;							\
	if (TSD->file_corrupt && !mupip_jnl_recover)						\
		gtm_errcode = ERR_DBFLCORRP;							\
	if ((dba_mm == TSD->acc_meth) && TSD->blks_to_upgrd)					\
		gtm_errcode = ERR_MMNODYNUPGRD;							\
	if (0 != gtm_errcode)									\
	{											\
		if (IS_DSE_IMAGE)								\
		{										\
			gtm_errcode = MAKE_MSG_WARNING(gtm_errcode);				\
			gtm_putmsg_csa(CSA_ARG(REG2CSA(REG)) VARLSTCNT(4) gtm_errcode, 2,	\
					DB_LEN_STR(REG));					\
		} else										\
			rts_error_csa(CSA_ARG(REG2CSA(REG)) VARLSTCNT(4) gtm_errcode, 2,	\
					DB_LEN_STR(REG));					\
	}											\
}

#define REG2CSA(REG)	(((REG) && (REG)->dyn.addr && (REG)->dyn.addr->file_cntl) ? (&FILE_INFO(REG)->s_addrs) : NULL)
#define JCTL2CSA(JCTL)	(((JCTL) && (JCTL->reg_ctl)) ? (JCTL->reg_ctl->csa) : NULL)

typedef struct	file_control_struct
{
	sm_uc_ptr_t	op_buff;
	UNIX_ONLY(gtm_int64_t) VMS_ONLY(int4)	op_pos;
	int		op_len;
	void		*file_info;   /* Pointer for OS specific struct */
	char		file_type;
	char		op;
} file_control;

typedef struct	header_struct_struct
{
	char		label[12];
	unsigned	filesize;	/* size of file excluding GDE info */

	/* removed unused file_log struct */
} header_struct;

typedef struct gdr_name_struct
{
	mstr			name;
	mstr			exp_name;
	struct gdr_name_struct	*link;
	struct gd_addr_struct	*gd_ptr;
} gdr_name;

typedef struct	gd_addr_struct
{
	struct gd_region_struct		*local_locks;
	int4				max_rec_size;
	short				n_maps;
	short				n_regions;
	short				n_segments;
	short				filler;
	struct gd_binding_struct	*maps;
	struct gd_region_struct		*regions;
	struct gd_segment_struct	*segments;
	struct gd_addr_struct		*link;
	struct hash_table_mname_struct  *tab_ptr;
	gd_id				*id;
	UINTPTR_T			end;
} gd_addr;
typedef gd_addr *(*gd_addr_fn_ptr)();

typedef struct	gd_segment_struct
{
	unsigned short		sname_len;
	unsigned char		sname[MAX_SN_LEN + 1];
	unsigned short		fname_len;
	unsigned char		fname[MAX_FN_LEN + 1];
	unsigned short		blk_size;
	unsigned short		ext_blk_count;
	uint4			allocation;
	struct CLB		*cm_blk;
	unsigned char		defext[4];
	char			defer_time; 	/* Was passed in cs_addrs */
	unsigned char		file_type;
	unsigned char		buckets;	/* Was passed in FAB */
	unsigned char		windows;	/* Was passed in FAB */
	uint4			lock_space;
	uint4			global_buffers;	/* Was passed in FAB */
	uint4			reserved_bytes;	/* number of bytes to be left in every database block */
	enum db_acc_method	acc_meth;
	file_control		*file_cntl;
	struct gd_region_struct	*repl_list;
	UNIX_ONLY(boolean_t		is_encrypted;)
} gd_segment;

typedef union
{
	int4             offset;  /* relative offset to segment  */
	gd_segment       *addr;   /* absolute address of segment */
} gd_seg_addr;

typedef struct	gd_region_struct
{
	unsigned short		rname_len;
	unsigned char		rname[MAX_RN_LEN + 1];
	unsigned short		max_key_size;
	uint4			max_rec_size;
	gd_seg_addr		dyn;
	gd_seg_addr		stat;
	bool			open;
	bool			lock_write;	/* Field is not currently used by GT.M */
	char            	null_subs;	/* 0 ->NEVER(previous NO), 1->ALWAYS(previous YES), 2->ALLOWEXISTING
						* i.e. will allow read null subs but prohibit set */
	unsigned char 		jnl_state;

	/* deleted gbl_lk_root and lcl_lk_root, obsolete fields */

	uint4			jnl_alq;
#ifdef UNIX
	uint4			jnl_deq;
	uint4			jnl_autoswitchlimit;
	uint4			jnl_alignsize;		/* not used, reserved */
	int4			jnl_epoch_interval;	/* not used, reserved */
	int4			jnl_sync_io;		/* not used, reserved */
	int4			jnl_yield_lmt;		/* not used, reserved */
#else
	unsigned short		jnl_deq;
#endif
	unsigned short		jnl_buffer_size;
	bool			jnl_before_image;
	bool			opening;
	bool			read_only;
	bool			was_open;
	unsigned char		cmx_regnum;
	unsigned char		def_coll;
	bool			std_null_coll;	/* 0 -> GT.M null collation,i,e, null subs collate between numeric and string
						 * 1-> standard null collation i.e. null subs collate before numeric and string
						 */
#ifdef UNIX
	bool			freeze_on_fail;
	bool			mumps_can_bypass; /* Allow mumps processes to bypass flushing, access control, and ftok semaphore
						   * in gds_rundown(). This was done to improve shutdown performance.
						   */
#endif
	unsigned char		jnl_file_len;
	unsigned char		jnl_file_name[JNL_NAME_SIZE];

	/* VMS file id struct goes to OS specific struct */
	/* VMS lock structure for reference goes to OS specific struct */

	int4			node;
	int4			sec_size;
} gd_region;

typedef struct	sgmnt_addrs_struct
{
	sgmnt_data_ptr_t			hdr;
	sm_uc_ptr_t				bmm;
	sm_uc_ptr_t				wc;
	bt_rec_ptr_t				bt_header;
	bt_rec_ptr_t				bt_base;
	th_rec_ptr_t				th_base;
	th_index_ptr_t				ti;
	node_local_ptr_t			nl;
	mutex_struct_ptr_t			critical;
	struct shmpool_buff_hdr_struct		*shmpool_buffer;	/* 1MB chunk of shared memory that we micro manage */
	sm_uc_ptr_t				db_addrs[2];
	sm_uc_ptr_t				lock_addrs[2];
	struct gv_namehead_struct		*dir_tree;
#	ifdef GTM_TRIGGER
	struct gv_namehead_struct		*hasht_tree;
#	endif
	struct sgmnt_addrs_struct		*next_fenced;		/* NULL if db has journaling turned off (or disabled)
									 * Otherwise (db has journaling turned on), it is
									 *  NULL if this db was not updated in this TP/ZTP
									 *  non-NULL if this db was updated in this TP/ZTP
									 * The non-NULL value points to the next csa that
									 * has a non-NULL next_fenced value i.e. a linked list
									 * of csas. The end of the list is JNL_FENCE_LIST_END
									 * (cannot use NULL due to special meaning described
									 * above and hence using a macro which evaluates to -1).
									 */
	struct jnl_private_control_struct	*jnl;
	struct sgm_info_struct			*sgm_info_ptr;
	gd_region				*region;		/* the region corresponding to this csa */
	struct hash_table_mname_struct		*gvt_hashtab;		/* NON-NULL only if regcnt > 1;
								 	 * Maintains all gv_targets mapped to this db file */
	struct reg_ctl_list_struct	*rctl;	/* pointer to rctl for this region (used only if jgbl.forw_phase_recovery) */
	struct sgmnt_addrs_struct	*next_csa; /* points to csa of NEXT database that has been opened by this process */
#	ifdef GTM_CRYPT
	char					*encrypted_blk_contents;
	gtmcrypt_key_t				encr_key_handle;
#	endif
#	ifdef GTM_SNAPSHOT
	struct snapshot_context_struct 	*ss_ctx;
#	endif
	union
	{
		sgmm_addrs	mm;
		sgbg_addrs	bg;
		/* May add new pointers here for other methods or change to void ptr */
	} acc_meth;
	gvstats_rec_t		gvstats_rec;
	trans_num		dbsync_timer_tn;/* copy of csa->ti->curr_tn when csa->dbsync_timer became TRUE.
						 * used to check if any updates happened in between when we flushed all
						 * dirty buffers to disk and when the idle flush timer (5 seconds) popped.
						 */
	/* 8-byte aligned at this point on all platforms (32-bit, 64-bit or Tru64 which is a mix of 32-bit and 64-bit pointers) */
	size_t		fullblockwrite_len;	/* Length of a full block write */
	uint4		total_blks;		/* Last we knew, file was this big. Used to signal MM processing file was
						 * extended and needs to be remapped. In V55000 was used with BG to detect
						 * file truncates. It is no longer used for that purpose: it was not necessary
						 * in the first place because bitmap block validations in t_end/tp_tend prevent
						 * updates from trying to commit past the end of the file.
						 * See mu_truncate.c for more details.
						 */
	uint4		prev_free_blks;
	/* The following uint4's are treated as bools but must be 4 bytes to avoid interaction between
	   bools in interrupted routines and possibly lost data */
	volatile uint4	timer;                  /* This process has a timer for this region */
	volatile uint4	in_wtstart;		/* flag we are busy writing */
	volatile uint4	now_crit;		/* This process has the critical write lock */
	volatile uint4	wbuf_dqd;		/* A write buffer has been dequeued - signals that
						   extra cleanup required if die while on */
	uint4		stale_defer;		/* Stale processing deferred this region */
	boolean_t	freeze;
	volatile boolean_t	dbsync_timer;	/* whether a timer to sync the filehdr (and write epoch) is active */
	block_id	reorg_last_dest;	/* last destinition block used for swap */
	boolean_t	jnl_before_image;
	boolean_t	read_write;
	boolean_t	persistent_freeze;	/* if true secshr_db_clnup() won't unfreeze this region */
	/* The following 3 fields are in cs_addrs instead of in the file-header since they are a function
	 * of the journal-record sizes that can change with journal-version-numbers (for the same database).
	 */
	int4		pblk_align_jrecsize;	/* maximum size of a PBLK record with corresponding ALIGN record */
	int4		min_total_tpjnl_rec_size;	/* minimum journal space requirement for a TP transaction */
	int4		min_total_nontpjnl_rec_size;	/* minimum journal space requirement for a non-TP transaction */
	int4		jnl_state;		/* journaling state: it can be 0, 1 or 2 (same as enum jnl_state_codes in jnl.h) */
	int4		repl_state;		/* state of replication whether open/closed/was_open */
	uint4		crit_check_cycle;	/* Used to mark which regions in a transaction legiticamtely have crit */
	int4		backup_in_prog;		/* true if online backup in progress for this region (used in op_tcommit/tp_tend) */
	boolean_t	snapshot_in_prog;	/* true if snapshots are in progress for this region */
	int4		ref_cnt;		/* count of number of times csa->nl->ref_cnt was incremented by this process */
	int4		fid_index;		/* index for region ordering based on unique_id */
	boolean_t	do_fullblockwrites;	/* This region enabled for full block writes */
	int4		regnum;			/* Region number (region open counter) used by journaling so all tokens
						   have a unique prefix per region (and all regions have same prefix)
						*/
	int4		n_pre_read_trigger;	/* For update process to keep track of progress and when to trigger pre-read */
	uint4		jnlpool_validate_check;	/* See the comment above VALIDATE_INITIALIZED_JNLPOOL for details on this field */
	int4		regcnt;			/* # of regions that have this as their csa */
	boolean_t	t_commit_crit;		/* set to FALSE by default. set to TRUE if in the middle of database commit.
						 * if access method is BG, this assumes a multi-state value.
						 * FALSE -> T_COMMIT_CRIT_PHASE1 -> T_COMMIT_CRIT_PHASE2 -> FALSE
						 *           (bg_update_phase1)      (bg_update_phase2)      (finish commit)
						 */
	boolean_t	wcs_pidcnt_incremented;	/* set to TRUE if we incremented cnl->wcs_phase2_commit_pidcnt.
						 * used by secshr_db_clnup to decrement the shared counter. */
	boolean_t	incr_db_trigger_cycle;	/* set to FALSE by default. set to TRUE if trigger state change (in ^#t) occurs for
						 * any global in this database which means an increment to csa->db_trigger_cycle and
						 * csd->db_trigger_cycle. Currently used by MUPIP TRIGGER/$ZTRIGGER(), MUPIP RECOVER
						 * and UPDATE PROCESS
						 */
	uint4		db_trigger_cycle;	/* mirror of csd->db_trigger_cycle; used to detect concurrent ^#t global changes */
	uint4		db_dztrigger_cycle;	/* incremented on every $ZTRIGGER() operation. Due to the presence of $ZTRIGGER()
						 * and ZTRIGGER command the 'd' prefix for ztrigger in db_dztrigger_cycle is used
						 * to denote the '$' in $ZTRIGGER() */
	boolean_t	hold_onto_crit;		/* TRUE currently for dse if a CRIT -SEIZE has been done on this region.
						 * Set to FALSE by a DSE CRIT -RELEASE done on this region. Will also be TRUE in
						 * case of ONLINE ROLLBACK. Any code that can be invoked by both DSE and ROLLBACK
						 * should use csa->hold_onto_crit.
						 */
	boolean_t	dse_crit_seize_done;	/* TRUE if DSE does a CRIT -SEIZE for this region. Set to FALSE when CRIT -RELEASE
						 * or CRIT -REMOVE is done. Other than the -SEIZE and -RELEASE window, if any other
						 * DSE module sets csa->hold_onto_crit to TRUE (like dse_b_dmp) but encounters a
						 * runtime error before getting a chance to do a rel_crit, preemptive_db_clnup
						 * should know to release crit even if hold_onto_crit is set to TRUE and so will
						 * rely on this variable
						 */
#	ifdef UNIX
	uint4		root_search_cycle;	/* local copy of cnl->root_search_cycle */
	uint4		onln_rlbk_cycle;	/* local copy of cnl->onln_rlbk_cycle */
	uint4		db_onln_rlbkd_cycle;	/* local copy of cnl->db_onln_rlbkd_cycle */
	boolean_t	dbinit_shm_created;	/* TRUE if shared memory for this region was created by this process */
#	endif
} sgmnt_addrs;

typedef struct	gd_binding_struct
{
	unsigned char	name[MAX_NM_LEN + 1];
	union
	{
		gd_region	*addr;
		int4		offset;
	} reg;
} gd_binding;

typedef struct
{
	unsigned short	offset;
	unsigned short	match;
} srch_rec_status;

typedef struct srch_blk_status_struct
{
	cache_rec_ptr_t	cr;
	sm_uc_ptr_t     buffaddr;
	block_id	blk_num;
	trans_num	tn;
	srch_rec_status	prev_rec,
			curr_rec;
	int4		cycle;
	int4		level;
	struct cw_set_element_struct	*cse;
	struct srch_blk_status_struct	*first_tp_srch_status;	/* In TP, this points to an entry in the si->first_tp_hist array
								 * that contains the srch_blk_status structure of this block the
								 * first time it was referenced in this TP transaction. So basically
								 * gvt->hist contains pointers to the first_tp_hist array. At
								 * tp_clean_up time, the first_tp_hist array is cleared but all
								 * pointers to it are not cleaned up then. That instead happens
								 * when the gvt->clue gets used first in the next TP transaction,
								 * at which point we are guaranteed local_tn is much higher than
								 * gvt->read_local_tn which is an indication to complete this
								 * deferred cleanup.
								 * In non-TP, this field is maintained but not used.
								 */
	struct gv_namehead_struct	*blk_target;
} srch_blk_status;

/* Defines for "cycle" member in srch_blk_status.
 * For histories pointing to shared-memory buffers,
 *	"cycle" will be CYCLE_SHRD_COPY in MM and some positive number in BG.
 * For histories pointing to privately-built blocks,
 *	"cycle" will be CYCLE_PVT_COPY for both BG and MM.
 */
#define		CYCLE_PVT_COPY		-1
#define		CYCLE_SHRD_COPY		-2

typedef struct
{
	int4		depth;
	int4		filler;
	srch_blk_status	h[MAX_BT_DEPTH + 1];
} srch_hist;

#define SUPER_HIST_SIZE		2 * MAX_BT_DEPTH + 2	/* can be increased in the future to accommodate more than 2 histories */

/* Currently used only by MUPIP REORG in order to pass 3 search histories to t_end. */
typedef struct
{
	int4		depth;				/* t_end's validations don't depend on this field */
	int4		filler;
	srch_blk_status h[SUPER_HIST_SIZE];
} super_srch_hist;

#define MERGE_SUPER_HIST(SUPER_HIST, HIST1, HIST2)								\
{	/* Possible enhancement: do memcpy instead of loop */							\
	srch_hist 	*hist;											\
	srch_blk_status	*t0, *t1;										\
														\
	(SUPER_HIST)->depth = 1 + (HIST1)->depth + (HIST2)->depth;						\
	assert(SUPER_HIST_SIZE > (SUPER_HIST)->depth);								\
	t0 = (SUPER_HIST)->h;											\
	for (hist = (HIST1);  (NULL != hist);  hist = (hist == (HIST1)) ? (HIST2) : NULL)			\
	{													\
		for (t1 = hist->h;  t1->blk_num;  t1++)								\
		{												\
			*t0 = *t1;										\
			t0++;											\
		}												\
	}													\
	t0->blk_num = 0;											\
}

typedef struct	gv_key_struct
{
	unsigned short	top;		/* Offset to top of buffer allocated for the key */
	unsigned short	end;		/* End of the current key. Offset to the second null */
	unsigned short	prev;		/* Offset to the start of the previous subscript.
					 * This is used for global nakeds.
					 */
	unsigned char	base[1];	/* Base of the key */
} gv_key;

/* The direction that the newly added record went after a block split at a given level */
enum split_dir
{
	NEWREC_DIR_FORCED,	/* direction forced due to one of the sides being too-full i.e. no choice */
	NEWREC_DIR_LEFT,	/* new record went into the end of the left block after the split */
	NEWREC_DIR_RIGHT,	/* new record went into the beginning of the right block after the split */
};

/* Any change to this structure should also have a corresponding [re]initialization in mupip_recover.c
 * in the code where we play the records in the forward phase i.e. go through each of the jnl_files
 * and within if (mur_options.update), initialize necessary fields of gv_target before proceeding with mur_forward().
 */
typedef struct	gv_namehead_struct
{
	gv_key		*first_rec, *last_rec;		/* Boundary recs of clue's data block */
	struct gv_namehead_struct *next_gvnh;		/* Used to chain gv_target's together */
	struct gv_namehead_struct *prev_gvnh;		/* Used to chain gv_target's together */
	struct gv_namehead_struct *next_tp_gvnh;	/* Used to chain gv_targets participating in THIS TP transaction */
	sgmnt_addrs	*gd_csa;			/* Pointer to Segment corresponding to this key */
	srch_hist	*alt_hist;			/* alternate history. initialized once per gv_target */
	struct collseq_struct	*collseq;		/* pointer to a linked list of user supplied routine addresses
							   for internationalization */
	trans_num	read_local_tn;			/* local_tn of last reference for this global */
	GTMTRIG_ONLY(trans_num trig_local_tn;)		/* local_tn of last trigger driven for this global */
	GTMTRIG_ONLY(trans_num trig_read_tn;)		/* local_tn when triggers for this global (^#t records) were read from db */
	boolean_t	noisolation;     		/* whether isolation is turned on or off for this global */
	block_id	root;				/* Root of global variable tree */
	mname_entry	gvname;				/* the name of the global */
	NON_GTM64_ONLY(uint4	filler_8byte_align0;)	/* for 8-byte alignment of "hist" member */
	srch_hist	hist;				/* block history array */
	int4		regcnt;				/* number of global directories whose hash-tables point to this gv_target.
							 * 1 by default. > 1 if the same name in TWO DIFFERENT global directories
							 * maps to the same physical file (i.e. two regions in different global
							 * directories have the same physical file).
							 */
	unsigned char	nct;				/* numerical collation type for internalization */
	unsigned char	act;				/* alternative collation type for internalization */
	unsigned char	ver;
	bool		split_cleanup_needed;
	char		last_split_direction[MAX_BT_DEPTH - 1];	/* maintain last split direction for each level in the GVT */
	char		filler_8byte_align1[2];
	block_id	last_split_blk_num[MAX_BT_DEPTH - 1];
#	ifdef GTM_TRIGGER
	struct gvt_trigger_struct *gvt_trigger;		/* pointer to trigger info for this global
							 * (is non-NULL only if db_trigger_cycle is non-zero) */
	uint4		db_trigger_cycle;		/* copy of csd->db_trigger_cycle when triggers for this global were
							 * last read/initialized from ^#t global (in gvtr_init) */
	uint4		db_dztrigger_cycle;		/* copy of csa->db_dztrigger_cycle when triggers for this global were
							 * last read/initialized from ^#t global (in gvtr_init) */
	boolean_t	trig_mismatch_test_done;	/* whether update process has checked once if there is a mismatch
							 * in trigger definitions between originating and replicating instance */
	GTM64_ONLY(uint4 filler_8byte_align2;)		/* for 8-byte alignment of "clue" member. (targ_alloc relies on this) */
#	endif
	gv_key		clue;				/* Clue key, must be last in namehead struct because of hung buffer. */
} gv_namehead;

typedef struct	gvnh_reg_struct
{
	gv_namehead	*gvt;
	gd_region	*gd_reg;			/* Region of key */
} gvnh_reg_t;

#define INVALID_GV_TARGET (gv_namehead *)-1L

typedef struct gvsavtarg_struct
{
	gd_addr			*gd_targ_addr;
	gd_binding		*gd_map;
	gd_region		*gv_cur_region;
	gv_namehead		*gv_target;
	bool			gv_last_subsc_null;
	bool			gv_some_subsc_null;
	short			prev;
	short			end;
	short			filler_8byte_align;
} gvsavtarg_t;

#define	GVSAVTARG_ALIGN_BNDRY	8
#define	GVSAVTARG_FIXED_SIZE	(SIZEOF(gvsavtarg_t))

/* Following three macros define the mechanism to restore gv_target under normal and error conditions.
 * RESET_GV_TARGET should be used to restore gv_target from the global, reset_gv_target, only when we
 * are sure that this function is the first one in the call stack to have saved gv_target.
 * If the module that needs the restoration mechanism is not the first one to save gv_target in the call
 * stack, then one of the last two macros should be used.
 * RESET_GV_TARGET_LCL is used to restore gv_target from the local variable used to save gv_target.
 * RESET_GV_TARGET_LCL_AND_CLR_GBL is used at the end of the module, when there are no more gv_target
 * restorations needed. This resets gv_target and invalidates reset_gv_target.
 *
 * This mechanism ensures that, when there are multiple functions in a given call stack that save and
 * restore gv_target, only the bottom most function gets to store its value in the global, reset_gv_target.
 * In case of rts errors, if the error is not SUCCESS or INFO, then gv_target gets restored to reset_gv_target
 * (in preemptive_db_clnup()). For SUCCESS or INFO, no restoration is necessary because CONTINUE from the condition
 * handlers would take us through the normal path for gv_target restoration.
 */

#define	SKIP_GVT_GVKEY_CHECK		0
#define	DO_GVT_GVKEY_CHECK		1
#define	DO_GVT_GVKEY_CHECK_RESTART	2	/* do GVT_GVKEY check but skip gvt/csa check since we are in a TP transaction
						 * and about to restart, gv_target and cs_addrs will anyways get back in sync
						 * as part of the tp_restart process. This flag should be used only in TP
						 * as non-TP restart does not do this reset/sync.
						 */

#define RESET_GV_TARGET(GVT_GVKEY_CHECK)							\
{												\
	assert(INVALID_GV_TARGET != reset_gv_target);						\
	gv_target = reset_gv_target;								\
	reset_gv_target = INVALID_GV_TARGET;							\
	DEBUG_ONLY(										\
		if (GVT_GVKEY_CHECK)								\
			DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);			\
	)											\
}

#define RESET_GV_TARGET_LCL(SAVE_TARG)	gv_target = SAVE_TARG;

#define RESET_GV_TARGET_LCL_AND_CLR_GBL(SAVE_TARG, GVT_GVKEY_CHECK)							\
{															\
	GBLREF	uint4			dollar_tlevel;									\
															\
	gv_target = SAVE_TARG;												\
	if (!gbl_target_was_set)											\
	{														\
		assert(SAVE_TARG == reset_gv_target || INVALID_GV_TARGET == reset_gv_target);				\
		DEBUG_ONLY(												\
			if (DO_GVT_GVKEY_CHECK == (GVT_GVKEY_CHECK))							\
			{												\
				DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);					\
			} else												\
			{												\
				assert((SKIP_GVT_GVKEY_CHECK == (GVT_GVKEY_CHECK))					\
					|| (dollar_tlevel && (DO_GVT_GVKEY_CHECK_RESTART == (GVT_GVKEY_CHECK))));	\
				DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_FALSE);					\
			}												\
		)													\
		reset_gv_target = INVALID_GV_TARGET;									\
	}														\
}


/* No point doing the gvtarget-gvcurrkey in-sync check or the gvtarget-csaddrs in-sync check if we are anyways going to exit.
 * There is no way op_gvname (which is where these design assumptions get actually used) is going to be called from now onwards.
 */
GBLREF	int		process_exiting;
GBLREF	trans_num	local_tn;
GBLREF	gv_namehead	*gvt_tp_list;

#define	RESET_FIRST_TP_SRCH_STATUS_FALSE	FALSE
#define	RESET_FIRST_TP_SRCH_STATUS_TRUE		TRUE

#define	GVT_CLEAR_FIRST_TP_SRCH_STATUS(GVT)								\
{													\
	srch_blk_status	*srch_status;									\
													\
	assert(GVT->clue.end);	/* or else first_tp_srch_status will be reset as part of traversal */	\
	assert(GVT->read_local_tn != local_tn);								\
	for (srch_status = &(GVT)->hist.h[0]; HIST_TERMINATOR != srch_status->blk_num; srch_status++)	\
		srch_status->first_tp_srch_status = NULL;						\
}

#define	ADD_TO_GVT_TP_LIST(GVT, RESET_FIRST_TP_SRCH_STATUS)									\
{																\
	if (GVT->read_local_tn != local_tn)											\
	{	/* Set read_local_tn to local_tn; Also add GVT to list of gvtargets referenced in this TP transaction. */	\
		if (GVT->clue.end && RESET_FIRST_TP_SRCH_STATUS)								\
			GVT_CLEAR_FIRST_TP_SRCH_STATUS(GVT);									\
		GVT->read_local_tn = local_tn;											\
		GVT->next_tp_gvnh = gvt_tp_list;										\
		gvt_tp_list = GVT;												\
	} else															\
	{	/* Check that GVT is already part of the list of gvtargets referenced in this TP transaction */			\
		DBG_CHECK_IN_GVT_TP_LIST(GVT, TRUE);	/* TRUE => we check that GVT IS present in the gvt_tp_list */		\
	}															\
}

/* Although the below macros are used only in DBG code, they are passed as parameters so need to be defined for pro code too */
#define	CHECK_CSA_FALSE		FALSE
#define	CHECK_CSA_TRUE		TRUE

#ifdef DEBUG
#define	DBG_CHECK_IN_GVT_TP_LIST(gvt, present)						\
{											\
	gv_namehead	*gvtarg;							\
											\
	GBLREF gv_namehead	*gvt_tp_list;						\
	GBLREF  uint4           dollar_tlevel;						\
											\
	for (gvtarg = gvt_tp_list; NULL != gvtarg; gvtarg = gvtarg->next_tp_gvnh)	\
	{										\
		if (gvtarg == gvt)							\
			break;								\
	}										\
	assert(!present || (NULL != gvtarg));						\
	assert(present || (NULL == gvtarg) || (process_exiting && !dollar_tlevel));	\
}

#define	DBG_CHECK_GVT_IN_GVTARGETLIST(gvt)								\
{													\
	gv_namehead	*gvtarg;									\
													\
	GBLREF gd_region	*gv_cur_region;								\
	GBLREF gv_namehead	*gv_target_list;							\
													\
	for (gvtarg = gv_target_list; NULL != gvtarg; gvtarg = gvtarg->next_gvnh)			\
	{												\
		if (gvtarg == gvt)									\
			break;										\
	}												\
	/* For dba_cm or dba_usr type of regions, gv_target_list is not maintained so			\
	 * if gv_target is not part of gv_target_list, assert region is not BG or MM.			\
	 * The only exception is if the region was dba_cm but later closed due to an error on		\
	 * the server side (in which case access method gets reset back to BG. (e.g. gvcmz_error.c)	\
	 */												\
	assert((NULL != gvtarg) || (dba_cm == gv_cur_region->dyn.addr->acc_meth)			\
		|| (dba_usr == gv_cur_region->dyn.addr->acc_meth)					\
		|| ((FALSE == gv_cur_region->open) && (dba_bg == gv_cur_region->dyn.addr->acc_meth)));	\
}

/* If CHECK_CSADDRS input parameter is CHECK_CSA_TRUE, then check that GV_CURRKEY, GV_TARGET and CS_ADDRS are all in sync.
 * If CHECK_CSADDRS input parameter is CHECK_CSA_FALSE, then only check GV_CURRKEY and GV_TARGET are in sync (skip CS_ADDRS check).
 * The hope is that most callers of this macro use CHECK_CSA_TRUE (i.e. a stricter check).
 *
 * The DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSADDRS) macro is used at various points in the database code to check that
 * gv_currkey, gv_target and cs_addrs are in sync. This is because op_gvname relies on this in order to avoid a gv_bind_name
 * function call (if incoming key matches gv_currkey from previous call, it uses gv_target and cs_addrs right
 * away instead of recomputing them). The only exception is if we were interrupted in the middle of TP transaction by an
 * external signal which resulted in us terminating right away. In this case, we are guaranteed not to make a call to op_gvname
 * again (because we are exiting) so it is ok not to do this check if process_exiting is TRUE.
 */
#define	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSADDRS)									\
{																\
	mname_entry		*gvent;												\
	mstr			*varname;											\
	int			varlen;												\
	unsigned short		keyend;												\
	unsigned char		*keybase;											\
																\
	GBLREF int4	gv_keysize;												\
																\
	GBLREF gv_key		*gv_currkey;											\
	GBLREF gv_namehead	*reset_gv_target;										\
																\
	assert((NULL != gv_currkey) || (NULL == gv_target));									\
	/* make sure gv_currkey->top always reflects the maximum keysize across all dbs that we opened until now */		\
	assert((NULL == gv_currkey) || (gv_currkey->top == gv_keysize));							\
	if (!process_exiting)													\
	{															\
		keybase = &gv_currkey->base[0];											\
		if ((NULL != gv_currkey) && (0 != keybase[0]) && (INVALID_GV_TARGET == reset_gv_target))			\
		{														\
			assert(NULL != gv_target);										\
			gvent = &gv_target->gvname;										\
			varname = &gvent->var_name;										\
			varlen = varname->len;											\
			assert(varlen);												\
			assert((0 != keybase[varlen]) || !memcmp(keybase, varname->addr, varlen));				\
			keyend = gv_currkey->end;										\
			assert(!keyend || (KEY_DELIMITER == keybase[keyend]));							\
			assert(!keyend || (KEY_DELIMITER == keybase[keyend - 1]));						\
			/* Check that gv_target is part of the gv_target_list */						\
			DBG_CHECK_GVT_IN_GVTARGETLIST(gv_target);								\
			if (CHECK_CSADDRS)											\
				DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC;								\
		}														\
		/* Do gv_target sanity check too; Do not do this if it is NULL or if it is GT.CM GNP client (gd_csa is NULL) */	\
		if ((NULL != gv_target) && (NULL != gv_target->gd_csa))								\
			DBG_CHECK_GVTARGET_INTEGRITY(gv_target);								\
	}															\
}

/* Do checks on the integrity of various fields in gv_target. targ_alloc initializes these and they are supposed to
 * stay that way. The following code is very similar to that in targ_alloc so needs to be maintained in sync. This
 * macro expects that gv_target->gd_csa is non-NULL (could be NULL for GT.CM GNP client) so any callers of this macro
 * should ensure they do not invoke it in case of NULL gd_csa.
 */
#define	DBG_CHECK_GVTARGET_INTEGRITY(GVT)											\
{																\
	int			keysize, partial_size;										\
	GBLREF	boolean_t	dse_running;											\
																\
	keysize = GVT->gd_csa->hdr->max_key_size;										\
	keysize = DBKEYSIZE(keysize);												\
	partial_size = SIZEOF(gv_namehead) + 2 * SIZEOF(gv_key) + 3 * keysize;							\
	/* DSE could change the max_key_size dynamically so account for it in the below assert */				\
	if (!dse_running)													\
	{															\
		assert(GVT->gvname.var_name.addr == (char *)GVT + partial_size);						\
		assert((char *)GVT->first_rec == ((char *)&GVT->clue + SIZEOF(gv_key) + keysize));				\
		assert((char *)GVT->last_rec  == ((char *)GVT->first_rec + SIZEOF(gv_key) + keysize));				\
		assert(GVT->clue.top == keysize);										\
	}															\
	assert(GVT->clue.top == GVT->first_rec->top);										\
	assert(GVT->clue.top == GVT->last_rec->top);										\
}
#else
#	define	DBG_CHECK_IN_GVT_TP_LIST(gvt, present)
#	define	DBG_CHECK_GVT_IN_GVTARGETLIST(gvt)
#	define	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSADDRS)
#	define	DBG_CHECK_GVTARGET_INTEGRITY(GVT)
#endif

/* The below GBLREFs are for the following macro */
GBLREF	gv_namehead	*gv_target;
GBLREF	sgmnt_addrs	*cs_addrs;
#define	DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC	assert(process_exiting || (NULL == gv_target) || (gv_target->gd_csa == cs_addrs))

/* Indicate incompleteness of (potentially subscripted) global name by adding a "*" (without closing ")") at the end */
#define	GV_SET_LAST_SUBSCRIPT_INCOMPLETE(BUFF, END)			\
{									\
	if (NULL == (char *)(END))					\
	{	/* The buffer passed to format_targ_key was not enough	\
		 * for the transformation. We don't expect this. Handle	\
		 * it nevertheless by adding ",*" at end.		\
		 */							\
		assert(FALSE);						\
		END = ((unsigned char *)ARRAYTOP(BUFF)) - 1;		\
		assert((char *)(END) > (char *)(BUFF));			\
		*(END)++ = '*';						\
	} else								\
	{	/* Overflow occurred while adding the global name OR	\
		 * after adding the last subscript OR in the middle of	\
		 * adding a subscript (not necessarily last). In all	\
		 * cases, add a '*' at end to indicate incompleteness.	\
		 */							\
		if (')' == END[-1])					\
			(END)--;					\
		/* ensure we have space to write 1 byte */		\
		assert((char *)(END) + 1 <= ((char *)ARRAYTOP(BUFF)));	\
		*(END)++ = '*';						\
	}								\
}

#define	ISSUE_GVSUBOFLOW_ERROR(GVKEY)								\
{												\
	unsigned char *endBuff, fmtBuff[MAX_ZWR_KEY_SZ];					\
												\
	/* Assert that input key to format_targ_key is double null terminated */		\
	assert(KEY_DELIMITER == GVKEY->base[GVKEY->end]);					\
	endBuff = format_targ_key(fmtBuff, ARRAYSIZE(fmtBuff), GVKEY, TRUE);			\
	GV_SET_LAST_SUBSCRIPT_INCOMPLETE(fmtBuff, endBuff); /* Note: might update "endBuff" */	\
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2,		\
			endBuff - fmtBuff, fmtBuff);						\
}

#define COPY_SUBS_TO_GVCURRKEY(mvarg, max_key, gv_currkey, was_null, is_null)					\
{														\
	GBLREF mv_stent		*mv_chain;									\
	GBLREF unsigned char	*msp, *stackwarn, *stacktop;							\
	mval			temp;										\
	unsigned char		buff[MAX_ZWR_KEY_SZ], *end;							\
	int			len;										\
														\
	was_null |= is_null;											\
	if (mvarg->mvtype & MV_SUBLIT)										\
	{													\
		is_null = ((STR_SUB_PREFIX == *(unsigned char *)mvarg->str.addr)				\
					&& (KEY_DELIMITER == *(mvarg->str.addr + 1))); 				\
		if (gv_target->collseq || gv_target->nct)							\
		{												\
			/* collation transformation should be done at the server's end for CM regions */	\
			assert(dba_cm != gv_cur_region->dyn.addr->acc_meth);					\
			TREF(transform) = FALSE;								\
			end = gvsub2str((uchar_ptr_t)mvarg->str.addr, buff, FALSE);				\
			TREF(transform) = TRUE;									\
			temp.mvtype = MV_STR;									\
			temp.str.addr = (char *)buff;								\
			temp.str.len = (mstr_len_t)(end - buff);						\
			mval2subsc(&temp, gv_currkey);								\
		} else												\
		{												\
			len = mvarg->str.len;									\
			if (gv_currkey->end + len - 1 >= max_key)						\
				ISSUE_GVSUBOFLOW_ERROR(gv_currkey);						\
			memcpy((gv_currkey->base + gv_currkey->end), mvarg->str.addr, len);			\
			if (is_null && 0 != gv_cur_region->std_null_coll)					\
				gv_currkey->base[gv_currkey->end] = SUBSCRIPT_STDCOL_NULL;			\
			gv_currkey->prev = gv_currkey->end;							\
			gv_currkey->end += len - 1;								\
		}												\
	} else													\
	{													\
		MV_FORCE_DEFINED(mvarg);									\
		mval2subsc(mvarg, gv_currkey);									\
		if (gv_currkey->end >= max_key)									\
			ISSUE_GVSUBOFLOW_ERROR(gv_currkey);							\
		is_null = (MV_IS_STRING(mvarg) && (0 == mvarg->str.len));					\
	}													\
}

/* Copy GVKEY to GVT->CLUE. Take care NOT to copy cluekey->top to GVKEY->top as they correspond
 * to the allocation sizes of two different memory locations and should stay untouched.
 */
#define	COPY_CURRKEY_TO_GVTARGET_CLUE(GVT, GVKEY)				\
{										\
	gv_key	*cluekey;							\
										\
	if (GVT->clue.top <= GVKEY->end)					\
		GTMASSERT;							\
	assert(KEY_DELIMITER == GVKEY->base[GVKEY->end]);			\
	assert(KEY_DELIMITER == GVKEY->base[GVKEY->end - 1]);			\
	cluekey = &GVT->clue;							\
	memcpy(cluekey->base, GVKEY->base, GVKEY->end + 1);			\
	cluekey->end = GVKEY->end;						\
	cluekey->prev = GVKEY->prev;						\
	DBG_CHECK_GVTARGET_INTEGRITY(GVT);					\
}

/* If SRC_KEY->end == 0, make sure to copy the first byte of SRC_KEY->base */
#define MEMCPY_KEY(TARG_KEY, SRC_KEY)												\
{																\
	memcpy((TARG_KEY), (SRC_KEY), OFFSETOF(gv_key, base[0]) + (SRC_KEY)->end + 1);						\
}
#define COPY_KEY(TARG_KEY, SRC_KEY)										\
{																\
	assert(TARG_KEY->top >= SRC_KEY->end);											\
	/* ensure proper alignment before dereferencing SRC_KEY->end */								\
	assert(0 == (((UINTPTR_T)(SRC_KEY)) % SIZEOF(SRC_KEY->end)));								\
	/* WARNING: depends on the first two bytes of gv_key structure being key top field */					\
	assert((2 == SIZEOF(TARG_KEY->top)) && ((sm_uc_ptr_t)(TARG_KEY) == (sm_uc_ptr_t)(&TARG_KEY->top)));			\
	memcpy(((sm_uc_ptr_t)(TARG_KEY) + 2), ((sm_uc_ptr_t)(SRC_KEY) + 2), OFFSETOF(gv_key, base[0]) + (SRC_KEY)->end - 1);	\
}

/* Macro to denote special value of first_rec when it is no longer reliable */
#define	GVT_CLUE_FIRST_REC_UNRELIABLE	(short)0xffff

/* Macro to denote special value of last_rec when it is the absolute maximum (in case of *-keys all the way down) */
#define	GVT_CLUE_LAST_REC_MAXKEY	(short)0xffff

/* Macro to reset first_rec to a special value to indicate it is no longer reliable
 * (i.e. the keyrange [first_rec, clue] should not be used by gvcst_search.
 * Note that [clue, last_rec] is still a valid keyrange and can be used by gvcst_search.
 */
#define	GVT_CLUE_INVALIDATE_FIRST_REC(GVT)						\
{											\
	assert(GVT->clue.end);								\
	*((short *)GVT->first_rec->base) = GVT_CLUE_FIRST_REC_UNRELIABLE;		\
}

#ifdef DEBUG
/* Macro to check that the clue is valid. Basically check that first_rec <= clue <= last_rec. Also check that
 * all of them start with the same global name in case of a GVT. A clue that does not satisfy these validity
 * checks implies the possibility of DBKEYORD errors (e.g. C9905-001119 in VMS).
 */
#define	DEBUG_GVT_CLUE_VALIDATE(GVT)											\
{															\
	mname_entry		*gvent;											\
	unsigned short		klen;											\
	gv_namehead		*gvt;											\
															\
	/* Verify that clue->first_rec <= clue.base <= clue->last_rec.							\
	 * The only exception is if first_rec has been reset to an unreliable value.					\
	 */														\
	gvt = GVT; /* copy into local variable to avoid evaluating input multiple times */				\
	klen = MIN(gvt->clue.end, gvt->first_rec->end);									\
	assert(klen);													\
	assert((0 <= memcmp(gvt->clue.base, gvt->first_rec->base, klen))						\
		|| (GVT_CLUE_FIRST_REC_UNRELIABLE == *((short *)gvt->first_rec->base)));				\
	klen = MIN(gvt->clue.end, gvt->last_rec->end);									\
	assert(klen);													\
	assert(0 <= memcmp(gvt->last_rec->base, gvt->clue.base, klen));							\
	if (DIR_ROOT != gvt->root)											\
	{	/* Not a directory tree => a GVT tree, check that first_rec/last_rec have at least gvname in it */	\
		gvent = &gvt->gvname;											\
		if (GVT_CLUE_FIRST_REC_UNRELIABLE != *((short *)gvt->first_rec->base))					\
		{													\
			assert((0 == memcmp(gvent->var_name.addr, gvt->first_rec->base, gvent->var_name.len))		\
				&& (KEY_DELIMITER == gvt->first_rec->base[gvent->var_name.len]));			\
		}													\
		if (GVT_CLUE_LAST_REC_MAXKEY != *((short *)gvt->last_rec->base))					\
		{													\
			assert((0 == memcmp(gvent->var_name.addr, gvt->last_rec->base, gvent->var_name.len))		\
				&& (KEY_DELIMITER == gvt->last_rec->base[gvent->var_name.len]));			\
		}													\
	}														\
}
#else
#define	DEBUG_GVT_CLUE_VALIDATE(GVT)
#endif

/* Macro used by $ZPREVIOUS to replace a NULL subscript at the end with the maximum possible subscript
 * that could exist in the database for this global name.
 */
#define GVZPREVIOUS_APPEND_MAX_SUBS_KEY(GVKEY, GVT)					\
{											\
	int		lastsubslen, keysize;						\
	unsigned char	*ptr;								\
											\
	assert(GVT->clue.top || (NULL == GVT->gd_csa));					\
	assert(!GVT->clue.top || (NULL != GVT->gd_csa) && (GVT->gd_csa == cs_addrs));	\
	/* keysize can be obtained from GVT->clue.top in case of GT.M.			\
	 * But for GT.CM client, clue will be uninitialized. So we would need to	\
	 * compute keysize from gv_cur_region->max_key_size. Since this is true for	\
	 * GT.M as well, we use the same approach for both to avoid an if check and a	\
	 * break in the pipeline.							\
	 */										\
	keysize = DBKEYSIZE(gv_cur_region->max_key_size);				\
	assert(!GVT->clue.top || (keysize == GVT->clue.top));				\
	lastsubslen = keysize - GVKEY->prev - 2;					\
	if ((0 < lastsubslen) && (GVKEY->top >= keysize) && (GVKEY->end > GVKEY->prev))	\
	{										\
		ptr = &GVKEY->base[GVKEY->prev];					\
		memset(ptr, STR_SUB_MAXVAL, lastsubslen);				\
		ptr += lastsubslen;							\
		*ptr++ = KEY_DELIMITER;	 /* terminator for last subscript */		\
		*ptr = KEY_DELIMITER;    /* terminator for entire key */		\
		GVKEY->end = GVKEY->prev + lastsubslen + 1;				\
		assert(GVKEY->end == (ptr - &GVKEY->base[0]));				\
	} else										\
		GTMASSERT;								\
	if (NULL != gv_target->gd_csa)							\
		DBG_CHECK_GVTARGET_INTEGRITY(GVT);					\
}

/* Bit masks for the update_trans & si->update_trans variables */
#define	UPDTRNS_DB_UPDATED_MASK		(1 << 0)	/* 1 if this region was updated by this non-TP/TP transaction */
#define	UPDTRNS_JNL_LOGICAL_MASK	(1 << 1)	/* 1 if logical jnl record was written in this region's
	          					 * journal file by this TP transaction. Maintained only for TP.
							 */
#define	UPDTRNS_JNL_REPLICATED_MASK	(1 << 2)	/* 1 if there is at least one logical jnl record written in this
							 * region's journal file by this TP transaction that needs to be
							 * replicated across. 0 if all updates done to this region was
							 * inside of a trigger. Maintained only for TP.
							 */
#define	UPDTRNS_TCOMMIT_STARTED_MASK	(1 << 3)	/* 1 if non-TP or TP transaction is beyond the point of rolling
							 * back by "t_commit_cleanup" and can only be rolled forward by
							 * "secshr_db_clnup".
							 */
#define UPDTRNS_ZTRIGGER_MASK		(1 << 4)	/* 1 if ZTRIGGER command was done in this transaction. This allows
							 * the transaction to be committed even if it had no updates.
							 * Maintained only for TP.
							 */
#define	UPDTRNS_VALID_MASK		(UPDTRNS_DB_UPDATED_MASK | UPDTRNS_JNL_LOGICAL_MASK		\
					 | UPDTRNS_JNL_REPLICATED_MASK | UPDTRNS_TCOMMIT_STARTED_MASK	\
					 | UPDTRNS_ZTRIGGER_MASK)

/* The enum codes below correspond to code-paths that can increment the database curr_tn
 * without having a logical update. Journaling currently needs to know all such code-paths */
typedef enum
{
	inctn_invalid_op = 0,		/*  0 : */
	/* the following opcodes do NOT populate the global variable "inctn_detail" */
	inctn_gvcstput_extra_blk_split,	/*  1 : */
	inctn_mu_reorg,			/*  2 : */
	inctn_wcs_recover,		/*  3 : */
	/* the following opcodes populate "inctn_detail.blks2upgrd_struct" */
	inctn_gdsfilext_gtm,		/*  4 : */
	inctn_gdsfilext_mu_reorg,	/*  5 : */
	inctn_db_format_change,		/*  6 : written when cs_data->desired_db_format changes */
	/* the following opcodes populate "inctn_detail.blknum_struct" */
	inctn_bmp_mark_free_gtm,	/*  7 : */
	inctn_bmp_mark_free_mu_reorg,	/*  8 : */
	inctn_blkmarkfree,		/*  9 : a RECYCLED block being marked free by MUPIP REORG UPGRADE/DOWNGRADE */
	inctn_blkupgrd,			/* 10 : written whenever a GDS block is upgraded by MUPIP REORG UPGRADE if
					 *      a) SAFEJNL is specified OR
					 *      b) NOSAFEJNL is specified and the block is not undergoing a fmt change
					 */
	inctn_blkupgrd_fmtchng,		/* 11 : written whenever a GDS block is upgraded by MUPIP REORG UPGRADE -NOSAFEJNL
					 *      and if that block is undergoing a fmt change i.e. (V4 -> V5) OR (V5 -> V4).
					 *      This differentiation (inctn_blkupgrd vs inctn_blkupgrd_fmtch) is necessary
					 *      because in the latter case we will not be writing a PBLK record and hence
					 *      have no record otherwise of a block fmt change if it occurs (note that a
					 *      PBLK journal record's "ondsk_blkver" field normally helps recovery
					 *      determine if a fmt change occurred or not).
					 */
	inctn_blkdwngrd,		/* 12 : similar to inctn_blkupgrd except that this is for DOWNGRADE */
	inctn_blkdwngrd_fmtchng,	/* 13 : similar to inctn_blkupgrd_fmtchng except that this is for DOWNGRADE */
	/* the following opcodes do NOT populate the global variable "inctn_detail" */
	inctn_opcode_total		/* 15 : MAX. All additions of inctn opcodes should be done BEFORE this line */
} inctn_opcode_t;

/* macros to check curr_tn */
#define MAX_TN_V4	((trans_num)(MAXUINT4 - TN_HEADROOM_V4))
#define MAX_TN_V6	(MAXUINT8 - TN_HEADROOM_V6)
#define TN_HEADROOM_V4	(2 * MAXTOTALBLKS_V4)
#define TN_HEADROOM_V6	(2 * MAXTOTALBLKS_V6)
#define	HEADROOM_FACTOR	4

/* the following macro checks that curr_tn < max_tn_warn <= max_tn.
 * if not, it adjusts max_tn_warn accordingly to ensure the above.
 * if not possible, it issues TNTOOLARGE error.
 */
#define CHECK_TN(CSA, CSD, TN)												\
{															\
	assert((CSA)->hdr == (CSD));											\
	assert((TN) <= (CSD)->max_tn_warn);										\
	assert((CSD)->max_tn_warn <= (CSD)->max_tn);									\
	assert((CSA)->now_crit);	/* Must be crit to mess with stuff */						\
	if ((TN) >= (CSD)->max_tn_warn)											\
	{														\
		trans_num trans_left;											\
															\
		if ((CSA)->hdr->max_tn <= (TN))										\
		{													\
			rts_error_csa(CSA_ARG(CSA) VARLSTCNT(5) ERR_TNTOOLARGE, 3, DB_LEN_STR((CSA)->region),		\
					&(CSA)->hdr->max_tn);								\
			assert(FALSE);	/* should not come here */							\
		}													\
		assert((CSD)->max_tn > (TN));										\
		trans_left = (CSD)->max_tn - (TN);									\
		send_msg_csa(CSA_ARG(CSA) VARLSTCNT(6) ERR_TNWARN, 4, DB_LEN_STR((CSA)->region), &trans_left,		\
				&(CSD)->max_tn);									\
		(CSD)->max_tn_warn = (TN) + 1 + ((trans_left - 1) >> 1);						\
		assert((TN) < (CSD)->max_tn_warn);									\
		assert((CSD)->max_tn_warn <= (CSD)->max_tn);								\
	}														\
}

#define	INCREMENT_CURR_TN(CSD)							\
{										\
	assert((CSD)->trans_hist.curr_tn < (CSD)->max_tn_warn);			\
	assert((CSD)->max_tn_warn <= (CSD)->max_tn);				\
	(CSD)->trans_hist.curr_tn++;						\
	assert((CSD)->trans_hist.curr_tn == (CSD)->trans_hist.early_tn);	\
}

#define SET_TN_WARN(CSD, ret_warn_tn)									\
{													\
	trans_num	headroom;									\
													\
	headroom = (gtm_uint64_t)(GDSV4 == (CSD)->desired_db_format ? TN_HEADROOM_V4 : TN_HEADROOM_V6);	\
	headroom *= HEADROOM_FACTOR;									\
	(ret_warn_tn) = (CSD)->trans_hist.curr_tn;							\
	if ((headroom < (CSD)->max_tn) && ((ret_warn_tn) < ((CSD)->max_tn - headroom)))			\
		(ret_warn_tn) = (CSD)->max_tn - headroom;						\
	assert((CSD)->trans_hist.curr_tn <= (ret_warn_tn));						\
	assert((ret_warn_tn) <= (CSD)->max_tn);								\
}

#define HIST_TERMINATOR		0
#define HIST_SIZE(h)		( (SIZEOF(int4) * 2) + (SIZEOF(srch_blk_status) * ((h).depth + 1)) )

/* Start of lock space in a bg file, therefore also doubles as overhead size for header, bt and wc queues F = # of wc blocks */
#define LOCK_BLOCK(X) 		(DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(X) + BT_SIZE(X), DISK_BLOCK_SIZE))
#define LOCK_BLOCK_SIZE(X)	(DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(X) + BT_SIZE(X), OS_PAGE_SIZE))
#define	LOCK_SPACE_SIZE(X)	(ROUND_UP2(((sgmnt_data_ptr_t)X)->lock_space_size, OS_PAGE_SIZE))
/* In case of an encrypted database, we maintain both encrypted and decrypted versions of the block in shared memory
 * in parallel arrays of global buffers hence the doubling calculation below. Although this doubles the shared memory
 * size requirements for encrypted databases (when compared to the same unencrypted database), it helps in other ways.
 * By ensuring that this encrypted global buffer array contents are identical to the encrypted on-disk block contents
 * of database blocks at all times, we can avoid allocating process private memory to store encrypted before-images
 * (to write to a journal file). Instead processes can use the encrypted global buffer directly for this purpose.
 * In user environments where process-private memory is very costly compared to database shared memory (e.g. where
 * 1000s of GT.M processes run against the same database) the above approach is expected to use lesser total memory.
 */
#define CACHE_CONTROL_SIZE(X)												\
	(ROUND_UP((ROUND_UP((X->bt_buckets + X->n_bts) * SIZEOF(cache_rec) + SIZEOF(cache_que_heads), OS_PAGE_SIZE)	\
		+ ((gtm_uint64_t)X->n_bts * X->blk_size * (X->is_encrypted ? 2 : 1))), OS_PAGE_SIZE))

OS_PAGE_SIZE_DECLARE

#ifdef VMS
#define MAX_NAME_LEN		31	/* Size of a repl resource name on vvms */
#endif
/* structure to identify a given system wide shared section to be ours (replic section) */
typedef struct
{
	unsigned char	label[GDS_LABEL_SZ];
	char		pool_type;
	char		now_running[MAX_REL_NAME];
#ifdef VMS
	char		repl_pool_key[MAX_NAME_LEN + 1];	/* resource name for the section */
	char 		filler[7];			/* makes sure the size of the structure is a multiple of 8 */
	char		gtmgbldir[MAX_FN_LEN + 1];	/* Identify which instance of this shared pool corresponds to */
#else
	int4		repl_pool_key_filler;		/* makes sure the size of the structure is a multiple of 8 */
	char		instfilename[MAX_FN_LEN + 1];	/* Identify which instance file this shared pool corresponds to */
#endif
} replpool_identifier;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef replpool_identifier 	*replpool_id_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

/* Macro to increment the count of processes that are doing two phase commit.
 * This is invoked just BEFORE starting phase1 of the commit.
 */
#define	INCR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl)				\
{									\
	assert(!csa->wcs_pidcnt_incremented);				\
	INCR_CNT(&cnl->wcs_phase2_commit_pidcnt, &cnl->wc_var_lock);	\
	csa->wcs_pidcnt_incremented = TRUE;				\
}

/* Macro to decrement the count of processes that are doing two phase commit.
 * This is invoked just AFTER finishing phase2 of the commit.
 */
#define	DECR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl)				\
{									\
	assert(csa->wcs_pidcnt_incremented);				\
	csa->wcs_pidcnt_incremented = FALSE;				\
	DECR_CNT(&cnl->wcs_phase2_commit_pidcnt, &cnl->wc_var_lock);	\
}

/* The CAREFUL_DECR_WCS_PHASE2_COMMIT_PIDCNT macro is the same as the DECR_WCS_PHASE2_COMMIT_PIDCNT macro
 * except that it uses CAREFUL_DECR_CNT instead of DECR_CNT. This does alignment checks and is needed by
 * secshr_db_clnup as it runs in kernel mode in VMS. The two macros should be maintained in parallel.
 */
#define	CAREFUL_DECR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl)				\
{										\
	assert(csa->wcs_pidcnt_incremented);					\
	csa->wcs_pidcnt_incremented = FALSE;					\
	CAREFUL_DECR_CNT(&cnl->wcs_phase2_commit_pidcnt, &cnl->wc_var_lock);	\
}

#ifdef UNIX
/* Insert the process_id into the list of process ids actively doing a kill */
#define INSERT_KIP_PID(local_csa)						\
{										\
	int		idx;							\
	uint4		pid;							\
	pid_t		*kip_pid_arr_ptr;					\
	GBLREF uint4	process_id;						\
										\
	kip_pid_arr_ptr = local_csa->nl->kip_pid_array;				\
	assert(local_csa->now_crit);						\
	for (idx = 0; idx < MAX_KIP_PID_SLOTS; idx++)				\
	{									\
		pid = kip_pid_arr_ptr[idx];					\
		if ((0 == pid) || (process_id == pid))				\
		{								\
			kip_pid_arr_ptr[idx] = process_id;			\
			break;							\
		}								\
	}									\
}
/* Remove the process_id from the list of process ids actively doing a kill */
#define REMOVE_KIP_PID(local_csa)						\
{										\
	int		idx;							\
	pid_t		*kip_pid_arr_ptr;					\
	GBLREF uint4	process_id;						\
										\
	kip_pid_arr_ptr = local_csa->nl->kip_pid_array;				\
	for (idx = 0; idx < MAX_KIP_PID_SLOTS; idx++)				\
	{									\
		if (process_id == kip_pid_arr_ptr[idx])				\
		{								\
			kip_pid_arr_ptr[idx] = 0;				\
			break;							\
		}								\
	}									\
}
#else
#define INSERT_KIP_PID(local_csa)
#define REMOVE_KIP_PID(local_csa)
#endif

#define DECR_KIP(CSD, CSA, KIP_CSA)						\
{										\
	sgmnt_data_ptr_t	local_csd;					\
	sgmnt_addrs		*local_csa;					\
										\
	/* Instead of using CSA and CSD directly in DECR_CNT, assign it to 	\
	 * local variables as the caller can potentially pass the global 	\
	 * kip_csa as the second argument(which also happens to be the 		\
	 * the third argument which will be reset to NULL below) thereby	\
	 * leading to SEG faults in the calls to DECR_CNT. Similar 		\
	 * modifications are in INCR_KIP and their CAREFUL counterparts */	\
	local_csd = CSD;							\
	local_csa = CSA;							\
	assert(NULL != KIP_CSA);						\
	KIP_CSA = NULL;								\
	DECR_CNT(&local_csd->kill_in_prog, &local_csa->nl->wc_var_lock);	\
	REMOVE_KIP_PID(local_csa);						\
}
/* Note that the INCR_KIP and CAREFUL_INCR_KIP macros should be maintained in parallel */
#define INCR_KIP(CSD, CSA, KIP_CSA)						\
{										\
	sgmnt_data_ptr_t	local_csd;					\
	sgmnt_addrs		*local_csa;					\
										\
	local_csd = CSD;							\
	local_csa = CSA;							\
	assert(NULL == KIP_CSA);						\
	INCR_CNT(&local_csd->kill_in_prog, &local_csa->nl->wc_var_lock);	\
	INSERT_KIP_PID(local_csa);						\
	KIP_CSA = CSA;								\
}
/* The CAREFUL_INCR_KIP macro is the same as the INCR_KIP macro except that it uses CAREFUL_INCR_CNT instead of INCR_CNT.
 * This does alignment checks and is needed by secshr_db_clnup as it runs in kernel mode in VMS.
 * The INCR_KIP and CAREFUL_INCR_KIP macros should be maintained in parallel.
 */
#define CAREFUL_INCR_KIP(CSD, CSA, KIP_CSA)						\
{											\
	sgmnt_data_ptr_t	local_csd;						\
	sgmnt_addrs		*local_csa;						\
											\
	local_csd = CSD;								\
	local_csa = CSA;								\
	assert(NULL == KIP_CSA);							\
	CAREFUL_INCR_CNT(&local_csd->kill_in_prog, &local_csa->nl->wc_var_lock);	\
	INSERT_KIP_PID(local_csa);							\
	KIP_CSA = CSA;									\
}
#define CAREFUL_DECR_KIP(CSD, CSA, KIP_CSA)						\
{											\
	sgmnt_data_ptr_t	local_csd;						\
	sgmnt_addrs		*local_csa;						\
											\
	local_csd = CSD;								\
	local_csa = CSA;								\
	assert(NULL != KIP_CSA);							\
	KIP_CSA = NULL;									\
	CAREFUL_DECR_CNT(&local_csd->kill_in_prog, &local_csa->nl->wc_var_lock);	\
	REMOVE_KIP_PID(local_csa);							\
}
/* Since abandoned_kills counter is only incremented in secshr_db_clnup it does not have its equivalent DECR_ABANDONED_KILLS */
#define CAREFUL_INCR_ABANDONED_KILLS(CSD, CSA)				\
{									\
        CAREFUL_INCR_CNT(&CSD->abandoned_kills, &CSA->nl->wc_var_lock);	\
}

#define INCR_INHIBIT_KILLS(CNL)					\
{								\
	INCR_CNT(&CNL->inhibit_kills, &CNL->wc_var_lock);	\
}

#define DECR_INHIBIT_KILLS(CNL)						\
{									\
	if (0 < CNL->inhibit_kills)					\
		DECR_CNT(&CNL->inhibit_kills, &CNL->wc_var_lock);	\
}

/* Commands like MUPIP BACKUP, MUPIP INTEG -REG or MUPIP FREEZE wait for kills-in-prog flag to become zero.
 * While these process wait for ongoing block-freeing KILLs (or reorg actions that free up blocks) to complete,
 * new block-freeing KILLs (or reorg actions that free up blocks) are deferred using inhibit_kills counter.
 * New block-freeing KILLs/REORG will wait for a maximum period of 1 minute until inhibit_kills counter is 0.
 * In case of timeout, they will proceed after resetting the inhibit_kills to 0. The reset is done in case
 * the inhibit_kills was orphaned (i.e. the process that set it got killed before it got a chance to reset).
 */
#define	WAIT_ON_INHIBIT_KILLS(CNL, MAXKILLINHIBITWAIT)				\
{										\
	int4			sleep_counter;					\
										\
	GBLREF	boolean_t	need_kip_incr;					\
	GBLREF	uint4		dollar_tlevel;					\
										\
	assert(dollar_tlevel || need_kip_incr);					\
	for (sleep_counter = 1; (0 < CNL->inhibit_kills); ++sleep_counter)	\
	{									\
		if (MAXKILLINHIBITWAIT <= sleep_counter)			\
		{								\
			CNL->inhibit_kills = 0;					\
			SHM_WRITE_MEMORY_BARRIER;				\
			break;							\
		}								\
		wcs_sleep(sleep_counter);					\
	}									\
}

/* Wait for a region freeze to be turned off. Note that we dont hold CRIT at this point. Ideally we would have
 * READ memory barriers between each iterations of sleep to try and get the latest value of the "freeze" field from
 * the concurrently updated database shared memory. But since region-freeze is a perceivably rare event, we choose
 * not to do the memory barriers. The consequence of this decision is that it might take more iterations for us to
 * see updates to the "freeze" field than it would have if we did the memory barrier each iteration. But since we
 * dont hold crit at this point AND since freeze is a rare event, we dont mind the extra wait.
 */
#define MAXHARDCRITS		31

#define	WAIT_FOR_REGION_TO_UNFREEZE(CSA, CSD)		\
{							\
	int	lcnt1;					\
							\
	assert(CSA->hdr == CSD);			\
	assert(!CSA->now_crit);				\
	for (lcnt1 = 1; ; lcnt1++)			\
	{						\
		if (!CSD->freeze)			\
			break;				\
		if (MAXHARDCRITS < lcnt1)       	\
			wcs_backoff(lcnt1);     	\
	}						\
}

#define	GRAB_UNFROZEN_CRIT(reg, csa, csd)				\
{									\
	int	lcnt;							\
									\
	assert(&FILE_INFO(reg)->s_addrs == csa && csa->hdr == csd);	\
	assert(csa->now_crit);						\
	for (lcnt = 0; ; lcnt++)					\
	{								\
		if (!csd->freeze)					\
			break;						\
		rel_crit(reg);						\
		WAIT_FOR_REGION_TO_UNFREEZE(csa, csd);			\
		grab_crit(reg);						\
	}								\
	assert(!csd->freeze && csa->now_crit);				\
}
/* remove "csa" from list of open regions (cs_addrs_list) */
#define	REMOVE_CSA_FROM_CSADDRSLIST(CSA)						\
{											\
	GBLREF	sgmnt_addrs	*cs_addrs_list;						\
											\
	sgmnt_addrs	*tmpcsa, *prevcsa;						\
											\
	assert(NULL != CSA);								\
	assert(NULL == CSA->nl);							\
	prevcsa = NULL;									\
	for (tmpcsa = cs_addrs_list; NULL != tmpcsa; tmpcsa = tmpcsa->next_csa)		\
	{										\
		if (CSA == tmpcsa)							\
			break;								\
		prevcsa = tmpcsa;							\
	}										\
	/* tmpcsa could not be equal to CSA in case CSA was never added to this list	\
	 * (possible in case of errors during gvcst_init). In dbg, the only case we 	\
	 * know of this is if an external signal causes exit processing before db_init	\
	 * completes. Assert accordingly.						\
	 */										\
	assert((tmpcsa == CSA) || process_exiting);					\
	if (tmpcsa == CSA)								\
	{										\
		if (NULL != prevcsa)							\
			prevcsa->next_csa = CSA->next_csa;				\
		else									\
			cs_addrs_list = CSA->next_csa;					\
	}										\
}

#define INVALID_SEMID			-1
#define INVALID_SHMID 			-1L
#ifdef VMS
#define NEW_DBINIT_SHM_IPC_MASK		(1 << 0)	/* 1 if db_init created a new shared memory (no pre-existing one) */
#define NEW_DBINIT_SEM_IPC_MASK		(1 << 1)	/* 1 if db_init created a new access control semaphore */
#endif

#define RESET_SHMID_CTIME(X)		\
{					\
	(X)->shmid = INVALID_SHMID;	\
	(X)->gt_shm_ctime.ctime = 0;	\
}

#define RESET_SEMID_CTIME(X)		\
{					\
	(X)->semid = INVALID_SEMID;	\
	(X)->gt_sem_ctime.ctime = 0;	\
}

#define RESET_IPC_FIELDS(X)		\
{					\
	RESET_SHMID_CTIME(X);		\
	RESET_SEMID_CTIME(X);		\
}

#if defined(UNIX)
#define DB_FSYNC(reg, udi, csa, db_fsync_in_prog, save_errno)	\
{								\
	int	rc;						\
								\
	BG_TRACE_PRO_ANY(csa, n_db_fsyncs);			\
	if (csa->now_crit)					\
		BG_TRACE_PRO_ANY(csa, n_db_fsyncs_in_crit);	\
	db_fsync_in_prog++;					\
	save_errno = 0;						\
	GTM_DB_FSYNC(csa, udi->fd, rc);				\
	if (-1 == rc)						\
		save_errno = errno;				\
	db_fsync_in_prog--;					\
	assert(0 <= db_fsync_in_prog);				\
}

#define STANDALONE(x) mu_rndwn_file(x, TRUE)
#define DBFILOP_FAIL_MSG(status, msg)	gtm_putmsg_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(5) msg, 2,			\
							DB_LEN_STR(gv_cur_region), status);
#elif defined(VMS)
#define STANDALONE(x) mu_rndwn_file(TRUE)	/* gv_cur_region needs to be equal to "x" */
#define DBFILOP_FAIL_MSG(status, msg)	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) msg, 2, DB_LEN_STR(gv_cur_region), status,	\
						   FILE_INFO(gv_cur_region)->fab->fab$l_stv);
#else
#error unsupported platform
#endif

#define CR_NOT_ALIGNED(cr, cr_base)		(!IS_PTR_ALIGNED((cr), (cr_base), SIZEOF(cache_rec)))
#define CR_NOT_IN_RANGE(cr, cr_lo, cr_hi)	(!IS_PTR_IN_RANGE((cr), (cr_lo), (cr_hi)))

/* Examine that cr->buffaddr is indeed what it should be. If not, this macro fixes its value by
 * recomputing from the cache_array.
 * NOTE: We rely on bt_buckets, n_bts and blk_size fields of file header being correct/not corrupt */
#define CR_BUFFER_CHECK(reg, csa, csd, cr)							\
{												\
	cache_rec_ptr_t		cr_lo, cr_hi;							\
												\
	cr_lo = (cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;	\
	cr_hi = cr_lo + csd->n_bts;								\
	CR_BUFFER_CHECK1(reg, csa, csd, cr, cr_lo, cr_hi);					\
}

/* A more efficient macro than CR_BUFFER_CHECK when we have cr_lo and cr_hi already available */
#define CR_BUFFER_CHECK1(reg, csa, csd, cr, cr_lo, cr_hi)					\
{												\
	INTPTR_T bp, bp_lo, bp_top, cr_top;							\
												\
	cr_top = GDS_ANY_ABS2REL(csa, cr_hi);							\
	bp_lo = ROUND_UP(cr_top, OS_PAGE_SIZE);							\
	bp = bp_lo + ((cr) - (cr_lo)) * csd->blk_size;						\
	if (bp != cr->buffaddr)									\
	{											\
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),	\
				cr, cr->blk, RTS_ERROR_TEXT("cr->buffaddr"),			\
				cr->buffaddr, bp, CALLFROM);					\
		cr->buffaddr = bp;								\
	}											\
	DEBUG_ONLY(bp_top = bp_lo + (gtm_uint64_t)csd->n_bts * csd->blk_size;)			\
	assert(IS_PTR_IN_RANGE(bp, bp_lo, bp_top) && IS_PTR_ALIGNED(bp, bp_lo, csd->blk_size));	\
}

#define DB_INFO	UNIX_ONLY(unix_db_info)VMS_ONLY(vms_gds_info)

#define FILE_CNTL_INIT_IF_NULL(SEG)								\
{												\
	file_control	*lcl_fc;								\
												\
	lcl_fc = SEG->file_cntl;								\
	if (NULL == lcl_fc)									\
	{											\
		MALLOC_INIT(lcl_fc, SIZEOF(file_control));					\
		SEG->file_cntl = lcl_fc;							\
	}											\
	if (NULL == lcl_fc->file_info)								\
	{											\
		MALLOC_INIT(lcl_fc->file_info, SIZEOF(DB_INFO));				\
		SEG->file_cntl->file_info = lcl_fc->file_info;					\
	}											\
}
#define FILE_CNTL_INIT(SEG)									\
{												\
	file_control	*lcl_fc;								\
												\
	MALLOC_INIT(lcl_fc, SIZEOF(file_control));						\
	MALLOC_INIT(lcl_fc->file_info, SIZEOF(DB_INFO));					\
	SEG->file_cntl = lcl_fc;								\
}

#define	IS_DOLLAR_INCREMENT			((is_dollar_incr) && (ERR_GVPUTFAIL == t_err))

#define AVG_BLKS_PER_100_GBL		200
#define PRE_READ_TRIGGER_FACTOR		50
#define UPD_RESERVED_AREA		50
#define UPD_WRITER_TRIGGER_FACTOR	33

#define ONLY_SS_BEFORE_IMAGES(CSA) (CSA->snapshot_in_prog && !CSA->backup_in_prog && !(JNL_ENABLED(CSA) && CSA->jnl_before_image))

#define SET_SNAPSHOTS_IN_PROG(X)	((X)->snapshot_in_prog = TRUE)
#define CLEAR_SNAPSHOTS_IN_PROG(X)	((X)->snapshot_in_prog = FALSE)
#ifdef GTM_SNAPSHOT
# define SNAPSHOTS_IN_PROG(X)	((X)->snapshot_in_prog)
/* Creates a new snapshot context. Called by GT.M (or utilities like update process, MUPIP LOAD which uses
 * GT.M runtime. As a side effect sets csa->snapshot_in_prog to TRUE if the context creation went fine.
 */
# define SS_INIT_IF_NEEDED(CSA, CNL)											\
{															\
	int			ss_shmcycle;										\
	boolean_t		status;											\
	snapshot_context_ptr_t	lcl_ss_ctx;										\
															\
	lcl_ss_ctx = SS_CTX_CAST(CSA->ss_ctx);										\
	assert(NULL != lcl_ss_ctx);											\
	CNL->fastinteg_in_prog ? SET_FAST_INTEG(lcl_ss_ctx) : SET_NORM_INTEG(lcl_ss_ctx);				\
	ss_shmcycle = CNL->ss_shmcycle;											\
	SET_SNAPSHOTS_IN_PROG(CSA);											\
	assert(lcl_ss_ctx->ss_shmcycle <= ss_shmcycle);									\
	if (lcl_ss_ctx->ss_shmcycle != ss_shmcycle)									\
	{	/* Process' view of snapshot is stale. Create/Update snapshot context */				\
		status = ss_create_context(lcl_ss_ctx, ss_shmcycle);							\
		if (!status)												\
		{	/* snapshot context creation failed. Reset private copy of snapshot_in_prog so that we don't	\
			 * read the before images in t_end or op_tcommit */						\
			CLEAR_SNAPSHOTS_IN_PROG(CSA);									\
		}													\
		assert(!status || (SNAPSHOT_INIT_DONE == lcl_ss_ctx->cur_state));					\
		assert(status || (SHADOW_FIL_OPEN_FAIL == lcl_ss_ctx->cur_state)					\
			|| (SNAPSHOT_SHM_ATTACH_FAIL  == lcl_ss_ctx->cur_state)						\
			|| (SNAPSHOT_NOT_INITED == lcl_ss_ctx->cur_state));						\
	} else if ((SHADOW_FIL_OPEN_FAIL == lcl_ss_ctx->cur_state) 							\
			|| (SNAPSHOT_SHM_ATTACH_FAIL == lcl_ss_ctx->cur_state))						\
	{	/* Previous attempt at snapshot context creation failed (say, snapshot file open failed) and the error	\
		 * has been reported in the shared memory. However, the snapshot is not yet complete. So, set 		\
		 * snapshot_in_prog to FALSE since the ongoing snapshot is not valid (as indicated by us in the prior	\
		 * transaction/retry inside crit) 									\
		 * Note that we will be doing this 'if' check unconditionally until MUPIP INTEG detects the error in 	\
		 * shared memory which can be avoided by making GT.M itself set CNL->snapshot_in_prog to FALSE when it	\
		 * detects inside crit that snapshot initialization failed for this process and hence the ongoing	\
		 * snapshot is no longer valid. This way we don't wait for MUPIP INTEG to detect and terminate the 	\
		 * snapshots												\
		 */													\
		CLEAR_SNAPSHOTS_IN_PROG(CSA);										\
	}														\
}

#ifdef DEBUG
# define DBG_ENSURE_SNAPSHOT_GOOD_TO_GO(LCL_SS_CTX, CNL)				\
{											\
	shm_snapshot_ptr_t		ss_shm_ptr;					\
											\
	assert(SNAPSHOTS_IN_PROG(CNL));							\
	assert(NULL != LCL_SS_CTX);							\
	ss_shm_ptr = LCL_SS_CTX->ss_shm_ptr;						\
	assert(NULL != ss_shm_ptr);							\
	assert(SNAPSHOT_INIT_DONE == LCL_SS_CTX->cur_state);				\
	assert(0 == LCL_SS_CTX->failure_errno);						\
	assert((-1 != CNL->ss_shmid) && 						\
		(LCL_SS_CTX->attach_shmid == CNL->ss_shmid));				\
	assert(NULL != LCL_SS_CTX->start_shmaddr);					\
	assert(0 == STRCMP(LCL_SS_CTX->shadow_file, ss_shm_ptr->ss_info.shadow_file));	\
	assert(-1 != LCL_SS_CTX->shdw_fd);						\
}
#else
# define DBG_ENSURE_SNAPSHOT_GOOD_TO_GO(LCL_SS_CTX, CNL)
#endif
/* Destroy an existing snapshot. Called by GT.M (or utilities like update process, MUPIP LOAD which uses
 * GT.M runtime. Assumes that csa->snapshot_in_prog is TRUE and as a side effect sets csa->snapshot_in_prog
 * to FALSE if the context is destroyed
 */
# define SS_RELEASE_IF_NEEDED(CSA, CNL)							\
{											\
	int			ss_shmcycle;						\
	snapshot_context_ptr_t	lcl_ss_ctx;						\
											\
	lcl_ss_ctx = SS_CTX_CAST(CSA->ss_ctx);						\
	assert(SNAPSHOTS_IN_PROG(CSA) && (NULL != lcl_ss_ctx));				\
	ss_shmcycle = CNL->ss_shmcycle;							\
	if (!SNAPSHOTS_IN_PROG(cnl) || (lcl_ss_ctx->ss_shmcycle != ss_shmcycle))	\
	{										\
		ss_destroy_context(lcl_ss_ctx);						\
		CLEAR_SNAPSHOTS_IN_PROG(CSA);						\
	}										\
}

/* No need to write before-image in case the block is FREE. In case the database had never been fully upgraded from V4 to V5 format
 * (after the MUPIP UPGRADE), all RECYCLED blocks can basically be considered FREE (i.e. no need to write before-images since
 * backward journal recovery will never be expected to take the database to a point BEFORE the mupip upgrade).
 * Logic to check if before image of a given block has to be read or not are slightly complicated if snapshots are present
 * For snapshots, we might want to read the before images of FREE blocks. Also, if the block that we are reading
 * is already before imaged by some other GT.M process then we do not needed to read the before image of such a block. But, such
 * a condition is applicable ONLY if snapshots alone are in progress as we might want the same block for BACKUP if it is in
 * progress.
 * Note: The below condition, to before image FREE blocks, is needed only if INTEG is the snapshot initiator. When we add
 * bitmasks or some alternate mechanism to optionalize the before image'ing of FREE blocks, this condition must be tweaked
 * accordingly. For now, INTEG is the only snapshot initiator.
 */
# define BEFORE_IMAGE_NEEDED(read_before_image, CS, csa, csd, blk_no, retval)				 		\
{													 		\
	retval = (read_before_image && csd->db_got_to_v5_once);								\
	retval = retval && (!WAS_FREE(CS->blk_prior_state) || SNAPSHOTS_IN_PROG(csa));				\
	retval = retval && (!ONLY_SS_BEFORE_IMAGES(csa) || !ss_chk_shdw_bitmap(csa, SS_CTX_CAST(csa->ss_ctx), blk_no));\
}

# define CHK_AND_UPDATE_SNAPSHOT_STATE_IF_NEEDED(CSA, CNL, SS_NEED_TO_RESTART)							\
{																\
	GBLREF	uint4		process_id;											\
																\
	uint4			lcl_failure_errno;										\
	ss_proc_status		cur_state;											\
	shm_snapshot_ptr_t	ss_shm_ptr;											\
	snapshot_context_ptr_t	lcl_ss_ctx;											\
	boolean_t		csa_snapshot_in_prog, cnl_snapshot_in_prog;							\
																\
	assert(CSA->now_crit);													\
	csa_snapshot_in_prog = SNAPSHOTS_IN_PROG(CSA);										\
	cnl_snapshot_in_prog = SNAPSHOTS_IN_PROG(CNL);										\
	if (csa_snapshot_in_prog || cnl_snapshot_in_prog)									\
	{															\
		lcl_ss_ctx = SS_CTX_CAST(CSA->ss_ctx);										\
		ss_shm_ptr = (shm_snapshot_ptr_t)(SS_GETSTARTPTR(CSA));								\
		assert(lcl_ss_ctx->ss_shmcycle <= CNL->ss_shmcycle);								\
		if (!cnl_snapshot_in_prog || ss_shm_ptr->failure_errno)								\
		{	/* No on going snapshots or on going snapshot is invalid. Even if we encountered error during snapshot	\
			 * context creation outside crit, we ignore it as the snapshot is no more active/valid. 		\
			 */													\
			CLEAR_SNAPSHOTS_IN_PROG(CSA);										\
		} else if (lcl_ss_ctx->ss_shmcycle == CNL->ss_shmcycle)								\
		{	/* Neither new snapshots started nor existing ones completed. However, it's possible that we might have \
			 * encountered error during snapshot context creation outside crit. If the values noted outside crit 	\
			 * matches with the global values, then the error is genuine. If not, then we might have done operations\
			 * (shm attach and file open) when things in the shared memory were in flux in which case we need to	\
			 * restart												\
			 */													\
			lcl_failure_errno = lcl_ss_ctx->failure_errno;								\
			assert(!ss_shm_ptr->failure_errno);									\
			SS_NEED_TO_RESTART = FALSE;										\
			cur_state = lcl_ss_ctx->cur_state;									\
			switch(cur_state)											\
			{													\
				case SNAPSHOT_INIT_DONE:									\
					/* Most common case. Ensure the local values of snapshot context matches with the 	\
					 * values stored in shared memory */							\
					assert(csa_snapshot_in_prog);								\
					DBG_ENSURE_SNAPSHOT_GOOD_TO_GO(lcl_ss_ctx, CNL);					\
					break;											\
				case SNAPSHOT_SHM_ATTACH_FAIL:									\
					assert(0 != lcl_failure_errno);								\
					assert(FALSE == SNAPSHOTS_IN_PROG(CSA));						\
					if (lcl_ss_ctx->nl_shmid == CNL->ss_shmid)						\
					{	/* Error encountered outside crit is genuine. Indicate MUPIP INTEG that the 	\
						 * snapshot is no more valid 							\
						 */										\
						send_msg_csa(CSA_ARG(CSA) VARLSTCNT(4) ERR_SSATTACHSHM, 1,			\
								lcl_ss_ctx->nl_shmid, lcl_failure_errno);			\
						ss_shm_ptr->failure_errno = lcl_failure_errno;					\
						ss_shm_ptr->failed_pid = process_id;						\
					} else /* snapshot context creation done while things were in flux */			\
						SS_NEED_TO_RESTART = TRUE;							\
					break;											\
				case SHADOW_FIL_OPEN_FAIL:									\
					assert(0 != lcl_failure_errno);								\
					assert(FALSE == SNAPSHOTS_IN_PROG(CSA));						\
					if (0 == STRCMP(lcl_ss_ctx->shadow_file, ss_shm_ptr->ss_info.shadow_file))		\
					{	/* Error encountered outside crit is genuine. Indicate MUPIP INTEG that the 	\
						 * snapshot is no more valid							\
						 */										\
						send_msg_csa(CSA_ARG(CSA) VARLSTCNT(7) ERR_SSFILOPERR, 4, LEN_AND_LIT("open"), 	\
							LEN_AND_STR(lcl_ss_ctx->shadow_file), lcl_failure_errno);		\
						ss_shm_ptr->failure_errno = lcl_failure_errno;					\
						ss_shm_ptr->failed_pid = process_id;						\
					} else	/* snapshot context creation done while things were in flux */			\
						SS_NEED_TO_RESTART = TRUE;							\
					break;											\
				default:											\
					assert(FALSE);										\
			}													\
		} else /* A new snapshot has started after we grabbed crit in t_end. Need to restart */				\
			SS_NEED_TO_RESTART = TRUE;										\
	}															\
}

# define WRITE_SNAPSHOT_BLOCK(csa, cr, mm_blk_ptr, blkid, lcl_ss_ctx)			\
{											\
	assert(NULL != lcl_ss_ctx);							\
	/* write this block to the snapshot shadow file only if this was not already	\
	 * before imaged. If error happens while writing to the snapshot file, then	\
	 * ss_write_block will mark the appropriate error in the shared memory		\
	 * which INTEG will later query and report accordingly. So, just continue	\
	 * as if nothing happened. 							\
	 */										\
	if (!ss_chk_shdw_bitmap(csa, lcl_ss_ctx, blkid))				\
		if (!ss_write_block(csa, blkid, cr, mm_blk_ptr, lcl_ss_ctx))		\
			assert(FALSE);							\
}
#else
/* GTM_SNAPSHOT is not defined */
# define SNAPSHOTS_IN_PROG(X)	(FALSE)
# define WRITE_SNAPSHOT_BLOCK(csa, cr, mm_blk_ptr, blkid, lcl_ss_ctx)
# define SS_INIT_IF_NEEDED(CSA, CNL)
# define SS_RELEASE_IF_NEEDED(CSA, CNL)
/* No need to write before-image in case the block is FREE. In case the database had never been fully upgraded from V4 to V5 format
 * (after the MUPIP UPGRADE), all RECYCLED blocks can basically be considered FREE (i.e. no need to write before-images since
 * backward journal recovery will never be expected to take the database to a point BEFORE the mupip upgrade).
 */
# define BEFORE_IMAGE_NEEDED(read_before_image, CS, csa, csd, blk_no, retval)		\
	retval = (read_before_image && csd->db_got_to_v5_once && !WAS_FREE(CS->blk_prior_state));
#endif

/* Determine if the state of 'backup in progress' has changed since we grabbed crit in t_end.c/tp_tend.c */
#define CHK_AND_UPDATE_BKUP_STATE_IF_NEEDED(CNL, CSA, NEW_BKUP_STARTED)				\
{												\
	if (CSA->backup_in_prog != (BACKUP_NOT_IN_PROGRESS != CNL->nbb))			\
	{											\
		if (!CSA->backup_in_prog)							\
			NEW_BKUP_STARTED = TRUE;						\
		CSA->backup_in_prog = !CSA->backup_in_prog;					\
	}											\
}

#define BLK_HDR_EMPTY(bp) ((0 == (bp)->bsiz) && (0 == (bp)->tn))

#ifdef GTM_TRUNCATE
/* Reduction in free blocks after truncating from a to b total blocks: a = old_total (larger), b = new_total */
# define DELTA_FREE_BLOCKS(a, b)	((a - b) - (DIVIDE_ROUND_UP(a, BLKS_PER_LMAP) - DIVIDE_ROUND_UP(b, BLKS_PER_LMAP)))
# define WRITE_EOF_BLOCK(reg, csd, new_total, status)								\
{														\
	off_t		new_eof;										\
	char		buff[DISK_BLOCK_SIZE];									\
	sgmnt_addrs	*csa;											\
	unix_db_info    *udi;											\
														\
	new_eof = ((off_t)(csd->start_vbn - 1) * DISK_BLOCK_SIZE) + ((off_t)new_total * csd->blk_size);		\
	memset(buff, 0, DISK_BLOCK_SIZE);									\
	udi = FILE_INFO(reg);											\
	csa = &udi->s_addrs;											\
	DB_LSEEKWRITE(csa, udi->fn, udi->fd, new_eof, buff, DISK_BLOCK_SIZE, status);				\
}
#endif

typedef enum
{
	REG_FREEZE_SUCCESS,
	REG_ALREADY_FROZEN,
	REG_HAS_KIP
} freeze_status;

#ifdef UNIX
/* This structure holds state captured on entry into gvcst_redo_root_search which it restores on error or exit */
typedef struct redo_root_search_context_struct
{
	unsigned char	t_fail_hist[CDB_MAX_TRIES];
	unsigned int	t_tries;
	unsigned int	prev_t_tries;
	inctn_opcode_t	inctn_opcode;
	trans_num	start_tn;
	uint4		update_trans;
	uint4		t_err;
	boolean_t	hold_onto_crit;
	char		currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	gv_key		*gv_currkey;
#	ifdef DEBUG
	unsigned char	t_fail_hist_dbg[T_FAIL_HIST_DBG_SIZE];
	unsigned int	t_tries_dbg;
#	endif
} redo_root_search_context;
#endif

#define SET_GV_CURRKEY_FROM_REORG_GV_TARGET						\
{	/* see mupip reorg.c for comment */						\
	GBLREF	gv_key		*gv_currkey;						\
	GBLREF	gv_namehead	*reorg_gv_target;	/* for global name */		\
	GBLREF	boolean_t	mu_reorg_process;					\
											\
	mname_entry		*gvent;							\
	int			end;							\
											\
	assert(mu_reorg_process);							\
	gvent = &reorg_gv_target->gvname;						\
	memcpy(gv_currkey->base, gvent->var_name.addr, gvent->var_name.len);		\
	end = gvent->var_name.len + 1;							\
	gv_currkey->end = end;								\
	gv_currkey->base[end - 1] = 0;							\
	gv_currkey->base[end] = 0;							\
}

#define SET_WANT_ROOT_SEARCH(CDB_STATUS, WANT_ROOT_SEARCH)								\
{															\
	GBLREF	uint4			t_tries;									\
	GBLREF	gv_namehead		*gv_target;									\
															\
	if (cdb_sc_normal == cdb_status)										\
		cdb_status = LAST_RESTART_CODE;										\
	/* Below IF check is a special case where t_retry/tp_restart detects mismatched root cycles in 2nd to 3rd retry	\
	 * transition. In this case, the CDB_STATUS can be anything but we still need to redo the root search		\
	 */														\
	if ((CDB_STAGNATE == t_tries) && !gv_target->root)								\
		WANT_ROOT_SEARCH = TRUE;										\
	switch(CDB_STATUS)												\
	{														\
		case cdb_sc_onln_rlbk1:											\
		case cdb_sc_gvtrootmod:											\
		case cdb_sc_gvtrootmod2:										\
			WANT_ROOT_SEARCH = TRUE;									\
			break;												\
		case cdb_sc_onln_rlbk2:											\
			/* Database was taken back to a different logical state and we are an implicit TP		\
			 * transaction. Issue DBROLLEDBACK error that the application programmer can catch and do	\
			 * the necessary stuff.										\
			 */												\
			assert(gtm_trigger_depth == tstart_trigger_depth);						\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DBROLLEDBACK);					\
		default:												\
			break;												\
	}														\
}

#define REDO_ROOT_SEARCH_IF_NEEDED(WANT_ROOT_SEARCH, CDB_STATUS)							\
{															\
	DEBUG_ONLY(GBLREF tp_frame	*tp_pointer;)									\
	GBLREF uint4			t_err;										\
	GBLREF uint4			update_trans;									\
	uint4				save_update_trans, save_t_err;							\
															\
	CDB_STATUS = cdb_sc_normal;											\
	if (WANT_ROOT_SEARCH)												\
	{	/* We are implicit transaction and one of two things has happened:					\
		 * 1. Online rollback updated the database but did NOT take us to a different logical state in which	\
		 *	case we've already done the restart, but the root is now reset to zero.				\
		 * 2. mu_swap_root concurrently moved root blocks. We've restarted and reset root to zero.		\
		 * In either case, do root search to establish the new root.						\
		 */													\
		NON_GTMTRIG_ONLY(assert(FALSE));									\
		assert(tp_pointer->implicit_tstart);									\
		assert(NULL != gv_target);										\
		save_t_err = t_err;											\
		save_update_trans = update_trans;									\
		GVCST_ROOT_SEARCH_DONOT_RESTART(CDB_STATUS);								\
		t_err = save_t_err;											\
		update_trans = save_update_trans;									\
		if (cdb_sc_normal == CDB_STATUS)									\
			WANT_ROOT_SEARCH = FALSE;									\
	}														\
}

#define ASSERT_BEGIN_OF_FRESH_TP_TRANS												\
{																\
	GBLREF	sgm_info	*first_sgm_info;										\
	GBLREF	sgm_info	*sgm_info_ptr;											\
																\
	assert((NULL == first_sgm_info) || ((sgm_info_ptr == first_sgm_info) && (NULL == first_sgm_info->next_sgm_info)));	\
	assert((NULL == first_sgm_info) || (0 == sgm_info_ptr->num_of_blks));							\
}

#define GVCST_ROOT_SEARCH													\
{	/* gvcst_root_search is invoked to establish the root block of a given global (pointed to by gv_target). We always	\
	 * expect the root block of the directory tree to be 1 and so must never come here with gv_target pointing to directory	\
	 * tree. Assert that.													\
	 */															\
	assert((NULL != gv_target) && (DIR_ROOT != gv_target->root));								\
	if (!gv_target->root)													\
		gvcst_root_search(FALSE);											\
}

/* Same as GVCST_ROOT_SEARCH, but tells gvcst_root_search NOT to restart but to return the status code back to the caller */
#define GVCST_ROOT_SEARCH_DONOT_RESTART(STATUS)											\
{																\
	assert((NULL != gv_target) && (DIR_ROOT != gv_target->root));								\
	STATUS = cdb_sc_normal;													\
	if (!gv_target->root)													\
		STATUS = gvcst_root_search(TRUE);										\
}

#define GVCST_ROOT_SEARCH_AND_PREP(est_first_pass)										\
{	/* Before beginning a spanning node try in a gvcst routine, make sure the root is established. If we've restarted	\
	 * issue DBROLLEDBACK appropriately.											\
	 */															\
	GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES]; /* for LAST_RESTART_CODE */					\
	GBLREF	stack_frame		*frame_pointer;										\
																\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	assert(dollar_tlevel);													\
	ASSERT_BEGIN_OF_FRESH_TP_TRANS;												\
	frame_pointer->flags |= SFF_IMPLTSTART_CALLD;										\
	if (est_first_pass && (cdb_sc_onln_rlbk2 == LAST_RESTART_CODE))								\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DBROLLEDBACK);							\
	tp_set_sgm();														\
	GVCST_ROOT_SEARCH;													\
}

#define GV_BIND_NAME_ONLY(ADDR, TARG)	gv_bind_name(ADDR, TARG)

#define GV_BIND_NAME_AND_ROOT_SEARCH(ADDR, TARG)										\
{																\
	enum db_acc_method	acc_meth;											\
	GBLREF gd_region	*gv_cur_region;											\
	GBLREF gv_namehead	*gv_target;											\
																\
	GV_BIND_NAME_ONLY(ADDR, TARG);												\
	acc_meth = gv_cur_region->dyn.addr->acc_meth;										\
	if ((dba_bg == acc_meth) || (dba_mm == acc_meth))									\
		GVCST_ROOT_SEARCH;												\
}

/* When invoking grab_lock or grab_gtmsource_srv_latch, use one of the following parameters.
 * GRAB_LOCK_ONLY			: use when code expects an online rollback, but handles it separately (like updproc.c)
 * GRAB_GTMSOURCE_SRV_LATCH_ONLY	: Same as above (just that this is used for grab_gtmsource_srv_latch instead of grab_lock)
 * ASSERT_NO_ONLINE_ROLLBACK		: use when holding some other lock (like crit) that prevents online rollback
 * HANDLE_CONCUR_ONLINE_ROLLBACK	: use when not blocking online rollback, but handling with this macro (only source server)
 */
#define	GRAB_LOCK_ONLY				0x01
#define	GRAB_GTMSOURCE_SRV_LATCH_ONLY		0x01
#define ASSERT_NO_ONLINE_ROLLBACK		0x02
#define HANDLE_CONCUR_ONLINE_ROLLBACK		0x03

#ifdef UNIX
/* caller should have THREADGBL_ACCESS and gbl access to t_tries and t_fail_hist */
# define LAST_RESTART_CODE			( (0 < t_tries) ? t_fail_hist[TREF(prev_t_tries)] : (enum cdb_sc)cdb_sc_normal )
# define SYNC_ONLN_RLBK_CYCLES													\
{																\
	GBLREF	sgmnt_addrs		*cs_addrs_list;										\
	GBLREF	jnlpool_addrs		jnlpool;										\
	GBLREF	boolean_t		mu_reorg_process;									\
																\
	sgmnt_addrs			*lcl_csa;										\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	if (!TREF(in_gvcst_bmp_mark_free) || mu_reorg_process)									\
	{															\
		for (lcl_csa = cs_addrs_list; NULL != lcl_csa; lcl_csa = lcl_csa->next_csa)					\
		{														\
			lcl_csa->onln_rlbk_cycle = lcl_csa->nl->onln_rlbk_cycle;						\
			lcl_csa->db_onln_rlbkd_cycle = lcl_csa->nl->db_onln_rlbkd_cycle;					\
			lcl_csa->db_trigger_cycle = lcl_csa->hdr->db_trigger_cycle;						\
		}														\
		if (NULL != jnlpool.jnlpool_ctl)										\
		{														\
			lcl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;						\
			lcl_csa->onln_rlbk_cycle = jnlpool.jnlpool_ctl->onln_rlbk_cycle;					\
		}														\
	}															\
}
# define SYNC_ROOT_CYCLES(CSA)													\
{	/* NULL CSA acts as a flag to do all regions */										\
	GBLREF  sgmnt_addrs             *cs_addrs_list;										\
	GBLREF	boolean_t		mu_reorg_process;									\
																\
	sgmnt_addrs                     *lcl_csa;										\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	for (lcl_csa = cs_addrs_list; NULL != lcl_csa; lcl_csa = lcl_csa->next_csa)						\
		if (NULL == (CSA) || (CSA) == lcl_csa)										\
			lcl_csa->root_search_cycle = lcl_csa->nl->root_search_cycle;						\
}
# define MISMATCH_ROOT_CYCLES(CSA, CNL)		((CSA)->root_search_cycle != (CNL)->root_search_cycle)
# define MISMATCH_ONLN_RLBK_CYCLES(CSA, CNL)	((CSA)->onln_rlbk_cycle != (CNL)->onln_rlbk_cycle)
# define ONLN_RLBK_STATUS(CSA, CNL)												\
	(((CSA)->db_onln_rlbkd_cycle != (CNL)->db_onln_rlbkd_cycle) ? cdb_sc_onln_rlbk2 : cdb_sc_onln_rlbk1)
# define ABORT_TRANS_IF_GBL_EXIST_NOMORE(LCL_T_TRIES, TN_ABORTED)								\
{																\
	DEBUG_ONLY(GBLREF unsigned int		t_tries;)									\
	DEBUG_ONLY(GBLREF unsigned char		t_fail_hist[CDB_MAX_TRIES];)							\
	GBLREF	gd_region			*gv_cur_region;									\
	GBLREF	sgmnt_addrs			*cs_addrs;									\
	GBLREF	gv_namehead			*gv_target;									\
																\
	DEBUG_ONLY(enum cdb_sc			failure;)									\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	assert(0 < t_tries);													\
	assert((CDB_STAGNATE == t_tries) || (lcl_t_tries == t_tries - 1));							\
	DEBUG_ONLY(failure = LAST_RESTART_CODE);										\
	assert(NULL != gv_target);												\
	TN_ABORTED = FALSE;													\
	if (!gv_target->root)													\
	{	/* online rollback took us back to a prior logical state where the global that existed when we came into	\
		 * mu_reorg or mu_extr_getblk, no longer exists. Consider this as the end of the tree and return to the		\
		 * caller with the appropriate code. The caller knows to continue with the next global				\
		 */														\
		assert(cdb_sc_onln_rlbk2 == failure || TREF(rlbk_during_redo_root));						\
		/* abort the current transaction */										\
		t_abort(gv_cur_region, cs_addrs);										\
		TN_ABORTED = TRUE;												\
	}															\
}
#else
# define ABORT_TRANS_IF_GBL_EXIST_NOMORE(LCL_T_TRIES, TN_ABORTED)
#endif

#define	SET_GV_ALTKEY_TO_GBLNAME_FROM_GV_CURRKEY				\
{										\
	uchar_ptr_t		dst_ptr, src_ptr;				\
										\
	GBLREF	gv_key		*gv_currkey, *gv_altkey;			\
	GBLREF	int4		gv_keysize;					\
										\
	assert(gv_altkey->top == gv_currkey->top);				\
	assert(gv_altkey->top == gv_keysize);					\
	assert(gv_currkey->end < gv_currkey->top);				\
	dst_ptr = gv_altkey->base;						\
	src_ptr = gv_currkey->base;						\
	for ( ; *src_ptr; )							\
		*dst_ptr++ = *src_ptr++;					\
	*dst_ptr++ = 0;								\
	*dst_ptr = 0;								\
	gv_altkey->end = dst_ptr - gv_altkey->base;				\
	assert(gv_altkey->end < gv_altkey->top);				\
}

typedef struct span_subs_struct {
	unsigned char	b_ctrl;
	unsigned char	b_first;
	unsigned char	b_sec;
} span_subs;

#define ASCII_0			'0'
#define SPAN_BLKID_BASE		255
#define DECIMAL_BASE		10

/* SPAN_SUBS_LEN macro define the length of the special subscript of spanning node in terms of number of characters.
 * The format of special spanning node subscript is '#-X-X', where X ranges from 1 to 255. Note that X never takes the
 * value of '0' because '0' is used as subscript deliminiter.
 */
#define SPAN_SUBS_LEN		3

#define SPAN_PREFIX		"#SPAN"
#define SPAN_PREFIX_LEN		(SIZEOF(SPAN_PREFIX) - 1)
#define ASGN_SPAN_PREFIX(B)	{*(B) = '#'; *(B + 1) = 'S'; *(B + 2) = 'P'; *(B + 3) = 'A'; *(B + 4) = 'N';}
#define SPAN_GVSUBS2INT(A)      (((A)->b_first - 1) * SPAN_BLKID_BASE + (A)->b_sec - 1)
#define SPAN_INT2GVSUBS(A, B)   {(A)->b_ctrl = 0x02; (A)->b_first = B / SPAN_BLKID_BASE + 1 ; (A)->b_sec = B % SPAN_BLKID_BASE + 1;}
#define SPAN_DECLSUBS(A)	(span_subs A)
#define SPAN_INITSUBS(A, B)	SPAN_INT2GVSUBS(A, B)
#define SPAN_INCRSUBS(A)	SPAN_INT2GVSUBS(A, SPAN_GVSUBS2INT(A) + 1)
#define SPAN_INCRBYSUBS(A, B)	SPAN_INT2GVSUBS(A, SPAN_GVSUBS2INT(A) + B)
#define SPAN_DECRSUBS(A, B)	SPAN_INT2GVSUBS(A, SPAN_GVSUBS2INT(A) - 1)
#define SPAN_DECRBYSUBS(A, B)	SPAN_INT2GVSUBS(A, SPAN_GVSUBS2INT(A) - B)

/* The following macro assumes that the length of the special subscript for spanning node is '3'. The assert in the macro verfies
 * that this assumption is true. In future, if the length of the special subscript for spanning node changes, the following macro
 * needs to be fixed accordingly.
 */
#define SPAN_SUBSCOPY_SRC2DST(DST, SRC) 						\
{											\
	assert(SPAN_SUBS_LEN == 3);							\
	*((DST) + 0) = *((SRC) + 0); 							\
	*((DST) + 1) = *((SRC) + 1); 							\
	*((DST) + 2) = *((SRC) + 2);							\
}

/*#include "mv_stent.h"*/
typedef struct
{
	boolean_t		span_status;
	boolean_t		enable_jnl_format;
	boolean_t		enable_trigger_read_and_fire;
	boolean_t		ztval_gvcst_put_redo;
	mval			*val_forjnl;
	int4			blk_reserved_bytes;
#	ifdef GTM_TRIGGER
	unsigned char		*save_msp;
	unsigned char		*save_mv_chain; /* actually mv_stent ptr */
	mval			*ztold_mval;
	mval			*ztval_mval;
#	endif
} span_parms;

#ifdef UNIX
# define DEFINE_NSB_CONDITION_HANDLER(gvcst_xxx_ch)							\
	CONDITION_HANDLER(gvcst_xxx_ch)									\
	{												\
		int rc;											\
													\
		START_CH;										\
		if ((int)ERR_TPRETRY == SIGNAL)								\
		{											\
			rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);					\
			/*DEBUG_ONLY(printf("gvcst_xxx_ch: Unwinding due to TP Restart\n");)*/		\
			UNWIND(NULL, NULL);								\
		}											\
		NEXTCH;											\
	}
/* Check if the value of a primary node is a dummy value: $c(0). If so, it might be a spanning node */
# define IS_SN_DUMMY(len, addr) ((1 == (len)) && ('\0' == *(unsigned char *)(addr)))
# define RTS_ERROR_IF_SN_DISALLOWED											\
{															\
	GBLREF	boolean_t 		span_nodes_disallowed;								\
															\
	error_def(ERR_TEXT); /* BYPASSOK */										\
	error_def(ERR_UNIMPLOP); /* BYPASSOK */										\
															\
	if (span_nodes_disallowed)											\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UNIMPLOP, 0,	ERR_TEXT, 2,				\
					LEN_AND_LIT("GT.CM Server does not support spanning nodes"));			\
}
# define IF_SN_DISALLOWED_AND_NO_SPAN_IN_DB(STATEMENT)									\
{	/* We've encountered a spanning node dummy value. Check if spanning nodes are disallowed (e.g., GT.CM).		\
	 * If csd->span_node_absent is TRUE we know the value really is $c(0) and we return that. Otherwise,		\
	 * this is potentially a spanning node. We cannot do an op_tstart to check so we issue an error instead.	\
	 */														\
	GBLREF	sgmnt_data_ptr_t	cs_data;									\
	GBLREF	boolean_t 		span_nodes_disallowed;								\
															\
	if (span_nodes_disallowed)											\
	{														\
		if (!cs_data->span_node_absent)										\
		{													\
			RTS_ERROR_IF_SN_DISALLOWED;									\
		} else													\
		{													\
			STATEMENT;											\
		}													\
	}														\
}
#else
# define DEFINE_NSB_CONDITION_HANDLER(gvcst_xxx_ch)
# define IF_NSB_DUMMY_RETURN
# define RTS_ERROR_IF_SN_DISALLOWED_AND_SPAN_IN_DB
# define RETURN_NO_VAL_IF_SN_DISALLOWED
# define RETURN_IF_SN_DISALLOWED(value)
#endif

#define MAX_NSBCTRL_SZ		30	/* Upper bound on size of ctrl value. 2*10 digs + 1 comma + 1 null + overhead */

/* Control node value is 6 bytes, 2 for unsigned short numsubs, 4 for uint4 gblsize. Each is little-endian. */
#ifdef	BIGENDIAN
# define PUT_NSBCTRL(BYTES, NUMSUBS, GBLSIZE)					\
{										\
	unsigned short	swap_numsubs;						\
	uint4		swap_gblsize;						\
										\
	swap_numsubs = GTM_BYTESWAP_16(NUMSUBS);				\
	swap_gblsize = GTM_BYTESWAP_32(GBLSIZE);				\
	PUT_USHORT((unsigned char *)BYTES, swap_numsubs);			\
	PUT_ULONG((unsigned char *)BYTES + 2, swap_gblsize);			\
}
# define GET_NSBCTRL(BYTES, NUMSUBS, GBLSIZE)					\
{										\
	unsigned short	swap_numsubs;						\
	uint4		swap_gblsize;						\
										\
	GET_USHORT(swap_numsubs, (unsigned char *)BYTES);			\
	GET_ULONG(swap_gblsize, (unsigned char *)BYTES + 2);			\
	NUMSUBS = GTM_BYTESWAP_16(swap_numsubs);				\
	GBLSIZE = GTM_BYTESWAP_32(swap_gblsize);				\
}
#else
# define PUT_NSBCTRL(BYTES, NUMSUBS, GBLSIZE)					\
{										\
	PUT_USHORT((unsigned char *)BYTES, NUMSUBS);				\
	PUT_ULONG((unsigned char *)BYTES + 2, GBLSIZE);				\
}
# define GET_NSBCTRL(BYTES, NUMSUBS, GBLSIZE)					\
{										\
	GET_USHORT(NUMSUBS, (unsigned char *)BYTES);				\
	GET_ULONG(GBLSIZE, (unsigned char *)BYTES + 2);				\
}
#endif

#define	CHECK_HIDDEN_SUBSCRIPT_AND_RETURN(found, gv_altkey, is_hidden)		\
{										\
	if (found)								\
	{									\
		CHECK_HIDDEN_SUBSCRIPT(gv_altkey, is_hidden);			\
		if (!is_hidden)							\
			return found;						\
	} else									\
		return found;							\
}
#define CHECK_HIDDEN_SUBSCRIPT_AND_BREAK(found, gv_altkey, is_hidden)		\
{										\
	if (found)								\
	{									\
		CHECK_HIDDEN_SUBSCRIPT(gv_altkey, is_hidden);			\
		if (!is_hidden)							\
			break;							\
	} else									\
		break;								\
}
#define CHECK_HIDDEN_SUBSCRIPT(KEY, IS_HIDDEN)										\
{															\
	sm_uc_ptr_t	keyloc;												\
															\
	keyloc = (KEY)->base + (KEY)->end - 5;										\
	if ((KEY)->end >= 5 && 0 == *keyloc && 2 == *(keyloc+1))							\
		IS_HIDDEN = TRUE;											\
	else														\
		IS_HIDDEN = FALSE;											\
}
#define SAVE_GV_CURRKEY									\
{											\
	assert(NULL != gv_currkey);							\
	assert((SIZEOF(gv_key) + gv_currkey->end) <= SIZEOF(save_currkey));		\
	save_gv_currkey = (gv_key *)&save_currkey[0];					\
	memcpy(save_gv_currkey, gv_currkey, SIZEOF(gv_key) + gv_currkey->end);		\
}
#define RESTORE_GV_CURRKEY								\
{											\
	assert(gv_currkey->top == save_gv_currkey->top);				\
	memcpy(gv_currkey, save_gv_currkey, SIZEOF(gv_key) + save_gv_currkey->end);	\
}
#define SAVE_GV_CURRKEY_LAST_SUBSCRIPT(gv_currkey, prev, oldend)			\
{											\
	prev = gv_currkey->prev;							\
	oldend = gv_currkey->end;							\
	assert('\0' == gv_currkey->base[oldend]);					\
	if (prev <= oldend)								\
		memcpy(save_currkey, &gv_currkey->base[prev], oldend - prev + 1);	\
}
#define RESTORE_GV_CURRKEY_LAST_SUBSCRIPT(gv_currkey, prev, oldend)			\
{											\
	gv_currkey->prev = prev;							\
	gv_currkey->end = oldend;							\
	if (prev <= oldend)								\
		memcpy(&gv_currkey->base[prev], save_currkey, oldend - prev + 1);	\
	assert('\0' == gv_currkey->base[oldend]);					\
}

#define CAN_APPEND_HIDDEN_SUBS(KEY)	(((KEY)->end + 5 <= MAX_KEY_SZ) && ((KEY)->end + 5 <= (KEY)->top))
#define APPEND_HIDDEN_SUB(KEY)										\
{													\
	int	end;											\
													\
	assert(CAN_APPEND_HIDDEN_SUBS(KEY));								\
	end = gv_currkey->end;										\
	gv_currkey->end += 4;										\
	(KEY)->base[end++] = 2;										\
	(KEY)->base[end++] = 1;										\
	(KEY)->base[end++] = 1;										\
	(KEY)->base[end++] = 0;										\
	(KEY)->base[end] = 0;										\
}
#define NEXT_HIDDEN_SUB(KEY, I)										\
{													\
	int	end;											\
													\
	end = gv_currkey->end - 4;									\
	(KEY)->base[end++] = 2;										\
	(KEY)->base[end++] = 1 + ((I + 1) / 0xFF);							\
	(KEY)->base[end++] = 1 + ((I + 1) % 0xFF);							\
	(KEY)->base[end++] = 0;										\
	(KEY)->base[end] = 0;										\
}
#define RESTORE_CURRKEY(KEY, OLDEND)									\
{													\
	(KEY)->end = OLDEND;										\
	(KEY)->base[OLDEND - 1] = 0; 									\
	(KEY)->base[OLDEND] = 0;									\
}
#define COMPUTE_CHUNK_SIZE(KEY, BLKSZ, RESERVED)							\
	(BLKSZ - RESERVED - ((KEY)->end + 1) - SIZEOF(blk_hdr) - SIZEOF(rec_hdr))

void		assert_jrec_member_offsets(void);
bt_rec_ptr_t	bt_put(gd_region *r, int4 block);
void		bt_que_refresh(gd_region *greg);
void		bt_init(sgmnt_addrs *cs);
void		bt_malloc(sgmnt_addrs *csa);
void		bt_refresh(sgmnt_addrs *csa, boolean_t init);
void		db_common_init(gd_region *reg, sgmnt_addrs *csa, sgmnt_data_ptr_t csd);
void		grab_crit(gd_region *reg);
boolean_t	grab_crit_immediate(gd_region *reg);
boolean_t	grab_lock(gd_region *reg, boolean_t is_blocking_wait, uint4 onln_rlbk_action);
void		gv_init_reg(gd_region *reg);
void		gvcst_init(gd_region *greg);
enum cdb_sc	gvincr_compute_post_incr(srch_blk_status *bh);
enum cdb_sc	gvincr_recompute_upd_array(srch_blk_status *bh, struct cw_set_element_struct *cse, cache_rec_ptr_t cr);
boolean_t	mupfndfil(gd_region *reg, mstr *mstr_addr);
boolean_t	region_init(bool cm_regions);
freeze_status	region_freeze(gd_region *region, boolean_t freeze, boolean_t override, boolean_t wait_for_kip);
void		rel_crit(gd_region *reg);
void		rel_lock(gd_region *reg);
boolean_t	wcs_verify(gd_region *reg, boolean_t expect_damage, boolean_t caller_is_wcs_recover);
bool		wcs_wtfini(gd_region *reg);

#ifdef VMS
int4 wcs_wtstart(gd_region *region);
#elif defined(UNIX)
int4 wcs_wtstart(gd_region *region, int4 writes);
#else
#error Undefined Platform
#endif

void bmm_init(void);
int4 bmm_find_free(uint4 hint, uchar_ptr_t base_addr, uint4 total_bits);

bool reg_cmcheck(gd_region *reg);

void gv_bind_name(gd_addr *addr, mstr *targ);

void db_csh_ini(sgmnt_addrs *cs);
void db_csh_ref(sgmnt_addrs *cs_addrs, boolean_t init);
cache_rec_ptr_t db_csh_get(block_id block);
cache_rec_ptr_t db_csh_getn(block_id block);

enum cdb_sc tp_hist(srch_hist *hist1);

sm_uc_ptr_t get_lmap(block_id blk, unsigned char *bits, sm_int_ptr_t cycle, cache_rec_ptr_ptr_t cr);

bool ccp_userwait(struct gd_region_struct *reg, uint4 state, int4 *timadr, unsigned short cycle);
void ccp_closejnl_ast(struct gd_region_struct *reg);
bt_rec *ccp_bt_get(sgmnt_addrs *cs_addrs, int4 block);
unsigned char *mval2subsc(mval *in_val, gv_key *out_key);

int4	dsk_read(block_id blk, sm_uc_ptr_t buff, enum db_ver *ondisk_blkver, boolean_t blk_free);

gtm_uint64_t gds_file_size(file_control *fc);

uint4	jnl_flush(gd_region *reg);
void	jnl_fsync(gd_region *reg, uint4 fsync_addr);
void	jnl_mm_timer(sgmnt_addrs *csa, gd_region *reg);
void	jnl_oper_user_ast(gd_region *reg);
void	jnl_wait(gd_region *reg);
void	view_jnlfile(mval *dst, gd_region *reg);
void	jnl_put_jrt_pfin(sgmnt_addrs *csa);
void	jnl_put_jrt_pini(sgmnt_addrs *csa);
void	jnl_write_epoch_rec(sgmnt_addrs *csa);
void	jnl_write_inctn_rec(sgmnt_addrs *csa);
void	fileheader_sync(gd_region *reg);

gd_addr	*create_dummy_gbldir(void);

/* These prototypes should ideally be included in gvstats_rec.h but they require "sgmnt_addrs" type
 * to be defined which is done in this header file, hence the prototyping is done here instead.
 */
void	gvstats_rec_csd2cnl(sgmnt_addrs *csa);
void	gvstats_rec_cnl2csd(sgmnt_addrs *csa);
void	gvstats_rec_upgrade(sgmnt_addrs *csa);

#include "gdsfheadsp.h"

/* End of gdsfhead.h */

#endif
