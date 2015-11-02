/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#ifdef VMS
#include "iosb_disk.h"
#endif

#define CACHE_STATE_OFF sizeof(que_ent)

/* all this record's fields should exactly be the first members of the cache_rec in the same order */
typedef struct mmblk_rec_struct
{
	struct
	{
		int4	fl;
		int4	bl;
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
	uint4		refer;
	enum db_ver	ondsk_blkver;	/* Actual block version from block header as it exists on disk
					   (prior to any dynamic conversion that may have occurred when read in).
					*/
	trans_num	dirty;
} mmblk_rec;

/* all the fields of this record should exactly be the first members of the cache_state_rec in the same order */
typedef struct mmblk_state_rec_struct
{
	struct
	{
		int4	fl;
		int4	bl;
	}
			state_que;	/* WARNING -- from this point onwards this should be identical to a mmblk_rec */
	union
	{
		short	semaphore;
		volatile int4	latch;		/* int required for atomic swap on Unix */
			/* volatile required as this value is referenced outside of the lock in db_csh_getn() */
	} interlock;
	block_id	blk;
	uint4		refer;
	enum db_ver	ondsk_blkver;	/* Actual block version from block header as it exists on disk
					   (prior to any dynamic conversion that may have occurred when read in).
					*/
	trans_num	dirty;
} mmblk_state_rec;

typedef struct
{
	mmblk_que_head	mmblkq_wip,	/* write-in-progress queue -- unused in Unix */
			mmblkq_active;	/* active queue */
	mmblk_rec	mmblk_array[1];		/* the first mmblk record */
} mmblk_que_heads;


		/* need to keep quadword aligned */

/* Cache record  -- NOTE: the head portion of this should exactly match with mmblk_rec */
typedef struct cache_rec_struct
{
	struct
	{
		int4 	fl;
		int4	bl;
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
	int4		filler;		/* Alignment */
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
	boolean_t	in_tend;	/* TRUE from bg_update indicates secshr_db_clnup should finish update */
	boolean_t	in_cw_set;	/* TRUE from t_end, tp_tend or bg_update protects block from db_csh_getn; returned to
  					 * FALSE by t_end, tp_tend or t_commit_cleanup */
	boolean_t	data_invalid;	/* TRUE from bg_update indicates t_commit_cleanup and wcs_recover should invalidate */
	boolean_t	stopped;	/* TRUE indicates to wcs_recover that secshr_db_clnup built the block */
	boolean_t	wip_stopped;	/* TRUE indicates to wcs_recover, wcs_wtfini, wcs_get_blk and gds_rundown
  					 * that secshr_db_clnup cancelled the qio*/
} cache_rec;

/* A note about cache line separation of the latches contained in these blocks. Because this block is duplicated
   many (ptentially tens+ of) thousands of times in a running system, we have decided against providing cacheline
   padding so as to force each cache record into a separate cacheline (due to it containing a latch and/or atomic
   counter field) to prevent processes from causing interference with each other. We decided that the probability
   of two processes working on adjacent cache records simultaneously was low enough that the interference was
   minimal whereas increasing the cache record size to prevent that interference could cause storage problems
   on some platforms where processes are already running near the edge.
*/

/* cache_state record  -- NOTE: the first few fields of this should be identical to that of mmblk_state_rec */
typedef struct
{
	struct
	{
		int4 	fl;
		int4	bl;
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
	int4		filler;		/* Alignment */
#endif
	off_jnl_t	jnl_addr;	/* offset from bg_update to prevent wcs_wtstart from writing a block ahead of the journal */
	global_latch_t	rip_latch;	/* for read_in_progress - note contains extra 16 bytes for HPPA */

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
	boolean_t	in_tend;	/* TRUE from bg_update indicates secshr_db_clnup should finish update */
	boolean_t	in_cw_set;	/* TRUE from t_end, tp_tend or bg_update protects block from db_csh_getn; returned to
  					 * FALSE by t_end, tp_tend or t_commit_cleanup */
	boolean_t	data_invalid;	/* TRUE from bg_update indicates t_commit_cleanup and wcs_recover should invalidate */
	boolean_t	stopped;	/* TRUE indicates to wcs_recover that secshr_db_clnup built the block */
	boolean_t	wip_stopped;	/* TRUE indicates to wcs_recover, wcs_wtfini, wcs_get_blk and gds_rundown
  					 * that secshr_db_clnup cancelled the qio*/
} cache_state_rec;

#define		CR_BLKEMPTY             -1
#define		FROZEN_BY_ROOT          (uint4)(0xFFFFFFFF)
#define		BACKUP_NOT_IN_PROGRESS  0x7FFFFFFF

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

typedef mmblk_que_head	*mmblk_que_head_ptr_t;
typedef mmblk_rec	*mmblk_rec_ptr_t;
typedef mmblk_rec	**mmblk_rec_ptr_ptr_t;
typedef mmblk_state_rec *mmblk_state_rec_ptr_t;
typedef mmblk_que_heads	*mmblk_que_heads_ptr_t;

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

/* the file header has relative pointers to its data structures so each process will malloc
 * one of these and fill it in with absolute pointers upon file initialization.
 */
#define GDS_REL2ABS(x)	(((sm_uc_ptr_t)cs_addrs->lock_addrs[0] + (sm_off_t)(x)))
#define GDS_ABS2REL(x) (sm_off_t)(((sm_uc_ptr_t)(x) - (sm_uc_ptr_t)cs_addrs->lock_addrs[0]))
#define GDS_ANY_REL2ABS(w,x) (((sm_uc_ptr_t)(w->lock_addrs[0]) + (sm_off_t)(x)))
#define GDS_ANY_ABS2REL(w,x) (sm_off_t)(((sm_uc_ptr_t)(x) - (sm_uc_ptr_t)w->lock_addrs[0]))

#define	ASSERT_IS_WITHIN_SHM_BOUNDS(ptr, csa)											\
	assert((NULL == (ptr)) || (((ptr) >= csa->db_addrs[0]) && ((0 == csa->db_addrs[1]) || ((ptr) < csa->db_addrs[1]))))

#ifdef DEBUG
#define	DBG_ENSURE_OLD_BLOCK_IS_VALID(cse, is_mm, csa, csd)							\
{														\
	cache_rec_ptr_t		cache_start;									\
	long			bufindx;									\
	sm_uc_ptr_t		bufstart;									\
	GBLREF	boolean_t	dse_running, write_after_image;							\
														\
	assert(gds_t_write != cse->mode && gds_t_writemap != cse->mode || cse->old_block); 			\
									/* don't miss writing a PBLK */		\
	if (NULL != cse->old_block)										\
	{													\
		if (!is_mm)											\
		{												\
			cache_start = &csa->acc_meth.bg.cache_state->cache_array[0];				\
			cache_start += csd->bt_buckets;								\
			bufstart = (sm_uc_ptr_t)GDS_REL2ABS(cache_start->buffaddr);				\
			bufindx = (cse->old_block - bufstart) / csd->blk_size;					\
			assert(bufindx < csd->n_bts);								\
			assert(cse->blk == cache_start[bufindx].blk);						\
			assert(dse_running || write_after_image || cache_start[bufindx].in_cw_set);		\
		} else												\
		{												\
			assert(cse->old_block == csa->db_addrs[0] + cse->blk * csd->blk_size			\
						+ (csd->start_vbn - 1) * DISK_BLOCK_SIZE);			\
		}												\
	}													\
}
#else
#define DBG_ENSURE_OLD_BLOCK_IS_VALID(cse, is_mm, csa, csd)
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

#define MM_ADDR(SGD)		((sm_uc_ptr_t)(((sgmnt_data_ptr_t)SGD) + 1))
#define MASTER_MAP_BLOCKS_DFLT	64				/* 64 gives 128M possible blocks  */
#define MASTER_MAP_BLOCKS_V4	32				/* 32 gives 64M possible blocks  */
#define MASTER_MAP_BLOCKS_MAX	128				/* 128 gives 256M possible blocks  */
#define MASTER_MAP_SIZE_V4	(MASTER_MAP_BLOCKS_V4 * DISK_BLOCK_SIZE)	/* MUST be a multiple of DISK_BLOCK_SIZE */
#define MASTER_MAP_SIZE_MAX	(MASTER_MAP_BLOCKS_MAX * DISK_BLOCK_SIZE)	/* MUST be a multiple of DISK_BLOCK_SIZE */
#define MASTER_MAP_SIZE_DFLT	(MASTER_MAP_BLOCKS_DFLT * DISK_BLOCK_SIZE)	/* MUST be a multiple of DISK_BLOCK_SIZE */
#define MASTER_MAP_SIZE(SGD)	(((sgmnt_data_ptr_t)SGD)->master_map_len)
#define SGMNT_HDR_LEN	sizeof(sgmnt_data)
#define SIZEOF_FILE_HDR(SGD)	(SGMNT_HDR_LEN + MASTER_MAP_SIZE(SGD))
#define SIZEOF_FILE_HDR_DFLT	(SGMNT_HDR_LEN + MASTER_MAP_SIZE_DFLT)
#define SIZEOF_FILE_HDR_MIN	(SGMNT_HDR_LEN + MASTER_MAP_SIZE_V4)
#define SIZEOF_FILE_HDR_MAX	(SGMNT_HDR_LEN + MASTER_MAP_SIZE_MAX)
#define MM_BLOCK		(SGMNT_HDR_LEN / DISK_BLOCK_SIZE + 1)	/* gt.m numbers blocks from 1 */
#define TH_BLOCK	 	1

#define JNL_NAME_SIZE	        256        /* possibly expanded when opened */
#define JNL_NAME_EXP_SIZE	1024       /* MAXPATHLEN, before jnl_buffer in shared memory */

#define BLKS_PER_LMAP		512
#define MAXTOTALBLKS_V4		(MASTER_MAP_SIZE_V4 * 8 * BLKS_PER_LMAP)
#define MAXTOTALBLKS_V5		(MASTER_MAP_SIZE_MAX * 8 * BLKS_PER_LMAP)
#define MAXTOTALBLKS_MAX	(MASTER_MAP_SIZE_MAX * 8 * BLKS_PER_LMAP)
#define MAXTOTALBLKS(SGD)	(MASTER_MAP_SIZE(SGD) * 8 * BLKS_PER_LMAP)
#define	IS_BITMAP_BLK(blk)	(ROUND_DOWN2(blk, BLKS_PER_LMAP) == blk)	/* TRUE if blk is a bitmap */

#define START_VBN_V5		129 /* 8K fileheader (= 16 blocks) + 32K mastermap (= 64 blocks) + 24K padding (= 48 blocks) + 1 */
#define	START_VBN_V4		 49 /* 8K fileheader (= 16 blocks) + 16K mastermap (= 32 blocks) + 1 */
#define START_VBN_CURRENT	START_VBN_V5

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
#define	DCLAST_WCS_WTSTART(reg, num_bufs, RET)			\
{								\
	if (SS$_NORMAL != sys$dclast(wcs_wtstart, reg, 0))	\
	{							\
		unsigned int	status;				\
								\
		status = DISABLE_AST;				\
		wcs_wtstart(reg);				\
		if (SS$_WASSET == status)			\
			ENABLE_AST;				\
	}							\
}
#elif defined(UNIX)
#define	DCLAST_WCS_WTSTART(reg, num_bufs, RET)	RET = wcs_wtstart(reg, num_bufs);
#else
#error UNSUPPORTED PLATFORM
#endif

#define ENSURE_JNL_OPEN(csa, reg)                                                 					\
{                                                                               					\
        assert(cs_addrs == csa);                                                        				\
        assert(gv_cur_region == reg);                                                   				\
        assert(FALSE == reg->read_only);                                                				\
        if (JNL_ENABLED(csa->hdr) && NULL != csa->jnl && NOJNL == csa->jnl->channel)  					\
        {                                                                       					\
                bool 	was_crit;                                               					\
		uint4	jnl_status;											\
                was_crit = csa->now_crit;                                         					\
                if (!was_crit)                                                  					\
                        grab_crit(reg);                                         					\
                jnl_status = jnl_ensure_open();                                 					\
                if (!was_crit)                                                  					\
                        rel_crit(reg);                                          					\
		if (0 != jnl_status)											\
                        rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csa->hdr), DB_LEN_STR(gv_cur_region));	\
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
 */
#define INCR_BLKS_TO_UPGRD(csa, csd, delta)										\
{															\
	int4	new_blks_to_upgrd;											\
															\
	assert((csd)->createinprogress || (csa)->now_crit);								\
	assert((csa)->hdr == (csd));											\
	assert(0 != (delta));												\
	assert(0 <= (csd)->blks_to_upgrd);										\
	new_blks_to_upgrd = (delta) + (csd)->blks_to_upgrd;								\
	assert(0 <= new_blks_to_upgrd);											\
	(csd)->blks_to_upgrd = new_blks_to_upgrd;									\
	if (0 >= new_blks_to_upgrd)											\
	{														\
		if (0 == new_blks_to_upgrd)										\
			(csd)->tn_upgrd_blks_0 = (csd)->trans_hist.curr_tn;						\
		else													\
		{	/* blks_to_upgrd counter in the fileheader should never hold a negative value.			\
			 * note down the negative value in a separate field for debugging and set the counter to 0.	\
			 */												\
			(csd)->blks_to_upgrd = 0;									\
			(csd)->blks_to_upgrd_subzero_error -= (new_blks_to_upgrd);					\
		}													\
	} else														\
		(csd)->fully_upgraded = FALSE;										\
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

#define	ARRAYSIZE(arr)	sizeof(arr)/sizeof(arr[0])

#define TAB_BG_TRC_REC(A,B)	B,
enum bg_trc_rec_type
{
#include "tab_bg_trc_rec.h"
n_bg_trc_rec_types
};
#undef TAB_BG_TRC_REC

#define UPGRD_WARN_INTERVAL (60 * 60 * 24)	/* Once every 24 hrs */
typedef struct compswap_time_field_struct
{	/* This structure is used where we want to do a compare-n-swap (CAS) on a time value. The CAS interfaces
	   need an instance of global_latch_t to operate on. We will utilize the "latch_pid" field to hold the
	   time and the latch_word is unused except on VMS where it will hold 0. Since this structure must be of
	   a constant size (size of global_latch_t varies), pad the latch with sufficient space to match the
	   size of global_latch_t's largest size (on HPUX).
	*/
	global_latch_t	time_latch;
#ifndef __hppa
	int4		hp_latch_space[4];	/* padding only on non-hpux systems */
#endif
} compswap_time_field;
/* takes value of time() but needs to be 4 byte so can use compswap on it. Not
   using time_t as that is an indeterminate size on various platforms.
   Value is time (in seconds) in a compare/swap updated field so only one
   process performs a given task in a given interval
*/
#define cas_time time_latch.u.parts.latch_pid

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
#  define ENDIANTHIS		"BIG"
#  define ENDIANOTHER		"LITTLE"
#  define ENDIANCHECKTHIS	big_endian
#else
#  define ENDIANTHIS		"LITTLE"
#  define ENDIANOTHER		"BIG"
#  define ENDIANCHECKTHIS	little_endian
#endif

#define CHECK_DB_ENDIAN(CSD,FNLEN,FNNAME)								\
{													\
	error_def(ERR_DBENDIAN);									\
	endian32_struct	check_endian;									\
	check_endian.word32 = (CSD)->minor_dbver;							\
	if (!check_endian.shorts.ENDIANCHECKTHIS)							\
		rts_error(VARLSTCNT(6) ERR_DBENDIAN, 4, FNLEN, FNNAME, ENDIANOTHER, ENDIANTHIS);	\
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
						   file-header or API changes occur. See note at top of sgmnt_data.
						*/
	uint4		jnl_checksum;
	char		filler_128[8];
	/************* FIELDS SET AT CREATION TIME ********************************/
	char		filler_created[52];	/* Now unused .. was "file_info created" */
	boolean_t	createinprogress;	/* TRUE only if MUPIP CREATE is in progress. FALSE otherwise */
	gtm_time8	creation;		/* time when the database file was created */

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
	uint4		owner_node;		/* Node on cluster that "owns" the file */
	uint4		image_count;		/* for db freezing. Set to "process_id" on Unix and "image_count" on VMS */
	uint4		freeze;			/* for db freezing. Set to "getuid"     on Unix and "process_id"  on VMS */
	int4		kill_in_prog;		/* counter for multi-crit kills that are not done yet */
	char		filler_320[12];
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
	boolean_t	fully_upgraded;		/* Set to TRUE by MUPIP REORG UPGRADE/INTEG/MUPIP CREATE when blks_to_upgrd is 0;
						 * If set to TRUE, this guarantees all blocks in the database are upgraded.
						 * "blks_to_upgrd" being 0 does not necessarily guarantee the same since the
						 *	counter might have become incorrect (due to presently unknown reasons).
						 * set to FALSE whenever desired_db_format changes or the database is
						 *	updated with V4 format blocks (by MUPIP JOURNAL).
						 */
	char		filler_384[20];
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
	volatile boolean_t wc_blocked;		/* Set to TRUE by process that knows it is leaving the cache in a possibly
						 * inconsistent state. Next process grabbing crit will do cache recovery.
						 * This setting also stops all concurrent writers from working on the cache.
						 * In MM mode, it is used to call wcs_recover during a file extension
						 */
	char		filler_512[20];
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
	int4		n_retries[CDB_MAX_TRIES];
						/* Counts of the number of retries it took to commit a transaction */
	int4		n_puts;			/* number of puts (non-tp only) */
	int4		n_kills;		/* number of kills */
	int4		n_queries;		/* number of $Query's */
	int4		n_gets;			/* number of MUMPS GETS */
	int4		n_order;		/* number of orders */
	int4		n_zprevs;		/* number of $ZPrevious's */
	int4		n_data;			/* number of datas */
	uint4		n_puts_duplicate;	/* number of duplicate sets in non-TP */
	uint4		n_tp_updates;		/* number of TP transactions that incremented db curr_tn for this region */
	uint4		n_tp_updates_duplicate;	/* number of TP transactions that were pure duplicate sets in this region */
	char		filler_accounting[4];	/* to ensure this section has 64-byte multiple size */
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
	seq_num		dualsite_resync_seqno;	/* Last known seqno communicated with the other side of the replication pipe.
						 * This field is maintained as long as the other side is still running a
						 * dual-site GT.M version. Once all replication instances in a configuration
						 * are upgraded to the multi-site GT.M version, this field is no longer used.
						 */
	char		filler_repl[16];	/* to ensure this section has 64-byte multiple size */
#endif
	/************* TP RELATED FIELDS ********************/
	int4		n_tp_retries[12];	/* indexed by t_tries; incremented by 1 for all regions in restarting TP */
	int4		n_tp_retries_conflicts[12];/* indexed by t_tries and incremented for conflicting region in TP */
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
	int4		jnl_sync_io;		/* drives sync I/O ('direct' if applicable) for journals, if set */
	int4		yield_lmt;		/* maximum number of times a process yields to get optimal jnl writes */
	char		filler_jnl[20];		/* to ensure this section has 64-byte multiple size */
	/************* INTERRUPTED RECOVERY RELATED FIELDS ****************/
	seq_num		intrpt_recov_resync_seqno;/* resync/fetchresync jnl_seqno of interrupted rollback */
	jnl_tm_t	intrpt_recov_tp_resolve_time;/* since-time for the interrupted recover */
	boolean_t	recov_interrupted;	/* whether a MUPIP JOURNAL RECOVER/ROLLBACK on this db got interrupted */
	int4		intrpt_recov_jnl_state;	/* journaling state at start of interrupted recover/rollback */
	int4		intrpt_recov_repl_state;/* replication state at start of interrupted recover/rollback */
	char		filler_1k[40];
	/************* HUGE CHARACTER ARRAYS **************/
	unsigned char	jnl_file_name[JNL_NAME_SIZE];	/* journal file name */
	unsigned char	reorg_restart_key[256];         /* 1st key of a leaf block where reorg was done last time */
        char		machine_name[MAX_MCNAMELEN];
	char		filler_2k[256];
   	/************* BG_TRC_REC RELATED FIELDS ***********/
#	define TAB_BG_TRC_REC(A,B)	bg_trc_rec_tn	B##_tn;
#	include "tab_bg_trc_rec.h"
#	undef TAB_BG_TRC_REC
	char		bg_trc_rec_tn_filler  [1200 - (sizeof(bg_trc_rec_tn) * n_bg_trc_rec_types)];

#	define TAB_BG_TRC_REC(A,B)	bg_trc_rec_cntr	B##_cntr;
#	include "tab_bg_trc_rec.h"
#	undef TAB_BG_TRC_REC
	char		bg_trc_rec_cntr_filler[ 600 - (sizeof(bg_trc_rec_cntr) * n_bg_trc_rec_types)];

   	/************* DB_CSH_ACCT_REC RELATED FIELDS ***********/
#	define	TAB_DB_CSH_ACCT_REC(A,B,C)	db_csh_acct_rec	A;
#	include "tab_db_csh_acct_rec.h"
#	undef TAB_DB_CSH_ACCT_REC
	char		db_csh_acct_rec_filler[ 1256 - (sizeof(db_csh_acct_rec) * n_db_csh_acct_rec_types)];
	/************* DB CREATION AND UPGRADE CERTIFICATION FIELDS ***********/
	enum db_ver	creation_db_ver;		/* Major DB version at time of creation */
	enum mdb_ver	creation_mdb_ver;		/* Minor DB version at time of creation */
	enum db_ver	certified_for_upgrade_to;	/* Version the database is certified for upgrade to */
	int		filler_5K;
   	/************* SECSHR_DB_CLNUP RELATED FIELDS ***********/
   	int4		secshr_ops_index;
   	int4		secshr_ops_array[255];	/* taking up 1K */
   	/********************************************************/
	compswap_time_field next_upgrd_warn;	/* Time when we can send the next upgrade warning to the operator log */
	char		filler_7k[1000];
	char		filler_8k[1024];
   	/********************************************************/
	/* Master bitmap immediately follows. Tells whether the local bitmaps have any free blocks or not. */
} sgmnt_data;

/* Block types for shmpool_blk_hdr */
enum shmblk_type
{
	SHMBLK_FREE = 1,	/* Block is not in use */
	SHMBLK_REFORMAT,	/* Block contains reformat information */
	SHMBLK_BACKUP		/* Block in use by online backup */
};

#define		SHMPOOL_BUFFER_SIZE	0x00100000	/* 1MB pool for shared memory buffers (backup and downgrade) */

/* These shared memory blocks are in what was called the "backup buffer" in shared memory. It is a 1MB area
   of shared mem storage with (now) multiple uses. It is used by both backup and the online downgrade process
   (the latter on VMS only). Free blocks are queued in shared memory.
*/

typedef struct shmpool_blk_hdr_struct
{	/* Block header for each block in shmpool buffer area. The data portion of the block immediately follows this header and
	   in the case of backup, is written out to the temporary file with the data block appended to it. Same holds true for
	   writing out incremental backup files so any change to this structure warrants consideration of changing the format
	   version for the incremental backup (INC_HEADER_LABEL in murest.h).
	 */
	que_ent		sm_que;			/* Main queue fields */
	volatile enum shmblk_type blktype;	/* free, backup or reformat? */
	block_id	blkid;			/* block number */
	union
	{
		struct
		{	/* Use for backup */
			enum db_ver	ondsk_blkver;	/* Version of block from cache_rec */
			VMS_ONLY(int4	filler;)	/* If VMS, this structure will be 2 words since rfrmt struct is */
		} bkup;
#ifdef VMS
		struct
		{	/* Use in downgrade mode (as reformat buffer) */
			volatile sm_off_t	cr_off;	/* Offset to cache rec associated with this reformat buffer */
			volatile int4		cycle;	/* cycle of given cache record (to validate we have same CR */
		} rfrmt;
#endif
	} use;
	pid_t		holder_pid;		/* PID holding/using this buffer */
	boolean_t	valid_data;		/* This buffer holds valid data (else not initialized) */
	int4		image_count;		/* VMS only */
	VMS_ONLY(int4	filler;)		/* 8 byte alignment. Only necessary for VMS since bkup struct will only
						   be 4 bytes for UNIX and this filler is not then necessary for alignment.
						 */
} shmpool_blk_hdr;

/* Header of the shmpool buffer area. Describes contents */
typedef struct  shmpool_buff_hdr_struct
{
	global_latch_t  shmpool_crit_latch;	/* Latch to update header fields */
	off_t           dskaddr;		/* Highest disk address used (backup only) */
	trans_num       backup_tn;		/* TN at start of full backup (backup only) */
	trans_num       inc_backup_tn;		/* TN to start from for incremental backup (backup only) */
	char            tempfilename[256];	/* Name of temporary file we are using (backup only) */
	que_ent		que_free;		/* Queue header for all free elements */
	que_ent		que_backup;		/* Queue header for all (allocated) backup elements */
	VMS_ONLY(que_ent que_reformat;)		/* Queue header for all (allocated) reformat elements */
	volatile int4	free_cnt;		/* Elements on free queue */
	volatile int4   backup_cnt;		/* Elements used for backup */
	volatile int4	reformat_cnt;		/* Elements used for reformat */
	volatile int4	allocs_since_chk;	/* Allocations since last lost block check */
	uint4		total_blks;		/* Total shmpool block buffers in 1MB buffer area */
	uint4		blk_size;		/* Size of the created buffers (excluding header - convenient blk_size field) */
	pid_t           failed;			/* Process id that failed to write to temp file causing failure (backup only) */
	int4            backup_errno;		/* errno value when "failed" is set (backup only) */
	uint4           backup_pid;		/* Process id performing online backup (backup only) */
	uint4           backup_image_count;	/* Image count of process running online backup (VMS & backup only) */
	boolean_t	shmpool_blocked;	/* secshr_db_clnup() detected a problem on shutdown .. force recovery */
	uint4		filler;			/* 8 byte alignment */
} shmpool_buff_hdr;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef	shmpool_buff_hdr	*shmpool_buff_hdr_ptr_t;
typedef	shmpool_blk_hdr		*shmpool_blk_hdr_ptr_t;
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
	FILL8DCL(sm_uc_ptr_t, base_addr, 1);
	FILL8DCL(mmblk_que_heads_ptr_t, mmblk_state, 2);	/* pointer to beginnings of state and blk queues */
} sgmm_addrs;

#define MAX_NM_LEN	MAX_MIDENT_LEN
#define MIN_RN_LEN	1
#define MAX_RN_LEN	MAX_MIDENT_LEN
#define MIN_SN_LEN	1
#define MAX_SN_LEN	MAX_MIDENT_LEN
#define STR_SUB_PREFIX  0x0FF
#define SUBSCRIPT_STDCOL_NULL 0x01
#define STR_SUB_ESCAPE  0X01
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
	short			defer_time; 	/* Was passed in cs_addrs */
	unsigned char		buckets;	/* Was passed in FAB */
	unsigned char		windows;	/* Was passed in FAB */
	uint4			lock_space;
	uint4			global_buffers;	/* Was passed in FAB */
	uint4			reserved_bytes;	/* number of bytes to be left in every database block */
	enum db_acc_method	acc_meth;
	file_control		*file_cntl;
	struct gd_region_struct	*repl_list;
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
	unsigned short		jnl_deq;
	short			jnl_buffer_size;
	bool			jnl_before_image;
	bool			opening;
	bool			read_only;
	bool			was_open;
	unsigned char		cmx_regnum;
	unsigned char		def_coll;
	bool			std_null_coll;	/* 0 -> GT.M null collation,i,e, null subs collate between numeric and string
					 	* 1-> standard null collation i.e. null subs collate before numeric and string */
	unsigned char		jnl_file_len;
	unsigned char		jnl_file_name[JNL_NAME_SIZE];

	/* VMS file id struct goes to OS specific struct */
	/* VMS lock structure for reference goes to OS specific struct */

	int4			node;
	int4			sec_size;
} gd_region;

typedef struct	sgmnt_addrs_struct
{
	sgmnt_data_ptr_t hdr;
	sm_uc_ptr_t	bmm;
	sm_uc_ptr_t	wc;
	bt_rec_ptr_t	bt_header;
	bt_rec_ptr_t	bt_base;
	th_rec_ptr_t	th_base;
	th_index_ptr_t	ti;
	node_local_ptr_t nl;
	mutex_struct_ptr_t critical;
	sm_uc_ptr_t	db_addrs[2];
	sm_uc_ptr_t	lock_addrs[2];

	union
	{
		sgmm_addrs	mm;
		sgbg_addrs	bg;
	/* May add new pointers here for other methods or change to void ptr */

	} acc_meth;
	struct gv_namehead_struct	*dir_tree;
	struct sgmnt_addrs_struct	*next_fenced;
	struct jnl_private_control_struct *jnl;
	uint4		total_blks;		/* Last we knew, file was this big. Used to
						 * signal MM processing file was extended and
						 * needs to be remapped.                     */
	uint4		prev_free_blks;

	/* The following uint4's are treated as bools but must be 4 bytes to avoid interaction between
	   bools in interrupted routines and possibly lost data */
	volatile uint4	timer;                  /* This process has a timer for this region */
	volatile uint4	in_wtstart;		/* flag we are busy writing */
	volatile uint4	now_crit;		/* This process has the critical write lock */
	volatile uint4	read_lock;		/* This process has the critical read lock */
	volatile uint4	wbuf_dqd;		/* A write buffer has been dequeued - signals that
						   extra cleanup required if die while on */
	uint4		stale_defer;		/* Stale processing deferred this region */

	short		segnum;
	short		n_new_wcrs;
	short		n_dsk_writes;

	bool		clustered;
	bool		in_wt_wcs_flu;
	unsigned char	mb_mess[6];
	bool		locking_flush;
	bool		freeze;
	bool		t_commit_crit;
	shmpool_buff_hdr_ptr_t shmpool_buffer;	/* 1MB chunk of shared memory that we micro manage */
	struct sgm_info_struct	*sgm_info_ptr;
	volatile boolean_t	dbsync_timer;	/* whether a timer to sync the filehdr (and write epoch) is active */
	gd_region	*region;		/* the region corresponding to this csa */
	block_id	reorg_last_dest;	/* last destinition block used for swap */
	boolean_t	jnl_before_image;
	boolean_t	read_write;
	boolean_t	extending;
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
	int4		ref_cnt;		/* count of number of times csa->nl->ref_cnt was incremented by this process */
	int4		fid_index;		/* index for region ordering based on unique_id */
	boolean_t	do_fullblockwrites;	/* This region enabled for full block writes */
	size_t		fullblockwrite_len;	/* Length of a full block write */
	int4		regnum;			/* Region number (region open counter) used by journaling so all tokens
						   have a unique prefix per region (and all regions have same prefix)
						*/
	int4		n_pre_read_trigger;	/* For update process to keep track of progress and when to trigger pre-read */
	boolean_t	replinst_matches_db;	/* TRUE if replication instance file name stored in db shared memory matches the
						 * instance file name stored in the journal pool that this process has attached to.
						 * Updates are allowed to this replicated database only if this is TRUE.
						 */
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
	struct cw_set_element_struct	*ptr;
	struct srch_blk_status_struct	*first_tp_srch_status;
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

typedef struct	gv_key_struct
{
	unsigned short	top;		/* Offset to top of buffer allocated for the key */
	unsigned short	end;		/* End of the current key. Offset to the second null */
	unsigned short	prev;		/* Offset to the start of the previous subscript.
					 * This is used for global nakeds.
					 */
	unsigned char	base[1];	/* Base of the key */
} gv_key;

/* Any change to this structure should also have a corresponding [re]initialization in mupip_recover.c
 * in the code where we play the records in the forward phase i.e. go through each of the jnl_files
 * and within if (mur_options.update), initialize necessary fields of gv_target before proceeding with mur_forward().
 */
typedef struct	gv_namehead_struct
{
	gd_region	*gd_reg;			/* Region of key */
	gv_key		*first_rec, *last_rec;		/* Boundary recs of clue's data block */
	struct gv_namehead_struct *next_gvnh;		/* Used to chain gv_target's together */
	trans_num	read_local_tn;			/* local_tn of last reference for this global */
	trans_num	write_local_tn;			/* local_tn of last update for this global */
	mname_entry	gvname;				/* the name of the global */
	boolean_t	noisolation;     		/* whether isolation is turned on or off for this global */
	block_id	root;				/* Root of global variable tree */
	srch_hist	hist;				/* block history array */
	unsigned char	nct;				/* numerical collation type for internalization */
	unsigned char	act;				/* alternative collation type for internalization */
	unsigned char	ver;
	char		filler[1];
	srch_hist	*alt_hist;			/* alternate history. initialized once per gv_target */
	struct collseq_struct	*collseq;		/* pointer to a linked list of user supplied routine addresses
			 				   for internationalization */
	NON_GTM64_ONLY(uint4		filler_uint4;)	/* To make clue start at 8-byte boundary */
	gv_key		clue;				/* Clue key, must be last in namehead struct because of hung buffer */
} gv_namehead;

#define INVALID_GV_TARGET (gv_namehead *)-1L

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
 * (in preemptive_ch()). For SUCCESS or INFO, no restoration is necessary because CONTINUE from the condition
 * handlers would take us through the normal path for gv_target restoration.
 */

#define RESET_GV_TARGET               			\
{							\
	assert(INVALID_GV_TARGET != reset_gv_target);	\
	gv_target = reset_gv_target;			\
       	reset_gv_target = INVALID_GV_TARGET;    	\
}

#define RESET_GV_TARGET_LCL(SAVE_TARG) 	gv_target = SAVE_TARG;

#define RESET_GV_TARGET_LCL_AND_CLR_GBL(SAVE_TARG)						\
{												\
	gv_target = SAVE_TARG;									\
	if (!gbl_target_was_set)								\
	{											\
		assert(SAVE_TARG == reset_gv_target || INVALID_GV_TARGET == reset_gv_target);	\
		reset_gv_target = INVALID_GV_TARGET;						\
	}											\
}

#define COPY_SUBS_TO_GVCURRKEY(mvarg, gv_currkey, was_null, is_null)								\
{																\
	GBLREF mv_stent		*mv_chain;											\
	GBLREF unsigned char	*msp, *stackwarn, *stacktop;									\
	mval			*temp;												\
																\
	error_def(ERR_STACKOFLOW);												\
	error_def(ERR_STACKCRIT);												\
																\
	was_null |= is_null;													\
	if (mvarg->mvtype & MV_SUBLIT)												\
	{															\
		is_null = ((STR_SUB_PREFIX == *(unsigned char *)mvarg->str.addr) && (KEY_DELIMITER == *(mvarg->str.addr + 1))); \
		if (gv_target->collseq || gv_target->nct)									\
		{														\
			assert(dba_cm != gv_cur_region->dyn.addr->acc_meth); /* collation transformation should be done at the	\
										server's end for CM regions */			\
			transform = FALSE;											\
			end = gvsub2str((uchar_ptr_t)mvarg->str.addr, buff, FALSE);						\
			transform = TRUE;											\
			/* it does not seem like we need the PUSH_MV_STENT and POP_MV_STENT here --- nars - 2003/09/17 */	\
			PUSH_MV_STENT(MVST_MVAL);										\
			temp = &mv_chain->mv_st_cont.mvs_mval;									\
			temp->mvtype = MV_STR;											\
			temp->str.addr = (char *)buff;										\
			temp->str.len = (mstr_len_t)(end - buff);								\
			mval2subsc(temp, gv_currkey);										\
			POP_MV_STENT(); /* temp */										\
		} else														\
		{														\
			len = mvarg->str.len;											\
			if (gv_currkey->end + len - 1 >= max_key)								\
			{													\
				if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))			\
					end = &buff[MAX_ZWR_KEY_SZ - 1];							\
				rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buff, buff);			\
			}													\
			memcpy((gv_currkey->base + gv_currkey->end), mvarg->str.addr, len);					\
			if (is_null && 0 != gv_cur_region->std_null_coll)							\
				gv_currkey->base[gv_currkey->end] = SUBSCRIPT_STDCOL_NULL;					\
			gv_currkey->prev = gv_currkey->end;									\
			gv_currkey->end += len - 1;										\
		}														\
	} else															\
	{															\
		mval2subsc(mvarg, gv_currkey);											\
		if (gv_currkey->end >= max_key)											\
		{														\
			if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))				\
				end = &buff[MAX_ZWR_KEY_SZ - 1 ];								\
			rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buff, buff);				\
		}														\
	 	is_null = (MV_IS_STRING(mvarg) && (0 == mvarg->str.len));							\
	}															\
}

/* The enum codes below correspond to code-paths that can increment the database curr_tn
 * without having a logical update. Journaling currently needs to know all such code-paths */
typedef enum
{
        inctn_invalid_op = 0,
        inctn_bmp_mark_free_gtm,
        inctn_bmp_mark_free_mu_reorg,
        inctn_gdsfilext_gtm,
        inctn_gdsfilext_mu_reorg,
        inctn_gvcstput_extra_blk_split,
        inctn_mu_reorg,
        inctn_wcs_recover,
	inctn_db_format_change,		/* written when cs_data->desired_db_format changes */
	inctn_blkupgrd,		/* written whenever a GDS block is upgraded by MUPIP REORG UPGRADE if
					 * a) SAFEJNL is specified OR
					 * b) NOSAFEJNL is specified and the block is not undergoing a fmt change
					 */
	inctn_blkupgrd_fmtchng,	/* written whenever a GDS block is upgraded by MUPIP REORG UPGRADE -NOSAFEJNL
					 * and if that block is undergoing a fmt change i.e. (GDSV4 -> GDSV5) OR (GDSV5 -> GDSV4).
					 * This differentiation (inctn_blkupgrd vs inctn_blkupgrd_fmtch) is necessary
					 * because in the latter case we will not be writing a PBLK record and hence have no
					 * record otherwise of a block fmt change if it occurs (note that a PBLK journal record's
					 * "ondsk_blkver" field normally helps recovery determine if a fmt change occurred or not).
					 */
	inctn_blkdwngrd,		/* similar to inctn_blkupgrd except that this is for DOWNGRADE */
	inctn_blkdwngrd_fmtchng,	/* similar to inctn_blkupgrd_fmtchng except that this is for DOWNGRADE */
        inctn_opcode_total
} inctn_opcode_t;

/* macros to check curr_tn */
#define MAX_TN_V4	((trans_num)(MAXUINT4 - TN_HEADROOM_V4))
#define MAX_TN_V5	(MAXUINT8 - TN_HEADROOM_V5)
#define TN_HEADROOM_V4	(2 * MAXTOTALBLKS_V4)
#define TN_HEADROOM_V5	(2 * MAXTOTALBLKS_V5)
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
		error_def(ERR_TNTOOLARGE);										\
		error_def(ERR_TNWARN);											\
															\
		if ((CSA)->hdr->max_tn <= (TN))										\
		{													\
			rts_error(VARLSTCNT(5) ERR_TNTOOLARGE, 3, DB_LEN_STR((CSA)->region), &(CSA)->hdr->max_tn);	\
			assert(FALSE);	/* should not come here */							\
		}													\
		assert((CSD)->max_tn > (TN));										\
		trans_left = (CSD)->max_tn - (TN);									\
		send_msg(VARLSTCNT(6) ERR_TNWARN, 4, DB_LEN_STR((CSA)->region), &trans_left, &(CSD)->max_tn);		\
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
	headroom = (gtm_uint64_t)(GDSV4 == (CSD)->desired_db_format ? TN_HEADROOM_V4 : TN_HEADROOM_V5);	\
	headroom *= HEADROOM_FACTOR;									\
	(ret_warn_tn) = (CSD)->trans_hist.curr_tn;							\
	if ((headroom < (CSD)->max_tn) && ((ret_warn_tn) < ((CSD)->max_tn - headroom)))			\
		(ret_warn_tn) = (CSD)->max_tn - headroom;						\
	assert((CSD)->trans_hist.curr_tn <= (ret_warn_tn));						\
	assert((ret_warn_tn) <= (CSD)->max_tn);								\
}

#define HIST_TERMINATOR		0
#define HIST_SIZE(h)		( (sizeof(int4) * 2) + (sizeof(srch_blk_status) * ((h).depth + 1)) )
#define KEY_COPY_SIZE(k)	( sizeof(gv_key) + (k)->end)   /* key and 2 trailing zeroes */

/* Start of lock space in a bg file, therefore also doubles as overhead size for header, bt and wc queues F = # of wc blocks */
#define LOCK_BLOCK(X) (DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(X) + BT_SIZE(X), DISK_BLOCK_SIZE))
#define LOCK_BLOCK_SIZE(X) (DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(X) + BT_SIZE(X), OS_PAGE_SIZE))
#define	LOCK_SPACE_SIZE(X)	(ROUND_UP(((sgmnt_data_ptr_t)X)->lock_space_size, OS_PAGE_SIZE))
#define CACHE_CONTROL_SIZE(X) 												\
	(ROUND_UP((ROUND_UP((((sgmnt_data_ptr_t)X)->bt_buckets + ((sgmnt_data_ptr_t)X)->n_bts) * sizeof(cache_rec)	\
								+ sizeof(cache_que_heads), OS_PAGE_SIZE)		\
		+ (((sgmnt_data_ptr_t)X)->n_bts * ((sgmnt_data_ptr_t)X)->blk_size)), OS_PAGE_SIZE))
#define MMBLK_CONTROL_SIZE(X) (ROUND_UP((((sgmnt_data_ptr_t)X)->bt_buckets + ((sgmnt_data_ptr_t)X)->n_bts) * sizeof(mmblk_rec) \
	+ sizeof(mmblk_que_heads), OS_PAGE_SIZE))

OS_PAGE_SIZE_DECLARE

#define MAX_NAME_LEN		31	/* Size of a repl resource name on vvms */
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

#define DECR_KIP(CSD, CSA, KIP_FLAG)				\
{								\
	assert(KIP_FLAG);					\
	KIP_FLAG = FALSE;					\
	DECR_CNT(&CSD->kill_in_prog, &CSA->nl->wc_var_lock);	\
}
/* Note that the INCR_KIP and CAREFUL_INCR_KIP macros should be maintained in parallel */
#define INCR_KIP(CSD, CSA, KIP_FLAG)				\
{								\
	assert(!KIP_FLAG);					\
	INCR_CNT(&CSD->kill_in_prog, &CSA->nl->wc_var_lock);	\
	KIP_FLAG = TRUE;					\
}
/* The CAREFUL_INCR_KIP macro is the same as the INCR_KIP macro except that it uses CAREFUL_INCR_CNT instead of INCR_CNT.
 * This does alignment checks and is needed by secshr_db_clnup as it runs in kernel mode in VMS.
 * The INCR_KIP and CAREFUL_INCR_KIP macros should be maintained in parallel.
 */
#define CAREFUL_INCR_KIP(CSD, CSA, KIP_FLAG)				\
{									\
	assert(!KIP_FLAG);						\
	CAREFUL_INCR_CNT(&CSD->kill_in_prog, &CSA->nl->wc_var_lock);	\
	KIP_FLAG = TRUE;						\
}
#define INVALID_SEMID -1
#define INVALID_SHMID -1L

#if defined(UNIX)
#define DB_FSYNC(reg, udi, csa, db_fsync_in_prog, save_errno)					\
{												\
	BG_TRACE_PRO_ANY(csa, n_db_fsyncs);							\
	if (csa->now_crit)									\
		BG_TRACE_PRO_ANY(csa, n_db_fsyncs_in_crit);					\
	db_fsync_in_prog++;									\
	save_errno = 0;										\
	if (-1 == fsync(udi->fd))								\
	{											\
		db_fsync_in_prog--;								\
		save_errno = errno;								\
	}											\
	db_fsync_in_prog--;									\
	assert(0 <= db_fsync_in_prog);								\
}

#define STANDALONE(x) mu_rndwn_file(x, TRUE)
#define DBFILOP_FAIL_MSG(status, msg)	gtm_putmsg(VARLSTCNT(5) msg, 2, DB_LEN_STR(gv_cur_region), status);
#elif defined(VMS)
#define STANDALONE(x) mu_rndwn_file(TRUE)	/* gv_cur_region needs to be equal to "x" */
#define DBFILOP_FAIL_MSG(status, msg)	gtm_putmsg(VARLSTCNT(6) msg, 2, DB_LEN_STR(gv_cur_region), status, \
						   FILE_INFO(gv_cur_region)->fab->fab$l_stv);
#else
#error unsupported platform
#endif

#define CR_NOT_ALIGNED(cr, cr_base)		(!IS_PTR_ALIGNED((cr), (cr_base), sizeof(cache_rec)))
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
	error_def(ERR_DBCRERR);									\
												\
	cr_top = GDS_ANY_ABS2REL(csa, cr_hi);							\
	bp_lo = ROUND_UP(cr_top, OS_PAGE_SIZE);							\
	bp = bp_lo + ((cr) - (cr_lo)) * csd->blk_size;						\
	if (bp != cr->buffaddr)									\
	{											\
		send_msg(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr, cr->blk, 		\
			RTS_ERROR_TEXT("cr->buffaddr"), cr->buffaddr, bp, CALLFROM);		\
		cr->buffaddr = bp;								\
	}											\
	DEBUG_ONLY(bp_top = bp_lo + csd->n_bts * csd->blk_size;)				\
	assert(IS_PTR_IN_RANGE(bp, bp_lo, bp_top) && IS_PTR_ALIGNED(bp, bp_lo, csd->blk_size));	\
}

#define	IS_DOLLAR_INCREMENT			((is_dollar_incr) && (ERR_GVPUTFAIL == t_err))

#define AVG_BLKS_PER_100_GBL		200
#define PRE_READ_TRIGGER_FACTOR		50
#define UPD_RESERVED_AREA		50
#define UPD_WRITER_TRIGGER_FACTOR	33

void		assert_jrec_member_offsets(void);
bt_rec_ptr_t	bt_put(gd_region *r, int4 block);
void		bt_que_refresh(gd_region *greg);
void		bt_init(sgmnt_addrs *cs);
void		bt_malloc(sgmnt_addrs *csa);
void		bt_refresh(sgmnt_addrs *csa);
void		db_common_init(gd_region *reg, sgmnt_addrs *csa, sgmnt_data_ptr_t csd);
void		grab_crit(gd_region *reg);
void		grab_lock(gd_region *reg);
void		gv_init_reg(gd_region *reg);
void		gvcst_init(gd_region *greg);
enum cdb_sc	gvincr_compute_post_incr(srch_blk_status *bh);
enum cdb_sc	gvincr_recompute_upd_array(srch_blk_status *bh, struct cw_set_element_struct *cse, cache_rec_ptr_t cr);
boolean_t	mupfndfil(gd_region *reg, mstr *mstr_addr);
boolean_t	region_init(bool cm_regions);
bool		region_freeze(gd_region *region, bool freeze, bool override);
void		rel_crit(gd_region *reg);
void		rel_lock(gd_region *reg);
bool		wcs_verify(gd_region *reg, boolean_t expect_damage, boolean_t caller_is_wcs_recover);
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
void db_csh_ref(sgmnt_addrs *cs_addrs);
cache_rec_ptr_t db_csh_get(block_id block);
cache_rec_ptr_t db_csh_getn(block_id block);

enum cdb_sc tp_hist(srch_hist *hist1);

sm_uc_ptr_t get_lmap(block_id blk, unsigned char *bits, sm_int_ptr_t cycle, cache_rec_ptr_ptr_t cr);

bool ccp_userwait(struct gd_region_struct *reg, uint4 state, int4 *timadr, unsigned short cycle);
void ccp_closejnl_ast(struct gd_region_struct *reg);
bt_rec *ccp_bt_get(sgmnt_addrs *cs_addrs, int4 block);
unsigned char *mval2subsc(mval *in_val, gv_key *out_key);

int4 dsk_read(block_id blk, sm_uc_ptr_t buff, enum db_ver *ondisk_blkver);

void jnl_flush(gd_region *reg);
void jnl_fsync(gd_region *reg, uint4 fsync_addr);
void jnl_mm_timer(sgmnt_addrs *csa, gd_region *reg);
void jnl_oper_user_ast(gd_region *reg);
void jnl_wait(gd_region *reg);
void view_jnlfile(mval *dst, gd_region *reg);
void jnl_put_jrt_pfin(sgmnt_addrs *csa);
void jnl_put_jrt_pini(sgmnt_addrs *csa);
void jnl_write_epoch_rec(sgmnt_addrs *csa);
void jnl_write_inctn_rec(sgmnt_addrs *csa);
void fileheader_sync(gd_region *reg);

gd_addr *create_dummy_gbldir(void);

#include "gdsfheadsp.h"

/* End of gdsfhead.h */

#endif
