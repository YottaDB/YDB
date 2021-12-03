/****************************************************************
 *								*
 * Copyright (c) 2020-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef V6_GDSFHEAD_H_INCLUDED
#define V6_GDSFHEAD_H_INCLUDED

/* v6_gdsfhead.h */
/* this requires gdsroot.h gtm_facility.h fileinfo.h gdsbt.h */

#include "mdef.h"
#include "gdsbt.h"
#include "gdsroot.h"
#include "gdsdbver.h"
#include "gvstats_rec.h"

/* Any changes must also be applied to the definition in filestruct.h */
#define V6_GDS_LABEL		GDS_LABEL_GENERIC GDS_V50

typedef struct
{
	trans_num	curr_tn;
	trans_num	early_tn;
	trans_num	last_mm_sync;		/* Last tn where a full mm sync was done */
	char		filler_8byte[8];	/* previously header_open_tn but no longer used.
						 * cannot remove as this is part of database file header */
	trans_num	mm_tn;			/* Used to see if CCP must update master map */
	uint4		lock_sequence;		/* Used to see if CCP must update lock section */
	uint4		ccp_jnl_filesize;	/* Passes size of journal file if extended */
	volatile block_id_32	total_blks;	/* Placed here so can be passed to other machines on cluster */
	volatile block_id_32	free_blocks;
} v6_th_index;

typedef struct v6_sgmnt_data_struct
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
	int		null_subs;
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
	uint4		reservedDBFlags;	/* Bit mask field containing the reserved DB flags (field copied from gd_region) */

	/************* FIELDS USED BY TN WARN PROCESSING *************************/
	trans_num	max_tn;			/* Hardstop TN for this database */
	trans_num	max_tn_warn;		/* TN for next TN_RESET warning for this database */

	/************* FIELDS SET BY MUPIP BACKUP/REORG *************************/
	trans_num	last_inc_backup;
	trans_num	last_com_backup;
	trans_num	last_rec_backup;
	block_id_32	last_inc_bkup_last_blk;	/* Last block in the database at time of last incremental backup */
	block_id_32	last_com_bkup_last_blk;	/* Last block in the database at time of last comprehensive backup */
	block_id_32	last_rec_bkup_last_blk;	/* Last block in the database at time of last record-ed backup */
	block_id_32	reorg_restart_block;
	gtm_timet       last_start_backup;	/* Last (successful) backup start (was filler). gtm_timet dumped as string */
	/************* FIELDS SET WHEN DB IS OPEN ********************************/
	char		now_running[MAX_REL_NAME];/* for active version stamp */
	uint4		filler_owner_node;	/* 4-byte filler - since owner_node is maintained on VMS only */
	uint4		image_count;		/* for db freezing. Set to "process_id" on Unix and "image_count" on VMS */
	uint4		freeze;			/* for db freezing. Set to "getuid"     on Unix and "process_id"  on VMS */
	int4		kill_in_prog;		/* counter for multi-crit kills that are not done yet */
	int4		abandoned_kills;
	uint4		unused_freeze_online_filler;	/* see field in node_local */
	char		filler_320[4];
	/************* FIELDS USED IN V4 <==> V5 COMPATIBILITY MODE ****************/
	trans_num	tn_upgrd_blks_0;	/* TN when blks_to_upgrd becomes 0.
						 *	 Never set = 0 => we have not achieved this yet,
						 *	Always set = 1 => database was created as V5 (or current version)
						 */
	trans_num	desired_db_format_tn;	/* Database tn when last db format change occurred */
	trans_num	reorg_db_fmt_start_tn;	/* Copy of desired_db_format_tn when MUPIP REORG UPGRADE/DOWNGRADE started */
	block_id_32	reorg_upgrd_dwngrd_restart_block;	/* Block numbers lesser than this were last upgraded/downgraded by
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
	v6_th_index	trans_hist;		/* transaction history - if moved from 1st filehdr block, change TH_BLOCK */
	/************* FIELDS RELATED TO WRITE CACHE FLUSHING *******************************/
	int4		write_fullblk;
	int4		statsdb_allocation;
	int4		flush_time[2];
	int4		flush_trigger;
	int4		n_wrt_per_flu;		/* Number of writes per flush call. Overloaded for BG and MM */
	int4		wait_disk_space;	/* seconds to wait for diskspace before giving up on a db block write */
	int4		defer_time;		/* defer write
						 *	 0 => immediate,
						 *	-1 => indefinite defer,
						 *	>0 => defer_time * flush_time[0] is actual defer time
						 * default value = 1 => a write-timer every csd->flush_time[0] seconds
						 */
	int4		flush_trigger_top;	/* biggest value for flush_trigger */
	boolean_t	mumps_can_bypass;	/* Allow mumps processes to bypass flushing, access control, and ftok semaphore
						 * in "gds_rundown". This was done to improve shutdown performance.
						 */
	boolean_t	epoch_taper;		/* If TRUE, GT.M tries to reduce dirty buffers as epoch approaches */
	uint4		epoch_taper_time_pct;	/* in the last pct we start tapering for time */
	uint4		epoch_taper_jnl_pct;	/* in the last pct we start tapering for jnl */
	boolean_t	asyncio;		/* If TRUE, GT.M uses async I/O */
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
	int4		filler_n_retries[CDB_MAX_TRIES];	/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_puts;				/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_kills;				/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_queries;			/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_gets;				/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_order;				/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_zprevs;			/* Now moved to TAB_GVSTATS_REC section */
	int4		filler_n_data;				/* Now moved to TAB_GVSTATS_REC section */
	uint4		filler_n_puts_duplicate;		/* Now moved to TAB_GVSTATS_REC section */
	uint4		filler_n_tp_updates;			/* Now moved to TAB_GVSTATS_REC section */
	uint4		filler_n_tp_updates_duplicate;		/* Now moved to TAB_GVSTATS_REC section */
	char		filler_accounting_64_align[4];		/* to ensure this section has 64-byte multiple size */
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
	/************* POTENTIALLY LARGE CHARACTER ARRAYS **************/
	unsigned char	jnl_file_name[JNL_NAME_SIZE];	/* journal file name */
	unsigned char	reorg_restart_key[OLD_MAX_KEY_SZ + 1];	/* 1st key of a leaf block where reorg was done last time.
								 * Note: In mu_reorg we don't save keys longer than OLD_MAX_KEY_SZ
								 */
	char		machine_name[MAX_MCNAMELEN];
	/************* ENCRYPTION-RELATED FIELDS **************/
	/* Prior to the introduction of encryption_hash and, subsequently, other encryption fields, this space was occupied by a
	 * char filler_2k[256]. Now that the encryption fields consume a part of that space, the filler has been reduced in size.
	 */
	char		encryption_hash[GTMCRYPT_RESERVED_HASH_LEN];
	char		encryption_hash2[GTMCRYPT_RESERVED_HASH_LEN];
	boolean_t	non_null_iv;
	block_id_32	encryption_hash_cutoff;		/* Points to the first block to be encrypted by MUPIP REORG -ENCRYPT with
							 * encryption_hash2. The value of -1 indicates that no (re)encryption is
							 * happening. */
	trans_num	encryption_hash2_start_tn;	/* Indicates the lowest transaction number at which a block is encrypted
							 * with encryption_hash2. */
	char		filler_encrypt[80];
	/***************************************************/
	/* The CLRGVSTATS macro wipes out everything from here through the GVSTATS fields up to gvstats_rec_old_now_filler
	 * starting from the end of the space reserved for the encryption_hash above - DO NOT insert anything in this range or
	 * move those two end points without appropriately adjusting that macro
	 */
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

	/************* FORMER GVSTATS_REC RELATED FIELDS ***********/
	/* gvstats_rec has been moved to the end of the header,    */
	/* leaving filler here.  This can be reused in the future  */
	char		gvstats_rec_old_now_filler[496];
	char		gvstats_rec_filler_4k_plus_512[16];
	char		filler_4k_plus_512[184];	/* Note: this filler array should START at offset 4K+512.
							 * So any additions of new fields should happen at the END of this
							 * filler array and the filler array size correspondingly adjusted.
							 */
	/************* FIELDS FOR V6 TO V7 UPGRADE *********************************/
	block_id	offset;				/* offset produced by mmb extension; interim pointer adjustment */
	int4		max_rec;			/* pessimistic estimate of what bkl_size could hold */
	int4		i_reserved_bytes;		/* for mangement of index splits; could be retained as a characteristic */
	boolean_t	db_got_to_V7_once;		/* set TRUE by MUPIP REORG UPGRADE once it completes all work on region */
	char		filler[164];			/* Filler to make 8-byte alignment explicit and aligned with V7 */
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
	uint4		is_encrypted;		/* Encryption state of the database as a superimposition of IS_ENCRYPTED and
						 * TO_BE_ENCRYPTED flags. */
	uint4		db_trigger_cycle;	/* incremented every MUPIP TRIGGER command that changes ^#t global contents */
	/************* SUPPLEMENTARY REPLICATION INSTANCE RELATED FIELDS ****************/
	seq_num		strm_reg_seqno[MAX_SUPPL_STRMS];	/* the jnl seqno of the last update to this region for a given
								 * supplementary stream -- 8-byte aligned */
	seq_num		save_strm_reg_seqno[MAX_SUPPL_STRMS];	/* a copy of strm_reg_seqno[] before it gets changed in
								 * "mur_process_intrpt_recov". Used only by journal recovery.
								 * See comment in "mur_get_max_strm_reg_seqno" function for
								 * purpose of this field. Must also be 8-byte aligned.
								 */
	/************* MISCELLANEOUS FIELDS ****************/
	boolean_t	freeze_on_fail;		/* Freeze instance if failure of this database observed */
	boolean_t	span_node_absent;	/* Database does not contain the spanning node */
	boolean_t	maxkeysz_assured;	/* All the keys in the database are less than MAX_KEY_SIZE */
	boolean_t	hasht_upgrade_needed;	/* ^#t global needs to be upgraded from V62000 to post-V62000 format */
	boolean_t	defer_allocate;		/* If FALSE: Use fallocate() preallocate space from the disk */
	boolean_t	filler_ftok_counter_halted;	/* Used only in V6.3-000. Kept as a filler just to be safe */
	boolean_t	filler_access_counter_halted;	/* Used only in V6.3-000. Kept as a filler just to be safe */
	boolean_t	lock_crit_with_db;		/* flag controlling LOCK crit mechanism; see interlock.h */
	uint4		basedb_fname_len;		/* byte length of filename stored in "basedb_fname[]" */
	unsigned char	basedb_fname[256]; /* full path filaneme of corresponding baseDB if this is a statsDB */
	boolean_t	read_only;		/* If TRUE, GT.M uses a process-private mmap instead of IPC */
	/************* GVSTATS_REC RELATED FIELDS ***********/
	/* gvstats_rec has outgrown its previous space.
	 * Note also that we are pushing the 8k barrier on sgmnt_data now, but are not critically pressed for space in
	 * the future because the former GVSTATS area (above) will be available for reuse.
	 */
	gvstats_rec_csd_t	gvstats_rec;	/* As of GTM-8863 1304 bytes == 163 counters */
	char			filler_8k[1464 - SIZEOF(gvstats_rec_csd_t)];
	/********************************************************/
	/* Master bitmap immediately follows. Tells whether the local bitmaps have any free blocks or not. */
} v6_sgmnt_data;

typedef	v6_sgmnt_data	*v6_sgmnt_data_ptr_t;

/* End of v6_gdsfhead.h */
#endif
