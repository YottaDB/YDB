/****************************************************************
 *								*
 *	Copyright 2005, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __V15_GDSFHEAD_H__
#define __V15_GDSFHEAD_H__

/* gdsfhead.h as of journal format 15 (last GT.M V4 format)  */
/* this requires gdsroot.h gtm_facility.h fileinfo.h gdsbt.h */

typedef struct	v15_gd_segment_struct
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
	struct v15_gd_region_struct	*repl_list;
} v15_gd_segment;

typedef union
{
	int4			offset;  /* relative offset to segment  */
        v15_gd_segment		*addr;   /* absolute address of segment */
} v15_gd_seg_addr;

typedef struct	v15_gd_region_struct
{
	unsigned short		rname_len;
	unsigned char		rname[MAX_RN_LEN];
	unsigned short		max_key_size;
	uint4			max_rec_size;
	v15_gd_seg_addr		dyn;
	v15_gd_seg_addr		stat;
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
} v15_gd_region;

typedef struct
{
	CNTR4DCL(evnt_cnt,100);
	v15_trans_num	evnt_tn;
} v15_bg_trc_rec;

typedef struct
{
	int4	curr_count;	/* count for this invocation of shared memory */
	int4	cumul_count;	/* count from the creation of database (not including this invocation) */
} v15_db_csh_acct_rec;

#define	TAB_DB_CSH_ACCT_REC(A,B,C)	v15_##A,
enum v15_db_csh_acct_rec_type
{
#include "v15_tab_db_csh_acct_rec.h"
v15_n_db_csh_acct_rec_types
};
#undef TAB_DB_CSH_ACCT_REC

#define TAB_BG_TRC_REC(A,B)		v15_##B,
enum v15_bg_trc_rec_fixed_type
{
#include "v15_tab_bg_trc_rec_fixed.h"
v15_n_bg_trc_rec_fixed_types
};
enum v15_bg_trc_rec_variable_type
{
#include "v15_tab_bg_trc_rec_variable.h"
v15_n_bg_trc_rec_variable_types
};
#undef TAB_BG_TRC_REC

/* This is the structure describing a segment. It is used as a database file
 * header (for MM or BG access methods). The overloaded fields for MM and BG are
 * n_bts, bt_buckets, cur_lru_cache_rec_off, cache_lru_cycle.
 */

typedef struct v15_sgmnt_data_struct
{
	unsigned char	label[GDS_LABEL_SZ];
	int4		n_bts;		/* number of cache record/blocks */
	int 		filler_bt_header_off[2];
	int		filler_bt_base_off[2];
	int		filler_th_base_off[2];
	int		filler_cache_off[2];
	int		filler_cur_lru_cache_rec_off[2];
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
		v15_time_t	date_time;	/* When file was created */
		int	filler[2];	/* Filler to make sure above is okay even if takes 2 words on some platform */
	} creation;
	CNTR4DCL(filler_wcs_active_lvl,14);	/* (n_wcrs / 2) - entries in wcq_active  (trips wcs_wtstart) */
	bool		filler_wc_blocked;	/* former location of wc_blocked as bool */
	char		root_level;	/* current level of the root */
	short 		filler_short;	/* filler added to ensure alignment, can be reused */
	int4		flush_time[2];
	v15_trans_num	last_inc_backup;
	v15_trans_num	last_com_backup;
	int4		staleness[2];		/* timer value */
	int4		filler_wc_in_free;		/* number of write cache records in free queue */
	int4		ccp_tick_interval[2];	/* quantum to release write mode if no write occurs and others are queued
						 * These three values are all set at creation by mupip_create
						 */
	int4		flu_outstanding;
	int4		free_blocks_filler;	/* Marks old free_blocks spot, needed for gvcst_init compatablility code */
    	int4		tp_cdb_sc_blkmod[7];	/* notes down the number of times each place got a cdb_sc_blkmod in tp */
	v15_trans_num	last_rec_backup;
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
	v15_th_index	trans_hist;		/* transaction history - if moved from 2nd fileheader block, change TH_BLOCK */
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
	boolean_t	std_null_coll;		/* 0 -> GT.M null collation,i,e, null subs collate between numeric and string
						 * 1-> standard null collation i.e. null subs collate before numeric and string */
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
#define TAB_BG_TRC_REC(A,B)	v15_bg_trc_rec	fixed_##B;
#include "v15_tab_bg_trc_rec_fixed.h"
#undef TAB_BG_TRC_REC
	char		now_running[MAX_REL_NAME];	 /* for active version stamp */
	int4		kill_in_prog;	/* counter for multi-crit kills that are not done yet */
	/* Note that TAB_BG_TRC_REC and TAB_DB_CSH_ACCT_REC grow in opposite directions */
	/* Include all the bg_trc_rec_variable accounting fields below */
#define TAB_BG_TRC_REC(A,B)	v15_bg_trc_rec	var_##B;
#include "v15_tab_bg_trc_rec_variable.h"
#undef TAB_BG_TRC_REC
	/* Note that when there is an overflow in the sum of the sizes of the bg_trc_rec_variable
	 * types and the db_csh_acct_rec types (due to introduction of new types of accounting
	 * fields), the character array common_filler below will become a negative sized array
	 * which will signal a compiler error (rather than an undecipherable runtime error).
	 */
	char		common_filler[584 - SIZEOF(v15_bg_trc_rec) * v15_n_bg_trc_rec_variable_types
				      - SIZEOF(v15_db_csh_acct_rec) * v15_n_db_csh_acct_rec_types];
	/* Include all the db cache accounting fields below */
#define	TAB_DB_CSH_ACCT_REC(A,B,C)	v15_db_csh_acct_rec	acct_##A;
#include "v15_tab_db_csh_acct_rec.h"
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
	} gt_sem_ctime;
	union
	{
		time_t	ctime;		/* For current GTM code sem_ctime field corresponds to creation time */
		int4	filler[2];	/* Filler to make sure above is okay even if takes 2 words on some platform */
	} gt_shm_ctime;
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
        char            filler_4096[832];	/* Fill out so map sits on 8K boundary */
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
	v15_trans_num	resync_tn;		/* tn for this region
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
	char		filler_5104[452];
	enum db_ver	creation_db_ver;	/* Major DB version at time of creation */
	enum mdb_ver	creation_mdb_ver;	/* Minor DB version at time of creation */
	enum db_ver	certified_for_upgrade_to;	/* Version the database is certified for upgrade to */
	int		filler_5k;		/* Fill out to 5K */
   	/******* SECSHR_DB_CLNUP RELATED FIELDS ***********/
   	int4		secshr_ops_index;
   	int4		secshr_ops_array[255];	/* taking up 1K */
	char		filler_7k[1024];		/* Fill out so map sits on 8K boundary */
	char		filler_8k[1024];	/* Fill out so map sits on 8K boundary */
	unsigned char   master_map[MASTER_MAP_SIZE_V4];	/* This map must be aligned on a block size boundary */
						/* Master bitmap. Tells whether the local bitmaps have any free blocks or not. */
} v15_sgmnt_data;

/* End of gdsfhead.h */

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef	v15_sgmnt_data		*v15_sgmnt_data_ptr_t;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

#endif
