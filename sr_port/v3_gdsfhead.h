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

/*-----------------------------------------------------------------------*
	v3_gdsfhead.h: This is made from V32016C UNIX gdsfhead.h
                       this requires gdsfhead.h, gdsroot.h
		       gtm_facility.h fileinfo.h gdsbt.h
 *-----------------------------------------------------------------------*/

#define V3_MASTER_MAP_SIZE     (2 * DISK_BLOCK_SIZE)   /* MUST be a multiple of DISK_BLOCK_SIZE */

typedef struct
{
	trans_num       curr_tn;
	trans_num       early_tn;
	trans_num       header_open_tn;         /* Tn to be compared against jnl tn on open */
	trans_num       mm_tn;                  /* Used to see if CCP must update master map */
	uint4           lock_sequence;          /* Used to see if CCP must update lock section */
	uint4           ccp_jnl_filesize;       /* Passes size of journal file if extended */
	uint4           total_blks;             /* Placed here so can be passed to other machines on cluster */
	uint4           free_blocks;
} v3_th_index;



#if defined(__alpha) && defined(__vms)
# pragma member_alignment save
# pragma nomember_alignment
#endif

typedef struct
{
	short		evnt_cnt;
	trans_num	evnt_tn;
} v3_bg_trc_rec;

typedef struct
{
	short		evnt_cnt;
	short		filler;
	trans_num	evnt_tn;
} new_v3_bg_trc_rec;


/* This is the structure describing a segment. It is used as a database file
 * header (for MM or BG access methods).
 */

typedef struct v3_sgmnt_data_struct
{
	unsigned char	label[GDS_LABEL_SZ];
	int4		n_bts;			/* number of cache record/blocks */
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
	unsigned short	in_wtstart;	/* Number of processes in wcs_wtstart, used for cluster control */
	short		filler_short1;
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
	int4		bt_off;		/* Offset from beginning of file to block table */
	int4 		bt_header_off;	/* offset to hash table */
	int4 		bt_freelist_off;	/* offset to free list header */
	int4 		bt_base_off;	/* bt first entry */
	int4 		th_base_off;
	short		ref_cnt;	/* reference count. How many people are using the database */
	short		n_wrt_per_flu;
	/************* ACCOUNTING INFOMATION ********************************/
	int4		n_retries[CDB_STAGNATE];
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
	short		wcs_staleness;	/* disk version of file is stale */
	short		wcs_timers;	/* number of write cache timers in use - 1 */
	bool		wc_blocked;	/* write cache blocked until recover done due to process being stopped */
	char		root_level;	/* current level of the root */
	int4		flush_time[2];
	short		wcs_active_lvl;	/* (n_wcrs / 2) - entries in wcq_active  (trips wcs_wtstart) */
	v3_bg_trc_rec	rmv_free;	/* space to record bg charcteristics - only maintained if DEBUG */
	v3_bg_trc_rec	rmv_clean;
	v3_bg_trc_rec	clean_to_mod;
	v3_bg_trc_rec	qio_to_mod;
	v3_bg_trc_rec	blocked;
	v3_bg_trc_rec	blkd_made_empty;
	v3_bg_trc_rec	obsolete_to_empty;
	v3_bg_trc_rec	qio_to_clean;
	v3_bg_trc_rec	stale;
	v3_bg_trc_rec	starved;
	v3_bg_trc_rec	active_lvl_trigger;
	trans_num	last_inc_backup;
	trans_num	last_com_backup;
	int4		staleness[2];		/* timer value */
	int4		wc_in_free;		/* number of write cache records in free queue */
	int4		ccp_tick_interval[2];	/* quantum to release write mode if no write occurs and others are queued
						 * These three values are all set at creation by mupip_create
						 */
	int4		flu_outstanding;
	int4		free_blocks_filler;	/* Marks old free_blocks spot, needed for gvcst_init compatablility code */
	union
	{
		gds_file_id	jnl_file_id;
		struct
		{
			int	inode;
			int	device;
		} u;
	} jnl_file;
	trans_num	last_rec_backup;
	v3_bg_trc_rec	new_buff;
	v3_bg_trc_rec	get_new_buff;
	v3_bg_trc_rec	mod_to_mod;
	int4		ccp_quantum_interval[2]; /* delta timer for ccp quantum */
	int4		ccp_response_interval[2]; /* delta timer for ccp mailbox response */
	int4		jnl_alq;
	unsigned short	jnl_deq;
	short		jnl_buffer_size;	/* in pages */
	bool		jnl_before_image;
	unsigned char	jnl_state;		/* Current journalling state */
	bool		glob_sec_init;
	unsigned char	jnl_file_len;		/* journal file name length */
	unsigned char	jnl_file_name[JNL_NAME_SIZE];	/* journal file name */
	int4		cache_off;
	int4		cache_lru_cycle;
	int4		db_latch;		/* latch for interlocking on tandem */
	int4		reserved_bytes;		/* Database blocks will always leave this many bytes unused */
	int4		global_aswp_lock; 	/* must be 16 byte aligned in struct */
	uint4		image_count;		/* Is used for Data Base Freezing.  */
						/* Set to PROCESS_ID on UNIX and    */
						/* to IMAGE_COUNT on VMS	    */
	uint4		freeze;			/* Set to PROCESS_ID on  VMS and    */
						/* to GETUID on UNIX    in order    */
						/* to "freeze" the Write Cache.     */
	int4		rc_srv_cnt;		/* Count of RC servers accessing database */
	short		dsid;			/* DSID value, non-zero when being accessed by RC */
	short		rc_node;
	new_v3_bg_trc_rec	db_csh_getn_flush_dirty;	/* these fields from now on use the BG_TRACE_PRO macro   */
	new_v3_bg_trc_rec	db_csh_getn_rip_wait;		/* since they are incremented rarely, they will go in	   */
	new_v3_bg_trc_rec	db_csh_getn_buf_owner_stuck;	/* production code too. The BG_TRACE_PRO macro does a      */
	new_v3_bg_trc_rec	db_csh_getn_out_of_design; 	/* NON-INTERLOCKED increment.				   */
	new_v3_bg_trc_rec	t_qread_buf_owner_stuck;
	new_v3_bg_trc_rec	t_qread_out_of_design;
	new_v3_bg_trc_rec	bt_put_flush_dirty;
	new_v3_bg_trc_rec	wc_blocked_wcs_verify_passed;
	new_v3_bg_trc_rec	wc_blocked_t_qread_db_csh_getn_invalid_blk;
	new_v3_bg_trc_rec	wc_blocked_t_qread_db_csh_get_invalid_blk;
	new_v3_bg_trc_rec	wc_blocked_db_csh_getn_loopexceed;
	new_v3_bg_trc_rec	wc_blocked_db_csh_getn_wcsstarvewrt;
	new_v3_bg_trc_rec	wc_blocked_db_csh_get;
	new_v3_bg_trc_rec	wc_blocked_tp_tend_wcsgetspace;
	new_v3_bg_trc_rec	wc_blocked_tp_tend_t1;
	new_v3_bg_trc_rec	wc_blocked_tp_tend_bitmap;
	new_v3_bg_trc_rec	wc_blocked_tp_tend_jnl_cwset;
	new_v3_bg_trc_rec	wc_blocked_tp_tend_jnl_wcsflu;
	new_v3_bg_trc_rec	wc_blocked_t_end_hist2;
	new_v3_bg_trc_rec	wc_blocked_t_end_hist1_nullbt;
	new_v3_bg_trc_rec	wc_blocked_t_end_hist1_nonnullbt;
	new_v3_bg_trc_rec	wc_blocked_t_end_bitmap_nullbt;
	new_v3_bg_trc_rec	wc_blocked_t_end_bitmap_nonnullbt;
	new_v3_bg_trc_rec	wc_blocked_t_end_jnl_cwset;
	new_v3_bg_trc_rec	wc_blocked_t_end_jnl_wcsflu;
	int4		cur_lru_cache_rec_off;	/* current LRU cache_rec pointer offset */
	int4		filler_quad_word;
	char		filler2[86];
	char		now_running[MAX_REL_NAME];	 /* for active version stamp */
	unsigned char	def_coll;		/* Default collation type for new globals */
	unsigned char	def_coll_ver;		/* Default collation type version */
	unsigned char   master_map[V3_MASTER_MAP_SIZE];	/* MUST BE AT MM_BLOCK - 1 * DISK_BLOCK_SIZE !!! */
						/* Master bitmap. Tells whether the local bitmaps have any free blocks or not. */
	v3_th_index	trans_hist;		/* transaction history */
} v3_sgmnt_data;

#if defined(__alpha) && defined(__vms)
# pragma member_alignment restore
#endif

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef v3_sgmnt_data *v3_sgmnt_data_ptr_t;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

