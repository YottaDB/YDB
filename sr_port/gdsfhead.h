/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GDSFHEAD_H__
#define __GDSFHEAD_H__

/* gdsfhead.h */
/* this requires gdsroot.h gtm_facility.h fileinfo.h gdsbt.h */

#include <sys/types.h>
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
	trans_num	dirty;
	uint4		refer;
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
	trans_num	dirty;
	uint4		refer;
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
	trans_num	dirty;		/* block has been modified since last written to disk; used by bt_put, db_csh_getn
  					 * mu_rndwn_file wcs_recover, secshr_db_clnup, wr_wrtfin_all and extensively by the ccp */
	uint4		refer;		/* reference bit for the clock algorithm */

        /* Keep our 64 bit fields up front */
	/* this point should be quad-word aligned */

	FILL8DCL(sm_off_t,bt_index,1);	/* offset to bt_rec */
	FILL8DCL(sm_off_t,buffaddr,2);	/* offset to buffer holding actual data*/
	JNL_OFF_T(jnl_addr,3);		/* offset from bg_update to prevent wcs_wtstart from writing a block ahead of the journal */
        FILL8DCL(sm_off_t,twin,4);      /* offset to cache_rec of another copy of the same block from bg_update & wcs_wt_all(VMS)*/
	global_latch_t	rip_latch;	/* for read_in_progress - 16 bytes for HPPA */

	/* and now the rest */

	int4		image_count;	/* maintained with r_epid in vms to ensure that the process has stayed in gt.m */
	trans_num	tn;
	int4		epid;		/* set by wcs_start to id the write initiator; cleared by wcs_wtfini
   					 * used by t_commit_cleanup, secshr_db_clnup and wcs_recover */
	trans_num	flushed_dirty_tn;	/* value of dirty at the time of flushing */
	int4		filler_rip_latch;	/* used in UNIX implementations as a semaphore for updates to read_in_progress */
	int4		cycle;		/* relative stamp indicates changing versions of the block for concurrency checking */
	int4		r_epid;		/* set by db_csh_getn, cleared by t_qread, bg_update, wcs_recover or secshr_db_clnup
  					 * used to check for process leaving without releasing the buffer
					 * must be word aligned on the VAX */
	CNTR4DCL(read_in_progress,10);	/* -1 for normal and 0 for rip used by t_qread and checked by others */
#ifdef VMS
	io_status_block_disk	iosb;	/* used on VMS write */
#endif
	bool		in_tend;	/* TRUE from bg_update indicates secshr_db_clnup should finish update */
	bool		data_invalid;	/* TRUE from bg_update indicates t_commit_cleanup and wcs_recover should invalidate */
	bool		stopped;	/* TRUE indicates to wcs_recover that secshr_db_clnup built the block */
	bool		wip_stopped;	/* TRUE indicates to wcs_recover, wcs_wtfini, wcs_get_blk and gds_rundown
  					 * that secshr_db_clnup cancelled the qio*/
	bool		in_cw_set;	/* TRUE from t_end, tp_tend or bg_update protects block from db_csh_getn; returned to
  					 * FALSE by t_end, tp_tend or t_commit_cleanup */
	bool		second_filler_bool[3];	/* preserve double word alignment */
} cache_rec;

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
	trans_num	dirty;		/* block has been modified since last written to disk; used by bt_put, db_csh_getn
  					 * mu_rndwn_file wcs_recover, secshr_db_clnup, wr_wrtfin_all and extensively by the ccp */
	uint4		refer; 		/* reference bit for the LRU algorithm */

        /* Keep our 64 bit fields up front */
	/* this point should be quad-word aligned */

	FILL8DCL(sm_off_t,bt_index,1);	/* offset to bt_rec */
	FILL8DCL(sm_off_t,buffaddr,2);	/* offset to buffer holding actual data*/
	JNL_OFF_T(jnl_addr,3);	/* offset from bg_update to prevent wcs_wtstart from writing a block ahead of the journal */
	FILL8DCL(sm_off_t,twin,4);	/* offset to cache_rec of another copy of the same block from bg_update & wcs_wt_all(VMS)*/
	global_latch_t	rip_latch;	/* for read_in_progress - 16 bytes for HPPA */

	/* and now the rest */

	int4		image_count;	/* maintained with r_epid in vms to ensure that the process has stayed in gt.m */
	trans_num	tn;
	int4		epid;		/* set by wcs_start to id the write initiator; cleared by wcs_wtfini
   					 * used by t_commit_cleanup, secshr_db_clnup and wcs_recover */
	trans_num	flushed_dirty_tn;	/* value of dirty at the time of flushing */
	int4		filler_rip_latch;	/* used in UNIX implementations as a semaphore for updates to read_in_progress */
	int4		cycle;		/* relative stamp indicates changing versions of the block for concurrency checking */
	int4		r_epid;		/* set by db_csh_getn, cleared by t_qread, bg_update, wcs_recover or secshr_db_clnup
  					 * used to check for process leaving without releasing the buffer
					 * must be word aligned on the VAX */
	CNTR4DCL(read_in_progress,10);	/* -1 for normal and 0 for rip used by t_qread and checked by others */
#ifdef VMS
	io_status_block_disk	iosb;	/* used on VMS write */
#endif
	bool		in_tend;	/* TRUE from bg_update indicates secshr_db_clnup should finish update */
	bool		data_invalid;	/* TRUE from bg_update indicates t_commit_cleanup and wcs_recover should invalidate */
	bool		stopped;	/* TRUE indicates to wcs_recover that secshr_db_clnup built the block */
	bool		wip_stopped;	/* TRUE indicates to wcs_recover, wcs_wtfini, wcs_get_blk and gds_rundown
  					 * that secshr_db_clnup cancelled the qio*/
	bool		in_cw_set;	/* TRUE from t_end, tp_tend or bg_update protects block from db_csh_getn; returned to
  					 * FALSE by t_end, tp_tend or t_commit_cleanup */
	bool		second_filler_bool[3];	/* preserve double word alignment */
} cache_state_rec;

#define		CR_BLKEMPTY             -1
#define		FROZEN_BY_ROOT          (uint4)(0xFFFFFFFF)
#define		BACKUP_NOT_IN_PROGRESS  0x7FFFFFFF
#define		BACKUP_BUFFER_SIZE      0x000FFFFF      /* temporary value 1M */

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
	int4			bufindx;									\
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

#define GLO_PREFIX_LEN 4
#define GDS_V20	"02"
#define GDS_X21	"03"
#define GDS_V23 "04"
#define GDS_V24 "05"
#define GDS_V25 "06"
#define GDS_V254 "07"
#define GDS_V255 "08"
#define GDS_V30 "09"
#define GDS_V40 "0A"

enum db_ver
{
	v20 = 2,
	x21,
	v23,
	v24,
	v25,
	v254,
	v255,
	v320,
	v40
};

#define MASTER_MAP_BLOCKS 32				/* 32 give 64M possible blocks  */
#define MASTER_MAP_SIZE	(MASTER_MAP_BLOCKS * DISK_BLOCK_SIZE)	/* MUST be a multiple of DISK_BLOCK_SIZE */
#define SGMNT_HDR_LEN	(sizeof(sgmnt_data) - MASTER_MAP_SIZE)
#define MM_BLOCK	(SGMNT_HDR_LEN / DISK_BLOCK_SIZE + 1)	/* gt.m numbers blocks from 1 */
#define TH_BLOCK	2				/* If trans_hist moves from 2nd fileheader block change this */

#define JNL_NAME_SIZE	        256        /* possibly expanded when opened */
#define JNL_NAME_EXP_SIZE	1024       /* MAXPATHLEN, before jnl_buffer in shared memory */

#define BLKS_PER_LMAP		512
#define MAXTOTALBLKS		MASTER_MAP_SIZE * 8 * BLKS_PER_LMAP
#define	IS_BITMAP_BLK(blk)	(ROUND_DOWN2(blk, BLKS_PER_LMAP) == blk)	/* TRUE if blk is a bitmap */

#define	STEP_FACTOR			64		/* the factor by which flush_trigger is incremented/decremented */
#define	MIN_FLUSH_TRIGGER(n_bts)	((n_bts)/4)	/* the minimum flush_trigger as a function of n_bts */
#define	MAX_FLUSH_TRIGGER(n_bts)	((n_bts)*15/16)	/* the maximum flush_trigger as a function of n_bts */

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
#elif UNIX
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

/* Interlocked queue instruction constants ... */

#define	QI_STARVATION		3
#define EMPTY_QUEUE		0
#define QUEUE_WAS_EMPTY		1
#define INTERLOCK_FAIL		-1
#define QUEUE_INSERT_SUCCESS	1

typedef struct
{
	CNTR4DCL(evnt_cnt,100);
	trans_num	evnt_tn;
} bg_trc_rec;

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
	tp_blkmod_gvcst_put = 0,
	tp_blkmod_gvcst_srch,
	tp_blkmod_t_qread,
	tp_blkmod_tp_tend,
	tp_blkmod_tp_hist,
	n_tp_blkmod_types
};

#define	ARRAYSIZE(arr)	sizeof(arr)/sizeof(arr[0])

#define TAB_BG_TRC_REC(A,B)	B,
enum bg_trc_rec_fixed_type
{
#include "tab_bg_trc_rec_fixed.h"
n_bg_trc_rec_fixed_types
};
enum bg_trc_rec_variable_type
{
#include "tab_bg_trc_rec_variable.h"
n_bg_trc_rec_variable_types
};
#undef TAB_BG_TRC_REC

/* This is the structure describing a segment. It is used as a database file
 * header (for MM or BG access methods). The overloaded fields for MM and BG are
 * n_bts, bt_buckets, cur_lru_cache_rec_off, cache_lru_cycle.
 */

typedef struct sgmnt_data_struct
{
	unsigned char	label[GDS_LABEL_SZ];
	int4		n_bts;		/* number of cache record/blocks */
	FILL8DCL(sm_off_t, filler_bt_header_off, 1);	/* offset to hash table */
	FILL8DCL(sm_off_t, filler_bt_base_off, 2);	/* bt first entry */
	FILL8DCL(sm_off_t, filler_th_base_off, 3);
	FILL8DCL(sm_off_t, filler_cache_off, 4);
	FILL8DCL(sm_off_t, filler_cur_lru_cache_rec_off, 5);	/* current LRU cache_rec pointer offset */
	enum db_acc_method	acc_meth;	/* Access method (BG or MM). This is static data defined
						 * at file creation.
						 */
	short		start_vbn;	/* starting virtual block number. */
	bool		createinprogress;
	bool		file_corrupt;	/* If this flag is set it shuts the file down.  No process
					 * (except DSE) can successfully map this section after
					 * the flag is set to TRUE.  Processes that already have it
					 * mapped should produce an error the next time that they use
					 * the file.  The flag can only be reset by the DSE utility.
					 */
	int4		total_blks_filler;	/* Marks old total_blks spot, needed for gvcst_init compatablility code */
	file_info	created;	/* Who created this file */
	uint4		lkwkval;	/* Incremented for each lock wake up */
	int4		yield_lmt;	/* maximum number of times a process yields to get optimal jnl writes */
	uint4		lock_space_size;/* Number of bytes to be used for locks (in database for bg) */
	uint4		owner_node;	/* Node on cluster that "owns" the file */
	uint4		free_space;	/* Space in file header not being used */
	uint4 		max_bts;	/* Maximum number of bt records allowed in file */
	uint4		extension_size;	/* Number of gds data blocks to extend by */
	int4		blk_size;	/* Block size for the file. This is static data defined when
					 * the file is created (via MUPIP).  This should correspond to the
					 * process'es gde description of the file. If it doesn't, the number
					 * in the file header should be used.
					 */
	int4		max_rec_size;	/* maximum record size allowed for this file */
	int4		max_key_size;	/* maximum key size allowed for this file */
 	bool            null_subs;
	bool            lock_write;
	bool		ccp_jnl_before;	/* used for clustered to pass if jnl file has before images */
	bool            clustered;
	bool		flush_done;
	bool		unbacked_cache;	/* see mupip_set_file for usage */
	short		bplmap;		/* Blocks per local map (bitmap). This is static data defined when
					 * the file is created (via MUPIP).
					 */
	int4		bt_buckets;	/* Number of buckets in bt table */
	CNTR4DCL(filler_ref_cnt,10);		/* reference count. How many people are using the database */
	CNTR4DCL(n_wrt_per_flu,11);	/* Number of writes per flush call */
					/* overloaded for BG and MM */
	/************* ACCOUNTING INFOMATION ********************************/
	int4		n_retries[CDB_MAX_TRIES];
					/* Counts of the number of retries it took to commit a transaction */
	int4		n_puts;		/* number of puts (non-tp only) */
	int4		n_kills;	/* number of kills */
	int4		n_queries;	/* number of $Query's */
	int4		n_gets;		/* number of MUMPS GETS */
	int4		n_order;	/* number of orders */
	int4		n_zprevs;	/* number of $ZPrevious's */
	int4		n_data;		/* number of datas */
	int4		wc_rtries;	/* write cache read tries */
	int4		wc_rhits;	/* write cache read hits */
	/* Note that the below field was placed here because these were previously used locations that (likely)
	   have a value. For this reason, this value should not be counted upon as a true creation date/time
	   but as a token whose value is somewhat unique amongst multiple generations of the same file. It's
	   only real purpose is to lend uniqueness to the ftok test in dbinit() where our test system has
	   created the same file with the same ftok and other matching inode/etc criteria but it is NOT the
	   same file -- only an extremely similar one whose use of old shared memory created integrity errors. */
	union
	{
		time_t	date_time;	/* When file was created */
		int	filler[2];	/* Filler to make sure above is okay even if takes 2 words on some platform */
	} creation;
	CNTR4DCL(filler_wcs_active_lvl,14);	/* (n_wcrs / 2) - entries in wcq_active  (trips wcs_wtstart) */
	bool		filler_wc_blocked;	/* former location of wc_blocked as bool */
	char		root_level;	/* current level of the root */
	short 		filler_short;	/* filler added to ensure alignment, can be reused */
	int4		flush_time[2];
	trans_num	last_inc_backup;
	trans_num	last_com_backup;
	int4		staleness[2];		/* timer value */
	int4		filler_wc_in_free;		/* number of write cache records in free queue */
	int4		ccp_tick_interval[2];	/* quantum to release write mode if no write occurs and others are queued
						 * These three values are all set at creation by mupip_create
						 */
	int4		flu_outstanding;
	int4		free_blocks_filler;	/* Marks old free_blocks spot, needed for gvcst_init compatablility code */
    	int4		tp_cdb_sc_blkmod[7];	/* notes down the number of times each place got a cdb_sc_blkmod in tp */
	trans_num	last_rec_backup;
	int4		ccp_quantum_interval[2]; /* delta timer for ccp quantum */
	int4		ccp_response_interval[2]; /* delta timer for ccp mailbox response */
	uint4		jnl_alq;
	unsigned short	jnl_deq;
	short		jnl_buffer_size;	/* in pages */
	bool		jnl_before_image;
	unsigned char	jnl_state;		/* Current journaling state */
	bool		filler_glob_sec_init[1];/* glob_sec_init field moved to node_local */
	unsigned char	jnl_file_len;		/* journal file name length */
	unsigned char	jnl_file_name[JNL_NAME_SIZE];	/* journal file name */
	th_index	trans_hist;		/* transaction history - if moved from 2nd fileheader block, change TH_BLOCK */
	int4		cache_lru_cycle;	/* no longer maintained, but field is preserved in case needed in future */
	int4		filler_mm_extender_pid;	/* pid of the process executing gdsfilext in MM mode */
	int4		filler_db_latch;		/* moved - latch for interlocking on tandem */
	int4		reserved_bytes;		/* Database blocks will always leave this many bytes unused */
        CNTR4DCL(filler_in_wtstart,15);		/* Count of processes in wcs_wtstart */
	short		defer_time;		/* defer write ; 0 => immediate, -1 => infinite defer,
						    >0 => defer_time * flush_time[0] is actual defer time
						    DEFAULT value of defer_time = 1 implying a write-timer every
						    csd->flush_time[0] seconds */
	unsigned char	def_coll;		/* Default collation type for new globals */
	unsigned char	def_coll_ver;		/* Default collation type version */
	int4		filler_global_aswp_lock; /* use db_latch now - must be 16 byte aligned in struct for HP aswp locking */
	uint4		image_count;		/* Is used for Data Base Freezing.  */
						/* Set to PROCESS_ID on UNIX and    */
						/* to IMAGE_COUNT on VMS	    */
	uint4		freeze;			/* Set to PROCESS_ID on  VMS and    */
						/* to GETUID on UNIX    in order    */
						/* to "freeze" the Write Cache.     */
	int4		rc_srv_cnt;		/* Count of RC servers accessing database */
	short		dsid;			/* DSID value, non-zero when being accessed by RC */
	short		rc_node;
	uint4		autoswitchlimit;	/* limit in disk blocks (max 4GB) when jnl should be auto switched */
	int4		epoch_interval;		/* Time between successive epochs in epoch-seconds */
	int4		n_tp_retries[7];	/* indexed by t_tries and incremented by 1 for all regions in restarting TP */
	/* The need for having tab_bg_trc_rec_fixed.h and tab_bg_trc_rec_variable.h is because
	 * of now_running and kill_in_prog coming in between the bg_trc_rec fields.
	 * In V5.0, this should be rearranged to have a contiguous space for bg_trc_rec fields
	 */
	/* Include all the bg_trc_rec_fixed accounting fields below */
#define TAB_BG_TRC_REC(A,B)	bg_trc_rec	B;
#include "tab_bg_trc_rec_fixed.h"
#undef TAB_BG_TRC_REC
	char		now_running[MAX_REL_NAME];	 /* for active version stamp */
	int4		kill_in_prog;	/* counter for multi-crit kills that are not done yet */
	/* Note that TAB_BG_TRC_REC and TAB_DB_CSH_ACCT_REC grow in opposite directions */
	/* Include all the bg_trc_rec_variable accounting fields below */
#define TAB_BG_TRC_REC(A,B)	bg_trc_rec	B;
#include "tab_bg_trc_rec_variable.h"
#undef TAB_BG_TRC_REC
	/* Note that when there is an overflow in the sum of the sizes of the bg_trc_rec_variable
	 * types and the db_csh_acct_rec types (due to introduction of new types of accounting
	 * fields), the character array common_filler below will become a negative sized array
	 * which will signal a compiler error (rather than an undecipherable runtime error).
	 */
	char		common_filler[584-sizeof(bg_trc_rec)*n_bg_trc_rec_variable_types
						- sizeof(db_csh_acct_rec)*n_db_csh_acct_rec_types];
	/* Include all the db cache accounting fields below */
#define	TAB_DB_CSH_ACCT_REC(A,B,C)	db_csh_acct_rec	A;
#include "tab_db_csh_acct_rec.h"
#undef TAB_DB_CSH_ACCT_REC
	unsigned char	reorg_restart_key[256];         /* 1st key of a leaf block where reorg was done last time */
	uint4		alignsize;		/* alignment size for JRT_ALIGN */
	block_id	reorg_restart_block;
	/******* following three members (filler_{jnl_file,dbfid}, filler_ino_t) together occupy 64 bytes on all platforms *******/
	/* this area which was previously used for the field "jnl_file" is now moved to node_local */
	union
	{
		gds_file_id	jnl_file_id;  	/* needed on UNIX to hold space */
		unix_file_id	u;		/* from gdsroot.h even for VMS */
	} filler_jnl_file;
	union
	{
		gds_file_id	vmsfid;		/* not used, just hold space */
		unix_file_id	u;		/* For unix ftok error detection */
	} filler_dbfid;
						/* jnl_file and dbfid use ino_t, so place them together */
#ifndef INO_T_LONG
	char		filler_ino_t[8];	/* this filler is not needed for those platforms that have the
						   size of ino_t 8 bytes -- defined in mdefsp.h (Sun only for now) */
#endif
	/*************************************************************************************/

	mutex_spin_parms_struct	mutex_spin_parms;
	int4		mutex_filler1;
	int4		mutex_filler2;
	int4		mutex_filler3;
	int4		mutex_filler4;
	/* semid/shmid/sem_ctime/shm_ctime are UNIX only */
	int4		semid;			/* Since int may not be of fixed size, int4 is used */
	int4		shmid;			/* Since int may not be of fixed size, int4 is used */
	union
	{
		time_t	ctime;		/* For current GTM code sem_ctime field corresponds to creation time */
		int4	filler[2];	/* Filler to make sure above is okay even if takes 2 words on some platform */
	} sem_ctime;
	union
	{
		time_t	ctime;		/* For current GTM code sem_ctime field corresponds to creation time */
		int4	filler[2];	/* Filler to make sure above is okay even if takes 2 words on some platform */
	} shm_ctime;
	boolean_t	recov_interrupted;	/* whether a MUPIP JOURNAL -RECOVER/ROLLBACK command on this db got interrupted */
	int4		intrpt_recov_jnl_state;		/* journaling state at start of interrupted recover/rollback */
	int4		intrpt_recov_repl_state;	/* replication state at start of interrupted recover/rollback */
	jnl_tm_t	intrpt_recov_tp_resolve_time;	/* since-time for the interrupted recover */
	seq_num 	intrpt_recov_resync_seqno;	/* resync/fetchresync jnl_seqno of interrupted rollback */
	uint4		n_puts_duplicate;		/* number of duplicate sets in non-TP */
	uint4		n_tp_updates;		/* number of TP transactions that incremented the db curr_tn for this region */
	uint4		n_tp_updates_duplicate;	/* number of TP transactions that did purely duplicate sets in this region */
	char		filler3[932];
	int4		filler_highest_lbm_blk_changed;	/* Records highest local bit map block that
							   changed so we know how much of master bit
							   map to write out. Modified only under crit */
	char		filler_3k_64[60];	/* get to 64 byte aligned */
	char		filler1_wc_var_lock[16];	/* moved to node_local */
	char		filler_3k_128[48];	/* 3k + 128 - cache line on HPPA */
	char		filler2_db_latch[16];	/* moved to node_local */
	char		filler_3k_192[48];	/* 3k + 192 - cache line on HPPA */
        char            filler_4k[832];         /* Fill out so map sits on 8K boundary */
	char		filler_unique_id[32];
        char            machine_name[MAX_MCNAMELEN];
 	int4		flush_trigger;
	int4		cache_hits;
	int4		max_update_array_size;	/* maximum size of update array needed for one non-TP set/kill */
	int4		max_non_bm_update_array_size;	/* maximum size of update array excepting bitmaps */
	int4		n_tp_retries_conflicts[7];	/* indexed by t_tries and incremented for conflicting region in TP */
	volatile boolean_t	wc_blocked;	/* write cache blocked until recover done due to process being stopped */
	                                        /* in MM mode it is used to call wcs_recover during a file extension */
 	char		filler_rep[176];	/* Leave room for non-replication fields */

	/******* REPLICATION RELATED FIELDS ***********/
	seq_num		reg_seqno;		/* the jnl seqno of the last update to this region -- 8-byte aligned */
	seq_num		resync_seqno;		/* Replication related field. The resync-seqno to be sent to the secondary */
	trans_num	resync_tn;		/* tn for this region
						 * corresponding to
						 * resync_seqno - used in
						 * replication lost
						 * transactions handling */
	uint4		repl_resync_tn_filler;	/* to accommodate 8 byte
						 * resync_tn in the future */
	seq_num		old_resync_seqno;	/* maintained to find out if
						 * transactions were sent
						 * from primary to secondary
						 * - used in replication */
	int4		repl_state;		/* state of replication whether "on" or "off" */
	int4            wait_disk_space;        /* seconds to wait for diskspace before giving up */
	int4		jnl_sync_io;		/* drives sync I/O ('direct' if applicable) for journals, if set */
	char		filler_5k[468];		/* Fill out so map sits on 8K boundary */
   	/******* SECSHR_DB_CLNUP RELATED FIELDS ***********/
   	int4		secshr_ops_index;
   	int4		secshr_ops_array[255];	/* taking up 1K */
	char		filler_7k[1024];		/* Fill out so map sits on 8K boundary */
	char		filler_8k[1024];	/* Fill out so map sits on 8K boundary */
	unsigned char   master_map[MASTER_MAP_SIZE];	/* This map must be aligned on a block size boundary */
						/* Master bitmap. Tells whether the local bitmaps have any free blocks or not. */
} sgmnt_data;

typedef struct  backup_buff_struct
{
	int4            size,
			free,
			disk;                   /* disk == free means the buffer is empty */
	off_t           dskaddr;
	global_latch_t  backup_ioinprog_latch;
	char            tempfilename[256];
	int4            backup_errno;
	uint4           backup_pid;
	uint4           backup_image_count;
	trans_num       backup_tn;
	trans_num       inc_backup_tn;
	uint4           failed;
	unsigned char   buff[1];                /* the real buffer */
} backup_buff;

typedef struct backup_blk_struct
{
	int4            rsize;
	block_id        id;
	unsigned char   bptr[1];
} backup_blk;


#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef       backup_buff     *backup_buff_ptr_t;
typedef       sgmnt_data      *sgmnt_data_ptr_t;
typedef       backup_blk      *backup_blk_ptr_t;

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

#define MIN_NAM_LEN	1
#define MAX_NAM_LEN	8
#define MAX_NM_LEN	8
#define MIN_RN_LEN	1
#define MAX_RN_LEN	16
#define MIN_SN_LEN	1
#define MAX_SN_LEN	16
#define MAX_KEY_SZ	255
#define STR_SUB_PREFIX  0x0FF
#define STR_SUB_ESCAPE  0X01
#define SUBSCRIPT_ZERO  0x080
#define SUBSCRIPT_BIAS  0x0BE
#define NEG_MNTSSA_END  0x0FF
#define KEY_DELIMITER   0X00
#define MIN_DB_BLOCKS	10	/* this should be maintained in conjunction with the mimimum allocation in GDEINIT.M */
#define MAX_ZWR_KEY_SZ	ZWR_EXP_RATIO(MAX_KEY_SZ)

#define OFFSET(x,y) ((uchar_ptr_t)x - (uchar_ptr_t)y)

#define FC_READ 0
#define FC_WRITE 1
#define FC_OPEN 2
#define FC_CLOSE 3

typedef struct	file_control_struct
{
	sm_uc_ptr_t	op_buff;
	int		op_len;
	int		op_pos;
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
	struct htab_desc_struct		*tab_ptr;
	gd_id				*id;	/* Need to be converted to be of type gd_id_ptr_t when 64-bit port is done */
	int4				end;
} gd_addr;
typedef gd_addr *(*gd_addr_fn_ptr)();

typedef struct	gd_segment_struct
{
	unsigned short		sname_len;
	unsigned char		sname[MAX_SN_LEN];
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
	unsigned char		rname[MAX_RN_LEN];
	unsigned short		max_key_size;
	uint4			max_rec_size;
	gd_seg_addr		dyn;
	gd_seg_addr		stat;
	bool			open;
	bool			lock_write;
	bool			null_subs;
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
	unsigned char		filler[1];
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
	block_id	change_bmm_block;	/* master map block (if only one) we need to flush */

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
	backup_buff_ptr_t       backup_buffer;
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
	int4		repl_state;		/* state of replication whether "on" or "off" */
	uint4		crit_check_cycle;	/* Used to mark which regions in a transaction legiticamtely have crit */
	int4		backup_in_prog;		/* true if online backup in progress for this region (used in op_tcommit/tp_tend) */
	int4		ref_cnt;		/* count of number of times csa->nl->ref_cnt was incremented by this process */
	int4		fid_index;		/* index for region ordering based on unique_id */
	boolean_t	do_fullblockwrites;	/* This region enabled for full block writes */
	size_t		fullblockwrite_len;	/* Length of a full block write */
} sgmnt_addrs;


typedef struct	gd_binding_struct
{
	unsigned char	name[MAX_NM_LEN];

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
	mident		gvname;				/* the name of the global */
	trans_num	read_local_tn;			/* local_tn of last reference for this global */
	trans_num	write_local_tn;			/* local_tn of last update for this global */
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
	gv_key		clue;				/* Clue key, must be last in namehead struct because of hung buffer */
} gv_namehead;

#define INVALID_GV_TARGET (gv_namehead *)-1

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

#define COPY_SUBS_TO_GVCURRKEY(var, gv_currkey, was_null, is_null)								\
{																\
	GBLREF mv_stent		*mv_chain;											\
	GBLREF unsigned char	*msp, *stackwarn, *stacktop;									\
	mval			*temp;												\
																\
	error_def(ERR_STACKOFLOW);												\
	error_def(ERR_STACKCRIT);												\
																\
	was_null |= is_null;													\
	val = va_arg(var, mval *);												\
	if (val->mvtype & MV_SUBLIT)												\
	{															\
		is_null = ((STR_SUB_PREFIX == *(unsigned char *)val->str.addr) && (KEY_DELIMITER == *(val->str.addr + 1))); 	\
		if (gv_target->collseq || gv_target->nct)									\
		{														\
			assert(dba_cm != gv_cur_region->dyn.addr->acc_meth); /* collation transformation should be done at the	\
										server's end for CM regions */			\
			transform = FALSE;											\
			end = gvsub2str((uchar_ptr_t)val->str.addr, buff, FALSE);						\
			transform = TRUE;											\
			/* it does not seem like we need the PUSH_MV_STENT and POP_MV_STENT here --- nars - 2003/09/17 */	\
			PUSH_MV_STENT(MVST_MVAL);										\
			temp = &mv_chain->mv_st_cont.mvs_mval;									\
			temp->mvtype = MV_STR;											\
			temp->str.addr = (char *)buff;										\
			temp->str.len = end - buff;										\
			mval2subsc(temp, gv_currkey);										\
			POP_MV_STENT(); /* temp */										\
		} else														\
		{														\
			len = val->str.len;											\
			if (gv_currkey->end + len - 1 >= max_key)								\
			{													\
				if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))			\
					end = &buff[MAX_ZWR_KEY_SZ - 1];							\
				rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buff, buff);			\
			}													\
			memcpy((gv_currkey->base + gv_currkey->end), val->str.addr, len);					\
			gv_currkey->prev = gv_currkey->end;									\
			gv_currkey->end += len - 1;										\
		}														\
	} else															\
	{															\
		mval2subsc(val, gv_currkey);											\
		if (gv_currkey->end >= max_key)											\
		{														\
			if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))				\
				end = &buff[MAX_ZWR_KEY_SZ - 1 ];								\
			rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buff, buff);				\
		}														\
	 	is_null = (MV_IS_STRING(val) && (0 == val->str.len));								\
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
        inctn_secshr_db_clnup,
        inctn_opcode_total
} inctn_opcode_t;

#define HIST_TERMINATOR		0
#define HIST_SIZE(h)		( (sizeof(int4) * 2) + (sizeof(srch_blk_status) * ((h).depth + 1)) )
#define KEY_COPY_SIZE(k)	( sizeof(gv_key) + (k)->end)   /* key and 2 trailing zeroes */

/* Start of lock space in a bg file, therefore also doubles as overhead size for header, bt and wc queues F = # of wc blocks */
#define LOCK_BLOCK(X) (DIVIDE_ROUND_UP(sizeof(sgmnt_data) + BT_SIZE(X), DISK_BLOCK_SIZE))
#define LOCK_BLOCK_SIZE(X) (DIVIDE_ROUND_UP(sizeof(sgmnt_data) + BT_SIZE(X), OS_PAGE_SIZE))
#define	LOCK_SPACE_SIZE(X)	(ROUND_UP(((sgmnt_data_ptr_t)X)->lock_space_size, OS_PAGE_SIZE))
#define CACHE_CONTROL_SIZE(X) 												\
	(ROUND_UP((ROUND_UP((((sgmnt_data_ptr_t)X)->bt_buckets + ((sgmnt_data_ptr_t)X)->n_bts) * sizeof(cache_rec)	\
								+ sizeof(cache_que_heads), DISK_BLOCK_SIZE)		\
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
	char		instname[MAX_FN_LEN + 1];	/* Identify which instance this shared pool corresponds to */
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

#define INCR_KIP(CSD, CSA, KIP_FLAG)				\
{								\
	assert(!KIP_FLAG);					\
	INCR_CNT(&CSD->kill_in_prog, &CSA->nl->wc_var_lock);	\
	KIP_FLAG = TRUE;					\
}
#define DECR_KIP(CSD, CSA, KIP_FLAG)				\
{								\
	assert(KIP_FLAG);					\
	KIP_FLAG = FALSE;					\
	DECR_CNT(&CSD->kill_in_prog, &CSA->nl->wc_var_lock);	\
}
#define INVALID_SEMID -1
#define INVALID_SHMID -1

#if defined(UNIX)
#define DB_FSYNC(reg, udi, csa, db_fsync_in_prog)						\
{												\
	error_def(ERR_DBFSYNCERR);								\
	BG_TRACE_PRO_ANY(csa, n_db_fsyncs);							\
	if (csa->now_crit)									\
		BG_TRACE_PRO_ANY(csa, n_db_fsyncs_in_crit);					\
	db_fsync_in_prog++;									\
	if (-1 == fsync(udi->fd))								\
	{											\
		db_fsync_in_prog--;								\
		rts_error(VARLSTCNT(5) ERR_DBFSYNCERR, 2, DB_LEN_STR(reg), errno);		\
	}											\
	db_fsync_in_prog--;									\
	assert(0 <= db_fsync_in_prog);								\
}

#define STANDALONE(x) mu_rndwn_file(x, TRUE)
#define DBFILOP_FAIL_MSG(status, msg)	gtm_putmsg(VARLSTCNT(5) msg, 2, DB_LEN_STR(gv_cur_region), status);
#elif defined(VMS)
#define STANDALONE(x) mu_rndwn_file(TRUE)	/* gv_cur_region needs to be equal to "x" */
#define DBFILOP_FAIL_MSG(status, msg)	gtm_putmsg(VARLSTCNT(7) msg, 2, DB_LEN_STR(gv_cur_region), status, \
						FILE_INFO(gv_cur_region)->fab->fab$l_stv);
#else
#error unsupported platform
#endif

#define CR_NOT_ALIGNED(cr, cr_base)		(!IS_PTR_ALIGNED((cr), (cr_base), sizeof(cache_rec)))
#define CR_NOT_IN_RANGE(cr, cr_lo, cr_hi)	(!IS_PTR_IN_RANGE((cr), (cr_lo), (cr_hi)))

bt_rec_ptr_t bt_put(gd_region *r, int4 block);
void bt_que_refresh(gd_region *greg);
void bt_init(sgmnt_addrs *cs);
void db_common_init(gd_region *reg, sgmnt_addrs *csa, sgmnt_data_ptr_t csd);
void bt_malloc(sgmnt_addrs *csa);
void bt_refresh(sgmnt_addrs *csa);

boolean_t region_init(bool cm_regions);
bool region_freeze(gd_region *region, bool freeze, bool override);
void grab_crit(gd_region *reg);
void grab_lock(gd_region *reg);
void rel_crit(gd_region *reg);
void rel_lock(gd_region *reg);
void gv_init_reg(gd_region *reg);
void gvcst_init(gd_region *greg);
void assert_jrec_member_offsets(void);
boolean_t mupfndfil(gd_region *reg, mstr *mstr_addr);
enum cdb_sc rel_read_crit(gd_region *reg, short crash_ct);

bool wcs_verify(gd_region *reg, boolean_t expect_damage, boolean_t caller_is_wcs_recover);
bool wcs_wtfini(gd_region *reg);
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

int4 dsk_read(block_id blk, sm_uc_ptr_t buff);

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
gv_namehead *tp_get_target(sm_uc_ptr_t buffaddr);

#include "gdsfheadsp.h"

/* End of gdsfhead.h */

#endif
