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

/* gdsfhead.h */
/* this requires gdsroot.h gtm_facility.h fileinfo.h gdsbt.h */

#include <sys/types.h>

typedef struct
{
	int4	fl;		/* forward link - relative offset from beginning of this element to next element in queue */
	int4	bl;		/* backward link - relative offset from beginning of this element to previous element in queue */
} que_ent;			/* this structure is intended to be identical to the first two items in a cache_que_head */

typedef struct
{
	int4	fl;		/* forward link - relative offset from beginning of this element to next element in queue */
	int4	bl;		/* backward link - relative offset from beginning of this element to previous element in queue */
	int4	latch;		/* required for platforms without atomic operations to modify both fl and bl concurrently;
				 * unused on platforms with such instructions. */
	int4	filler;
} que_head, cache_que_head, mmblk_que_head;

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
		int4	latch;		/* int required for atomic swap on Unix */
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
		int4	latch;		/* int required for atomic swap on Unix */
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
		int4	latch;		/* int required for atomic swap on Unix */
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
	short		iosb[4];	/* used on VMS write */
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
		int4	latch;		/* int required for atomic swap on Unix */
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
	short		iosb[4];	/* used on VMS write */
	bool		in_tend;	/* TRUE from bg_update indicates secshr_db_clnup should finish update */
	bool		data_invalid;	/* TRUE from bg_update indicates t_commit_cleanup and wcs_recover should invalidate */
	bool		stopped;	/* TRUE indicates to wcs_recover that secshr_db_clnup built the block */
	bool		wip_stopped;	/* TRUE indicates to wcs_recover, wcs_wtfini, wcs_get_blk and gds_rundown
  					 * that secshr_db_clnup cancelled the qio*/
	bool		in_cw_set;	/* TRUE from t_end, tp_tend or bg_update protects block from db_csh_getn; returned to
  					 * FALSE by t_end, tp_tend or t_commit_cleanup */
	bool		second_filler_bool[3];	/* preserve double word alignment */
} cache_state_rec;


#define       CR_BLKEMPTY             -1
#define       FROZEN_BY_ROOT          0xFFFFFFFF
#define       BACKUP_NOT_IN_PROGRESS  0x7FFFFFFF
#define       BACKUP_BUFFER_SIZE      0x000FFFFF      /* temporary value 1M */
#define       BACKUP_MAX_FLUSH_TRY    10
#define       UNIQUE_ID_SIZE          32


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

typedef que_ent		*que_ent_ptr_t;
typedef que_head 	*que_head_ptr_t;

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

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

/* the file header has relative pointers to its data structures so each
 * process will malloc one of these and fill it in with absolute pointers
 * upon file initialization.
 */

/* there is a similar macro TP_GDS_REL2ABS in tp_tend.c */
/* any change to the following macros should be reflected in that too */

#define GDS_REL2ABS(x)	(((sm_uc_ptr_t)cs_addrs->lock_addrs[0] + (sm_off_t)(x)))
#define GDS_ABS2REL(x) (sm_off_t)(((sm_uc_ptr_t)(x) - (sm_uc_ptr_t)cs_addrs->lock_addrs[0]))
#define GDS_ANY_REL2ABS(w,x) (((sm_uc_ptr_t)(w->lock_addrs[0]) + (sm_off_t)(x)))
#define GDS_ANY_ABS2REL(w,x) (sm_off_t)(((sm_uc_ptr_t)(x) - (sm_uc_ptr_t)w->lock_addrs[0]))


#define GLO_PREFIX_LEN 4
#define GDS_LABEL_SZ 12
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

#define MIN_FN_LEN	1
#define MAX_FN_LEN	255
#define MAX_REL_NAME	36
#define MAX_MCNAMELEN   256

#define JNL_NAME_SIZE	        256        /* possibly expanded when opened */
#define JNL_NAME_EXP_SIZE	1024       /* MAXPATHLEN, before jnl_buffer in shared memory */

#define BLKS_PER_LMAP	512
#define MAXTOTALBLKS	MASTER_MAP_SIZE * 8 * BLKS_PER_LMAP

#define BUF_OWNER_STUCK 620
#define BITS_PER_UCHAR	8
#define MAXGETSPACEWAIT (60 * 200) + 20		/* seconds/min. * 5-ms-intervals/sec. */

#define	STEP_FACTOR			64		/* the factor by which flush_trigger is incremented/decremented */
#define	MIN_FLUSH_TRIGGER(n_bts)	((n_bts)/4)	/* the minimum flush_trigger as a function of n_bts */
#define	MAX_FLUSH_TRIGGER(n_bts)	((n_bts)*15/16)	/* the maximum flush_trigger as a function of n_bts */

#define ENSURE_JNL_OPEN(csa, reg)							\
{											\
	assert(cs_addrs == csa);							\
	assert(gv_cur_region == reg);							\
	assert(FALSE == reg->read_only);						\
	if (JNL_ENABLED(csa->hdr) && NULL != csa->jnl && NOJNL == csa->jnl->channel)	\
        {										\
		bool was_crit;								\
		was_crit = csa->now_crit;						\
		if (!was_crit)								\
        		grab_crit(reg);							\
   		jnl_ensure_open();							\
		if (!was_crit)								\
       			rel_crit(reg);							\
	}										\
}

#ifdef VMS
#define	JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, num_bufs)		\
{								\
	ENSURE_JNL_OPEN(csa, reg);				\
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
#define	JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, num_bufs)	\
{							\
	ENSURE_JNL_OPEN(csa, reg);			\
	wcs_wtstart(reg, num_bufs);			\
}
#else
#error UNSUPPORTED PLATFORM
#endif

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

/* This is the structure describing a segment. It is used as a database file
 * header (for MM or BG access methods). The overloaded fields for MM and BG are
 * n_bts, bt_buckets, cur_lru_cache_rec_off, cache_lru_cycle.
 */

typedef struct sgmnt_data_struct
{
	unsigned char	label[GDS_LABEL_SZ];
	int4		n_bts;		/* number of cache record/blocks */
	FILL8DCL(sm_off_t, bt_header_off, 1);	/* offset to hash table */
	FILL8DCL(sm_off_t, bt_base_off, 2);	/* bt first entry */
	FILL8DCL(sm_off_t, th_base_off, 3);
	FILL8DCL(sm_off_t, cache_off, 4);
	FILL8DCL(sm_off_t, cur_lru_cache_rec_off, 5);	/* current LRU cache_rec pointer offset */
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
	int4		filler_wc_var_lock;	/* moved - lock used for access to various wc_* ref counters */
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
	CNTR4DCL(ref_cnt,10);		/* reference count. How many people are using the database */
	CNTR4DCL(n_wrt_per_flu,11);	/* Number of writes per flush call */
					/* overloaded for BG and MM */
	/************* ACCOUNTING INFOMATION ********************************/
	int4		n_retries[CDB_MAX_TRIES];
					/* Counts of the number of retries it took to commit a transaction */
	int4		n_puts;		/* number of puts */
	int4		n_kills;	/* number of kills */
	int4		n_queries;	/* number of $Query's */
	int4		n_gets;		/* number of MUMPS GETS */
	int4		n_order;	/* number of orders */
	int4		n_zprevs;	/* number of $ZPrevious's */
	int4		n_data;		/* number of datas */
	int4		wc_rtries;	/* write cache read tries */
	int4		wc_rhits;	/* write cache read hits */
	CNTR4DCL(wcs_staleness,12);	/* disk version of file is stale */
	CNTR4DCL(wcs_timers,13);	/* number of write cache timers in use - 1 */
	CNTR4DCL(wcs_active_lvl,14);	/* (n_wcrs / 2) - entries in wcq_active  (trips wcs_wtstart) */
	bool		wc_blocked;	/* write cache blocked until recover done due to process being stopped */
					/* in MM mode it is used to call wcs_recover during a file extension */
	char		root_level;	/* current level of the root */
	short 		filler_short;	/* filler added to ensure alignment, can be reused */
	int4		flush_time[2];
	trans_num	last_inc_backup;
	trans_num	last_com_backup;
	int4		staleness[2];		/* timer value */
	int4		wc_in_free;		/* number of write cache records in free queue */
	int4		ccp_tick_interval[2];	/* quantum to release write mode if no write occurs and others are queued
						 * These three values are all set at creation by mupip_create
						 */
	int4		flu_outstanding;
	int4		free_blocks_filler;	/* Marks old free_blocks spot, needed for gvcst_init compatablility code */
	char            filler_jnl_file[28];    /* moved jnl_file to an empty no-problem space,
                                                   since size of this union is different on Sun
                                                   and non Sun platforms */
	trans_num	last_rec_backup;
	int4		ccp_quantum_interval[2]; /* delta timer for ccp quantum */
	int4		ccp_response_interval[2]; /* delta timer for ccp mailbox response */
	uint4		jnl_alq;
	unsigned short	jnl_deq;
	short		jnl_buffer_size;	/* in pages */
	bool		jnl_before_image;
	unsigned char	jnl_state;		/* Current journalling state */
	bool		filler_glob_sec_init[1];/* glob_sec_init field moved to node_local */
	unsigned char	jnl_file_len;		/* journal file name length */
	unsigned char	jnl_file_name[JNL_NAME_SIZE];	/* journal file name */
	th_index	trans_hist;		/* transaction history - if moved from 2nd fileheader block, change TH_BLOCK */
	int4		cache_lru_cycle;
	int4		mm_extender_pid;	/* pid of the process executing gdsfilext in MM mode */
	int4		filler_db_latch;		/* moved - latch for interlocking on tandem */
	int4		reserved_bytes;		/* Database blocks will always leave this many bytes unused */
        CNTR4DCL(in_wtstart,15);		/* Count of processes in wcs_wtstart */
	short		defer_time;		/* defer write ; 0 => immediate, -1 => infinite defer,
						    >0 => defer_time * flush_time[0] is actual defer time
						    DEFAULT value of defer_time = 1 implying a write-timer every
						    csd->flush_time[0] seconds */
	unsigned char	def_coll;		/* Default collation type for new globals */
	unsigned char	def_coll_ver;		/* Default collation type version */
	int4		filler_global_aswp_lock; 	/* use db_latch now - must be 16 byte aligned in struct for HP aswp locking */
	uint4		image_count;		/* Is used for Data Base Freezing.  */
						/* Set to PROCESS_ID on UNIX and    */
						/* to IMAGE_COUNT on VMS	    */
	uint4		freeze;			/* Set to PROCESS_ID on  VMS and    */
						/* to GETUID on UNIX    in order    */
						/* to "freeze" the Write Cache.     */
	int4		rc_srv_cnt;		/* Count of RC servers accessing database */
	short		dsid;			/* DSID value, non-zero when being accessed by RC */
	short		rc_node;
	key_t		ftok;			/* Ftok of this file -- saved by db_init */
	int4		nbb;			/* Next backup block -- for online backup */
	char            filler_dbfid[28];       /* moved dbfid to an empty no-problem space,
                                                   since size of this union is different on Sun
                                                   and non Sun platforms */
#if defined(VMS)
	/* VMS specific trace info */
	bg_trc_rec	rmv_free;		/* space to record bg charcteristics - only maintained if DEBUG */
	bg_trc_rec	rmv_clean;
	bg_trc_rec	clean_to_mod;
	bg_trc_rec	qio_to_mod;
	bg_trc_rec	blocked;
	bg_trc_rec	blkd_made_empty;
	bg_trc_rec	obsolete_to_empty;
	bg_trc_rec	qio_to_clean;
	bg_trc_rec	stale;
	bg_trc_rec	starved;
	bg_trc_rec	active_lvl_trigger;
	bg_trc_rec	new_buff;
	bg_trc_rec	get_new_buff;
	bg_trc_rec	mod_to_mod;
#elif defined(UNIX)
	/* Unix specific trace info */
	bg_trc_rec	total_buffer_flush;	/* Count of wcs_flu calls */
	bg_trc_rec	bufct_buffer_flush;	/* Count of flushing-till-buffers-free-cnt (wcs_get_space) */
	bg_trc_rec	bufct_buffer_flush_loop;/* Count of flushing-till-buffers-free-cnt looping back (wcs_get_space) */
	bg_trc_rec	stale_timer_started;	/* Stale buffer timer started */
	bg_trc_rec	stale_timer_pop;	/* Stale timer has popped */
	bg_trc_rec	stale_process_defer;	/* Deferring processing due to conditions */
	bg_trc_rec	stale_defer_processed;	/* Stale processing done outside crit */
	bg_trc_rec	wrt_calls;		/* Calls to wcs_wtstart */
	bg_trc_rec	wrt_count;		/* Count of writes done in wcs_wtstart */
	bg_trc_rec	wrt_blocked;		/* wc_blocked was on in wcs_wtstart */
	bg_trc_rec	wrt_busy;		/* Encountered wcs_wtstart lock */
	bg_trc_rec	wrt_noblks_wrtn;	/* Times wcs_wtstart ran queues but nothing written */
	bg_trc_rec	reserved_bgtrcrec;	/* Reserved filler to match length of VMS section */
 	bg_trc_rec	lost_block_recovery;	/* Performing lost block recovery in gds_rundown (traced PRO also) */
#else
# error Unsupported platform
#endif
	bg_trc_rec	spcfc_buffer_flush;	/* Count of flushing specific buffer (wcs_get_space) */
 	bg_trc_rec	spcfc_buffer_flush_loop;/* Passes through the active queue made to flush a specific buffer (wcs_get_space) */
 	bg_trc_rec	spcfc_buffer_flush_retries; /* Times we re-flushed when 1st flush didn't flush buffer */
 	bg_trc_rec	spcfc_buffer_flushed_during_lockwait;

	bg_trc_rec	tp_crit_retries;	/* Number of times we re-tried getting crit (common Unix & VMS) */
	bg_trc_rec	db_csh_getn_flush_dirty;	/* all the fields from now on use the BG_TRACE_PRO macro   */
	bg_trc_rec	db_csh_getn_rip_wait;		/* since they are incremented rarely, they will go in	   */
	bg_trc_rec	db_csh_getn_buf_owner_stuck;	/* production code too. The BG_TRACE_PRO macro does a      */
	bg_trc_rec	db_csh_getn_out_of_design; 	/* NON-INTERLOCKED increment.				   */
	bg_trc_rec	t_qread_buf_owner_stuck;
	bg_trc_rec	t_qread_out_of_design;
	bg_trc_rec	bt_put_flush_dirty;
        bg_trc_rec	mlock_wakeups;		/* Times a process has slept on a lock in this region and been awakened */
	bg_trc_rec	wc_blocked_wcs_verify_passed;
	bg_trc_rec	wc_blocked_t_qread_db_csh_getn_invalid_blk;
	bg_trc_rec	wc_blocked_t_qread_db_csh_get_invalid_blk;
	bg_trc_rec	wc_blocked_db_csh_getn_loopexceed;
	bg_trc_rec	wc_blocked_db_csh_getn_wcsstarvewrt;
	bg_trc_rec	wc_blocked_db_csh_get;
	bg_trc_rec	wc_blocked_tp_tend_wcsgetspace;
	bg_trc_rec	wc_blocked_tp_tend_t1;
	bg_trc_rec	wc_blocked_tp_tend_bitmap;
	bg_trc_rec	wc_blocked_tp_tend_jnl_cwset;
	bg_trc_rec	wc_blocked_tp_tend_jnl_wcsflu;
	bg_trc_rec	wc_blocked_t_end_hist;
	bg_trc_rec	wc_blocked_t_end_hist1_nullbt;
	bg_trc_rec	wc_blocked_t_end_hist1_nonnullbt;
	bg_trc_rec	wc_blocked_t_end_bitmap_nullbt;
	bg_trc_rec	wc_blocked_t_end_bitmap_nonnullbt;
	bg_trc_rec	wc_blocked_t_end_jnl_cwset;
	bg_trc_rec	wc_blocked_t_end_jnl_wcsflu;
	bg_trc_rec	db_csh_get_too_many_loops;
	char		now_running[MAX_REL_NAME];	 /* for active version stamp */
	int4		kill_in_prog;	/* counter for multi-crit kills that are not done yet */
	bg_trc_rec	wc_blocked_tpckh_hist1_nullbt;
	bg_trc_rec	wc_blocked_tpckh_hist1_nonnullbt;
	bg_trc_rec  	jnl_blocked_writer_lost;
	bg_trc_rec  	jnl_blocked_writer_stuck;
	bg_trc_rec  	jnl_blocked_writer_blocked;
#if defined(VMS)
	bg_trc_rec      jnl_fsync_filler[3];
#elif defined(UNIX)
	bg_trc_rec      jnl_fsync;              /* count of jnl fsync on to disk */
	bg_trc_rec      jnl_fsync_tries;        /* attempted jnl fsyncs */
	bg_trc_rec      jnl_fsync_recover;      /* how many fsync recovers were done */
#endif
	char            filler_2k[784];
	/******* following three members (jnl_file, dbfid, filler_ino_t) together occupy 64 bytes on all platforms *******/
        union
        {
                gds_file_id     jnl_file_id;    /* needed on UNIX to hold space */
                unix_file_id    u;              /* from gdsroot.h even for VMS */
        } jnl_file;
        union
        {
                gds_file_id     vmsfid;         /* not used, just hold space */
                unix_file_id    u;              /* For unix ftok error detection */
        } dbfid;
                                                /* jnl_file and dbfid use ino_t, so place them together */
#ifndef INO_T_LONG
        char            filler_ino_t[8];        /* this filler is not needed for those platforms that have the
                                                   size of ino_t 8 bytes -- defined in mdefsp.h (Sun only for now) */
#endif
        /*************************************************************************************/

	mutex_spin_parms_struct	mutex_spin_parms; /* Unix only */
	int4		mutex_filler1;		/* Unix only */
	int4		mutex_filler2;		/* Unix only */
	int4		mutex_filler3;		/* Unix only */
	int4		mutex_filler4;		/* Unix only */
	char		filler3[992];
	int4		highest_lbm_blk_changed;	/* Records highest local bit map block that
							   changed so we know how much of master bit
							   map to write out. Modified only under crit */
	char		filler_3k_64[60];	/* get to 64 byte aligned */
	global_latch_t	wc_var_lock;		/* lock used for access to various wc_* ref counters */
	char		filler_3k_128[48];	/* 3k + 128 - cache line on HPPA */
	global_latch_t	db_latch;		/*  latch for interlocking on hppa and tandem */
	char		filler_3k_192[48];	/* 3k + 192 - cache line on HPPA */
        char            filler_4k[832];         /* Fill out so map sits on 8K boundary */
	char		unique_id[UNIQUE_ID_SIZE];
        char            machine_name[MAX_MCNAMELEN];
 	int4		flush_trigger;
	int4		cache_hits;
	int4		max_update_array_size;	/* maximum size of update array needed for one non-TP set/kill */
	int4		max_non_bm_update_array_size;	/* maximum size of update array excepting bitmaps */
 	char		filler_rep[208];	/* Leave room for non-replication fields */
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
	char		filler_5k[476];	/* Fill out so map sits on 8K boundary */
   	/******* SECSHR_DB_CLNUP RELATED FIELDS ***********/
   	int4		secshr_ops_index;
   	int4		secshr_ops_array[255];	/* taking up 1K */
	char		filler_7k[1024];	/* Fill out so map sits on 8K boundary */
	char		filler_8k[1024];	/* Fill out so map sits on 8K boundary */
	unsigned char   master_map[MASTER_MAP_SIZE];	/* This map must be aligned on a block size boundary */
						/* Master bitmap. Tells whether the local bitmaps have any free blocks or not. */
} sgmnt_data;

typedef struct  backup_buff_struct
{
	int4            size,
			free,
			disk;                   /* disk == free means the buffer is empty */
	uint4           dskaddr;
	int             io_in_progress;		/* protected by bb_latch */
	char            tempfilename[256];
	int4            backup_errno;
	uint4           backup_pid;
	uint4           backup_image_count;
	trans_num       backup_tn;
	trans_num       inc_backup_tn;
	uint4           failed;
	uint4		flusher;
	global_latch_t	bb_latch;		/* for access to io_in_progress */
	CACHELINE_PAD(sizeof(global_latch_t), 1)    /* ; supplied by macro */
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
#define MIN_DB_BLOCKS	10	/* this should be maintained in conjunction with the mimimum allocation in GDEINIT.M */

#define OFFSET(x,y) ((uchar_ptr_t)x - (uchar_ptr_t)y)

#define FC_READ 0
#define FC_WRITE 1
#define FC_OPEN 2

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
	struct gd_id_struct		*id;

	/* removed pointers to OS specific structs */

	int4				end;
} gd_addr;

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
	short			jnl_deq;
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

	int4		filler;			/* For alignment */
	volatile int4	mutex_state;		/* Mutex state of this process */

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

typedef struct
{
	cache_rec_ptr_t	cr;
        sm_uc_ptr_t     buffaddr;
	block_id	blk_num;
	trans_num	tn;
	srch_rec_status	prev_rec,
			curr_rec;
	int4		cycle;
	void		*ptr;	/* actually ptr to the cw_set_element, but cannot declare it here due to header file ordering */
} srch_blk_status;

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

/* This structure is referenced in gvcst_search.mar, any changes to it must
	be reflected there in the defined offsets */

typedef struct	gv_namehead_struct
{
	gd_region	*gd_reg;			/* Region of key */
	gv_key		*first_rec, *last_rec;		/* Boundary recs of clue's data block */
	struct gv_namehead_struct *next_gvnh;		/* Used to chain gv_target's together */
	trans_num	tn_used;			/* Last tn to use this structure */
	block_id	root;				/* Root of global variable tree */
	srch_hist	hist;				/* block history array */
	unsigned char	nct;				/* numerical collation type for internalization */
	unsigned char	act;				/* alternative collation type for internalization */
	unsigned char	ver;
	char		filler[1];
	struct collseq_struct	*collseq;		/* pointer to a linked list of user supplied routine addresses
			 				   for internationalization */
	gv_key		clue;				/* Clue key, must be last in namehead struct because of hung buffer */
} gv_namehead;

#define HIST_TERMINATOR		0
#define HIST_SIZE(h)		( (sizeof(int4) * 2) + (sizeof(srch_blk_status) * ((h).depth + 1)) )
#define KEY_COPY_SIZE(k)	( sizeof(gv_key) + (k)->end)   /* key and 2 trailing zeroes */

/* Start of lock space in a bg file, therefore also doubles as overhead size for header, bt and wc queues F = # of wc blocks */
#define LOCK_BLOCK(X) (DIVIDE_ROUND_UP(sizeof(sgmnt_data) + BT_SIZE(X), DISK_BLOCK_SIZE))
#define LOCK_SPACE_SIZE(X) (((sgmnt_data_ptr_t)X)->lock_space_size)
#define CACHE_CONTROL_SIZE(X) (ROUND_UP((((sgmnt_data_ptr_t)X)->bt_buckets + ((sgmnt_data_ptr_t)X)->n_bts) * sizeof(cache_rec) \
	+ sizeof(cache_que_heads), DISK_BLOCK_SIZE) + (((sgmnt_data_ptr_t)X)->n_bts * ((sgmnt_data_ptr_t)X)->blk_size))

#define MMBLK_CONTROL_SIZE(X) (ROUND_UP((((sgmnt_data_ptr_t)X)->bt_buckets + ((sgmnt_data_ptr_t)X)->n_bts) * sizeof(mmblk_rec) \
	+ sizeof(mmblk_que_heads), DISK_BLOCK_SIZE))
#define TIME_TO_FLUSH           10                      /* milliseconds */
/* End of gdsfhead.h */
