/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDSBT_H
#define GDSBT_H
/* this requires gdsroot.h */

#include <sys/types.h>
#ifdef MUTEX_MSEM_WAKE
#ifdef POSIX_MSEM
#  include "gtm_semaphore.h"
#else
#  include <sys/mman.h>
#endif
#endif

#include <gtm_limits.h>		/* for _POSIX_HOST_NAME_MAX */
#if defined(SUNOS) && !defined(_POSIX_HOST_NAME_MAX)
#	include <netdb.h>	/* for MAXHOSTNAMELEN (Solaris 9) */
#endif

#include "gvstats_rec.h"

#define CR_NOTVALID (-1L)

#define GTM64_WC_MAX_BUFFS (2*1024*1024)-1 /* to fit in an int4 */
#define WC_MAX_BUFFS 64*1024

#define WC_DEF_BUFFS 128
#define WC_MIN_BUFFS 64

#define MAX_LOCK_SPACE 262144	/* need to change these whenever global directory defaults change */
#define MIN_LOCK_SPACE 10

#define MAX_REL_NAME	36
#define MAX_MCNAMELEN   256 /* We do not support hostname truncation */
#if defined(_POSIX_HOST_NAME_MAX)
#	if MAX_MCNAMELEN <= _POSIX_HOST_NAME_MAX /* _POSIX_HOST_NAME_MAX excludes terminating NULL */
#		error MAX_MCNAMELEN is not greater than _POSIX_HOST_NAME_MAX.
#	endif
#elif defined(MAXHOSTNAMELEN)
#	if MAX_MCNAMELEN < MAXHOSTNAMELEN /* MAXHOSTNAMELEN includes terminating NULL */
#		error MAX_MCNAMELEN is less than MAXHOSTNAMELEN.
#	endif
#else
#	error _POSIX_HOST_NAME_MAX or MAXHOSTNAMELEN not defined.
#endif

#define GDS_LABEL_SZ 	12

#define MAX_DB_WTSTARTS		2			/* Max number of "flush-timer driven" simultaneous writers in wcs_wtstart */
#define MAX_WTSTART_PID_SLOTS	4 * MAX_DB_WTSTARTS	/* Max number of PIDs for wcs_wtstart to save */
#define MAX_KIP_PID_SLOTS 	8
#define MAX_WT_PID_SLOTS 	4

#define BT_FACTOR(X) (X)
#define FLUSH_FACTOR(X) ((X)-(X)/16)
#define BT_QUEHEAD (-2)
#define BT_NOTVALID (-1)
#define BT_MAXRETRY 3
#define BT_SIZE(X) ((((sgmnt_data_ptr_t)X)->bt_buckets + 1 + ((sgmnt_data_ptr_t)X)->n_bts) * SIZEOF(bt_rec))
			/* parameter is *sgmnt_data*/
			/* note that the + 1 above is for the th_queue head which falls between the hash table and the */
			/* actual bts */
#define HEADER_UPDATE_COUNT 1024
#define LAST_WBOX_SEQ_NUM 1000

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
	volatile uint4	total_blks;		/* Placed here so can be passed to other machines on cluster */
	volatile uint4	free_blocks;
} th_index;

typedef struct
{
	struct
	{
		sm_off_t 		fl;
		sm_off_t		bl;	/* self-relative queue entry */
	} blkque, tnque;		/* for block number hash, lru queue */
	trans_num	tn;		/* transaction # #*/
	trans_num	killtn;		/* last transaction when this block was updated as part of an M-kill */
	block_id	blk;		/* block #*/
	int4		cache_index;
	bool		flushing;	/* buffer is being flushed after a machine switch on a cluster */
	char		filler[3];
	int4		filler_int4;	/* maintain 8 byte alignment */
} bt_rec;				/* block table record */

/* This structure is used to access the transaction queue.  It points at all but the
   first two longwords of a bt_rec.  CAUTION:  there is no such thing as a queue of
   th_recs, they are always bt_recs, and the extra two longwords are always there */
typedef struct
{
	struct
	{
		sm_off_t 		fl;
		sm_off_t		bl;
	} tnque;
	trans_num	tn;
	trans_num	killtn;		/* last transaction when this block was updated as part of an M-kill */
	block_id	blk;
	int4		cache_index;
	bool		flushing;
	char		filler[3];
	int4		filler_int4;	/* maintain 8 byte alignment */
} th_rec;

/* This structure is used to maintain all cache records.  The BT queue contains
   a history of those blocks that have been updated.	*/

/*
 *	Definitions for GT.M Mutex Control
 *
 *	See MUTEX.MAR for VMS functional implementation
 */
typedef struct
{
	struct
	{
		sm_off_t	fl,
			bl;
	}		que;
	int4		pid;
	void		*super_crit;
	/*
	 * Make sure that the size of mutex_que_entry is a multiple of 8 bytes
	 * for quadword alignment requirements of remqhi and insqti.
	 */
	int4		mutex_wake_instance;
	int4		filler1; /* for dword alignment 		 */
#ifdef MUTEX_MSEM_WAKE
# ifdef POSIX_MSEM
	sem_t		mutex_wake_msem; /* Not two ints .. somewhat larger */
# else
	msemaphore	mutex_wake_msem; /* Two ints (incidentally two int4s) */
# endif

#endif
} mutex_que_entry;

typedef struct
{
	struct
	{
		sm_off_t	fl,
			bl;
	}		que;
	global_latch_t	latch;
} mutex_que_head;

typedef struct
{
	FILL8DCL(uint4, crit_cycle, 1);
	global_latch_t	semaphore;
	CACHELINE_PAD(8 + SIZEOF(global_latch_t), 2)		/* 8 for the FILL8DCL */
	FILL8DCL(latch_t, crashcnt, 3);
	global_latch_t	crashcnt_latch;
	CACHELINE_PAD(8 + SIZEOF(global_latch_t), 4)	/* 8 for the FILL8DCL */
	compswap_time_field	stuckexec;
	CACHELINE_PAD(SIZEOF(compswap_time_field), 5)
	FILL8DCL(latch_t, queslots, 6);
	CACHELINE_PAD(SIZEOF(latch_t) + SIZEOF(latch_t), 7)
	mutex_que_head	prochead;
	CACHELINE_PAD(SIZEOF(mutex_que_head), 8)
	mutex_que_head	freehead;
	CACHELINE_PAD(SIZEOF(mutex_que_head), 9)
} mutex_struct;

typedef struct {
	int4	mutex_hard_spin_count;
	int4	mutex_sleep_spin_count;
	int4	mutex_spin_sleep_mask;			/* mask for maximum spin sleep time */
	int4	mutex_que_entry_space_size;		/* total number of entries */
} mutex_spin_parms_struct;

enum crit_ops
{	crit_ops_gw = 1,	/* grab [write] crit */
	crit_ops_rw,		/* rel [write] crit */
	crit_ops_nocrit		/* did a rel_crit when now_crit flag was off */
};

typedef struct
{
	caddr_t		call_from;
	enum crit_ops	crit_act;
	int4		epid;
	trans_num	curr_tn;
} crit_trace;

typedef struct
{
	sm_off_t	cr_off;
	trans_num	cr_tn;
	uint4		process_id;
	block_id	blk;
	uint4		cycle;
} dskread_trace;

enum wcs_ops_trace_t
{
	wcs_ops_flu1,
	wcs_ops_flu2,
	wcs_ops_flu3,
	wcs_ops_flu4,
	wcs_ops_flu5,
	wcs_ops_flu6,
	wcs_ops_flu7,
	wcs_ops_wtstart1,
	wcs_ops_wtstart2,
	wcs_ops_wtstart3,
	wcs_ops_wtstart4,
	wcs_ops_wtstart5,
	wcs_ops_wtstart6,
	wcs_ops_wtstart7,
	wcs_ops_wtstart8,
	wcs_ops_wtfini1,
	wcs_ops_wtfini2,
	wcs_ops_wtfini3,
	wcs_ops_wtfini4,
	wcs_ops_getspace1
};

typedef struct
{
	trans_num		db_tn;
	uint4			process_id;
	uint4			wc_in_free;
	uint4			wcs_active_lvl;
	uint4			wcs_wip_lvl;
	enum wcs_ops_trace_t	type;
	block_id		blk;
	sm_off_t		cr_off;
	trans_num		cr_dirty;
	uint4			detail1;
	uint4			detail2;
} wcs_ops_trace_t;

#define OP_LOCK_SIZE	4

/* Structure to hold lock history */
typedef struct
{
	sm_int_ptr_t	lock_addr;		/* Address of actual lock */
	caddr_t		lock_callr;		/* Address of (un)locker */
	int4		lock_pid;		/* Process id of (un)locker */
	int4		loop_cnt;		/* iteration count of lock retry loop */
	char		lock_op[OP_LOCK_SIZE];	/* Operation performed (either OBTN or RLSE) */
} lockhist;

enum ftok_ops
{
	ftok_ops_lock = 1,
	ftok_ops_release
};

typedef struct
{
	enum ftok_ops	ftok_oper;
	uint4           process_id;
	trans_num       cr_tn;
} ftokhist;

#define	DSKREAD_OPS_ARRAY_SIZE		 512
#define	WCS_OPS_ARRAY_SIZE		1024
#define	CRIT_OPS_ARRAY_SIZE		 512
#define	LOCKHIST_ARRAY_SIZE		 512
#define FTOK_OPS_ARRAY_SIZE		 512

/*
 * Enable the GTM_CRYPT_UPDATES_REPORT define below to activate logging of encryption-related operations in shared memory. Those
 * operations currently include a write and read of an encrypted block (wcs_wtstart and dsk_read, respectively), update retry or
 * abort due to a concurrent change of encryption cycle (t_end and tp_tend, wcs_wtstart, respectively), update of encryption
 * settings (mupip_reorg_encrypt), and receipt of new encryption settings (t_retry and tp_restart). Add more macros and macro
 * callers as necessary. The final report is printed via the DBG_PRINT_BLOCK_INFOS macro (gtm_fork_n_core).
 */
#ifdef DEBUG
/* #define GTM_CRYPT_UPDATES_REPORT 1 */
#endif

#ifdef GTM_CRYPT_UPDATES_REPORT
# define BLK_INFO_ARRAY_SIZE		 10000	/* Static array size for all encryption-related updates. */

typedef struct
{
	uint4		blk_num;
	uint4		operation;
	trans_num	dbtn;
	trans_num	blktn;
	boolean_t	use_new_key;
	uint4		subtype;
	uint4		blk_size;
	uint4		blk_encr_len;
	char		pre_block[32];
	char		post_block[32];
} block_update_t;

typedef struct
{
	uint4		is_encrypted;
	uint4		reorg_encrypt_cycle;
	char            hash[GTMCRYPT_HASH_LEN];
	char            hash2[GTMCRYPT_HASH_LEN];
	block_id	encryption_hash_cutoff;
} trans_update_t;

typedef union
{
	block_update_t	block_update;
	trans_update_t	trans_update;
} info_t;

typedef struct blk_info_struct
{
	uint4		type;
	uint4		pid;
	uint4		is_encrypted;
	int4		csa_reorg_encrypt_cycle;
	uint4		cnl_reorg_encrypt_cycle;
	block_id	encryption_hash_cutoff;
	trans_num	encryption_hash2_start_tn;
	char            hash[GTMCRYPT_HASH_LEN];
	char            hash2[GTMCRYPT_HASH_LEN];
	char		region[100];
	char		where[100];
	info_t		info;
} blk_info;

# define DBG_PRINT_BLOCK_INFOS(CNL)										\
{														\
	int		i, j, block_index, start, end;								\
	blk_info	*blk_info_ptr;										\
														\
	if ((CNL)->blk_info_cnt <= BLK_INFO_ARRAY_SIZE)								\
	{													\
		start = 0;											\
		end = start + (CNL)->blk_info_cnt;								\
	} else													\
	{													\
		end = (CNL)->blk_info_cnt;									\
		start = end - BLK_INFO_ARRAY_SIZE;								\
	}													\
	for (i = start; i < end; i++)										\
	{													\
		block_index = i % BLK_INFO_ARRAY_SIZE;								\
		blk_info_ptr = &((CNL)->blk_infos[block_index]);						\
		if (0 == blk_info_ptr->type)									\
		{												\
			if (0 == blk_info_ptr->info.block_update.operation)					\
				FPRINTF(stderr, "%d: BLOCK READ", block_index);					\
			else if (1 == blk_info_ptr->info.block_update.operation)				\
				FPRINTF(stderr, "%d: BLOCK WRITE", block_index);				\
		} else if (1 == blk_info_ptr->type)								\
			FPRINTF(stderr, "%d: BLOCK RETRY", block_index);					\
		else if (2 == blk_info_ptr->type)								\
			FPRINTF(stderr, "%d: BLOCK ABORT", block_index);					\
		else if (3 == blk_info_ptr->type)								\
			FPRINTF(stderr, "%d: CRYPT UPDATE", block_index);					\
		else if (4 == blk_info_ptr->type)								\
			FPRINTF(stderr, "%d: CRYPT RECEIVE", block_index);					\
		FPRINTF(stderr, " on %s in %s:", blk_info_ptr->region, blk_info_ptr->where);			\
		FPRINTF(stderr,											\
			"\n\tpid: %u\n"										\
			"\tis_encrypted: %u\n"									\
			"\tcsa_reorg_encrypt_cycle: %d\n"							\
			"\tcnl_reorg_encrypt_cycle: %u\n"							\
			"\tencryption_hash_cutoff: %d\n"							\
			"\tencryption_hash2_start_tn: %ld",							\
			blk_info_ptr->pid,									\
			blk_info_ptr->is_encrypted,								\
			blk_info_ptr->csa_reorg_encrypt_cycle,							\
			blk_info_ptr->cnl_reorg_encrypt_cycle,							\
			blk_info_ptr->encryption_hash_cutoff,							\
			blk_info_ptr->encryption_hash2_start_tn);						\
		FPRINTF(stderr, "\n\tencryption_hash: ");							\
		for (j = 0; j < GTMCRYPT_HASH_LEN; j += 2)							\
			FPRINTF(stderr, "%02X", (unsigned char)blk_info_ptr->hash[j / 2]);			\
		FPRINTF(stderr, "\n\tencryption_hash2: ");							\
		for (j = 0; j < GTMCRYPT_HASH_LEN; j += 2)							\
			FPRINTF(stderr, "%02X", (unsigned char)blk_info_ptr->hash2[j / 2]);			\
		if (0 == blk_info_ptr->type)									\
		{												\
			FPRINTF(stderr,										\
				"\n\tblk_num   : %u\n"								\
				"\toperation   : %d\n"								\
				"\tdbtn        : 0x%llx\n"							\
				"\tblktn       : 0x%llx\n"							\
				"\tuse_new_key : %d\n"								\
				"\tsubtype     : %d\n"								\
				"\tblk_size    : %d\n"								\
				"\tblk_encr_len: %d\n",								\
				blk_info_ptr->info.block_update.blk_num,					\
				blk_info_ptr->info.block_update.operation,					\
				blk_info_ptr->info.block_update.dbtn,						\
				blk_info_ptr->info.block_update.blktn,						\
				blk_info_ptr->info.block_update.use_new_key,					\
				blk_info_ptr->info.block_update.subtype,					\
				blk_info_ptr->info.block_update.blk_size,					\
				blk_info_ptr->info.block_update.blk_encr_len);					\
		} else if (4 == blk_info_ptr->type)								\
		{												\
			FPRINTF(stderr,										\
				"\n\tis_encrypted: %u\n"							\
				"\treorg_encrypt_cycle: %u\n"							\
				"\tencryption_hash_cutoff: %d",							\
				blk_info_ptr->info.trans_update.is_encrypted,					\
				blk_info_ptr->info.trans_update.reorg_encrypt_cycle,				\
				blk_info_ptr->info.trans_update.encryption_hash_cutoff);			\
			FPRINTF(stderr, "\n\thash: ");								\
			for (j = 0; j < GTMCRYPT_HASH_LEN; j += 2)						\
				FPRINTF(stderr, "%02X",								\
						(unsigned char)blk_info_ptr->info.trans_update.hash[j / 2]);	\
			FPRINTF(stderr, "\n\thash2: ");								\
			for (j = 0; j < GTMCRYPT_HASH_LEN; j += 2)						\
				FPRINTF(stderr, "%02X",								\
						(unsigned char)blk_info_ptr->info.trans_update.hash2[j / 2]);	\
			FPRINTF(stderr, "\n");									\
		} else												\
			FPRINTF(stderr, "\n");									\
		FFLUSH(stderr);											\
	}													\
}

# define DBG_RECORD_COMMON_STUFF(BLK_INFO_PTR, TYPE, CSD, CSA, CNL, PID)				\
	blk_info *blk_info_ptr;										\
													\
	BLK_INFO_PTR = &((CNL)->blk_infos[(CNL)->blk_info_cnt++ % BLK_INFO_ARRAY_SIZE]);		\
	BLK_INFO_PTR->type = TYPE;									\
	BLK_INFO_PTR->pid = PID;									\
	BLK_INFO_PTR->is_encrypted = (CSD)->is_encrypted;						\
	BLK_INFO_PTR->csa_reorg_encrypt_cycle =								\
		(NULL == (CSA)->encr_ptr) ? -1 : (CSA)->encr_ptr->reorg_encrypt_cycle;			\
	BLK_INFO_PTR->cnl_reorg_encrypt_cycle = (CNL)->reorg_encrypt_cycle;				\
	BLK_INFO_PTR->encryption_hash_cutoff = (CSD)->encryption_hash_cutoff;				\
	BLK_INFO_PTR->encryption_hash2_start_tn = (CSD)->encryption_hash2_start_tn;			\
	memcpy(BLK_INFO_PTR->hash, (CSD)->encryption_hash, GTMCRYPT_HASH_LEN);				\
	memcpy(BLK_INFO_PTR->hash2, (CSD)->encryption_hash2, GTMCRYPT_HASH_LEN);			\
	strcpy(BLK_INFO_PTR->where, __FILE__);								\
	if ((NULL != (CSA)->region) && (NULL != (CSA)->region->dyn.addr))				\
	{												\
		memcpy(BLK_INFO_PTR->region, (char *)(CSA)->region->dyn.addr->fname,			\
				(CSA)->region->dyn.addr->fname_len);					\
		BLK_INFO_PTR->region[(CSA)->region->dyn.addr->fname_len] = '\0';			\
	} else												\
		strcpy(BLK_INFO_PTR->region, "UNKNOWN");						\

# define DBG_RECORD_BLOCK_UPDATE(CSD, CSA, CNL, PID, BLK, OPER, BLKTN, SUBTYPE, NEW_KEY, PRE_BLK, POST_BLK, BLK_SIZE, BLK_ENC_LEN) \
{														\
	DBG_RECORD_COMMON_STUFF(blk_info_ptr, 0, CSD, CSA, CNL, PID);						\
	blk_info_ptr->info.block_update.blk_num = BLK;								\
	blk_info_ptr->info.block_update.operation = OPER;							\
	blk_info_ptr->info.block_update.dbtn = CSD->trans_hist.curr_tn;						\
	blk_info_ptr->info.block_update.blktn = BLKTN;								\
	blk_info_ptr->info.block_update.use_new_key = NEW_KEY;							\
	blk_info_ptr->info.block_update.subtype = SUBTYPE;							\
	memcpy(blk_info_ptr->info.block_update.pre_block, PRE_BLK, 32);						\
	memcpy(blk_info_ptr->info.block_update.post_block, POST_BLK, 32);					\
	blk_info_ptr->info.block_update.blk_size = BLK_SIZE;							\
	blk_info_ptr->info.block_update.blk_encr_len = BLK_ENC_LEN;						\
}

# define DBG_RECORD_BLOCK_READ(CSD, CSA, CNL, PID, BLK, BLKTN, SUBTYPE, NEW_KEY, PRE_BLK, POST_BLK, BLK_SIZE, BLK_ENCR_LEN)	\
{																\
	DBG_RECORD_BLOCK_UPDATE(CSD, CSA, CNL, PID, BLK, 0, BLKTN, SUBTYPE, NEW_KEY, PRE_BLK, POST_BLK, BLK_SIZE, BLK_ENCR_LEN);\
}

# define DBG_RECORD_BLOCK_WRITE(CSD, CSA, CNL, PID, BLK, BLKTN, SUBTYPE, NEW_KEY, PRE_BLK, POST_BLK, BLK_SIZE, BLK_ENCR_LEN)	\
{																\
	DBG_RECORD_BLOCK_UPDATE(CSD, CSA, CNL, PID, BLK, 1, BLKTN, SUBTYPE, NEW_KEY, PRE_BLK, POST_BLK, BLK_SIZE, BLK_ENCR_LEN);\
}

# define DBG_RECORD_BLOCK_RETRY(CSD, CSA, CNL, PID)							\
{													\
	DBG_RECORD_COMMON_STUFF(blk_info_ptr, 1, CSD, CSA, CNL, PID);					\
}

# define DBG_RECORD_BLOCK_ABORT(CSD, CSA, CNL, PID)							\
{													\
	DBG_RECORD_COMMON_STUFF(blk_info_ptr, 2, CSD, CSA, CNL, PID);					\
}

# define DBG_RECORD_CRYPT_UPDATE(CSD, CSA, CNL, PID)							\
{													\
	DBG_RECORD_COMMON_STUFF(blk_info_ptr, 3, CSD, CSA, CNL, PID);					\
}

# define DBG_RECORD_CRYPT_RECEIVE(CSD, CSA, CNL, PID, TRANS_INFO)					\
{													\
	DBG_RECORD_COMMON_STUFF(blk_info_ptr, 4, CSD, CSA, CNL, PID);					\
	blk_info_ptr->info.trans_update.is_encrypted = TRANS_INFO->is_encrypted;			\
	blk_info_ptr->info.trans_update.reorg_encrypt_cycle = TRANS_INFO->reorg_encrypt_cycle;		\
	blk_info_ptr->info.trans_update.encryption_hash_cutoff = TRANS_INFO->encryption_hash_cutoff;	\
	memcpy(blk_info_ptr->info.trans_update.hash, TRANS_INFO->encryption_hash, GTMCRYPT_HASH_LEN);	\
	memcpy(blk_info_ptr->info.trans_update.hash2, TRANS_INFO->encryption_hash2, GTMCRYPT_HASH_LEN);	\
}

#else
# define DBG_PRINT_BLOCK_INFOS(CNL)
# define DBG_RECORD_BLOCK_READ(CSD, CSA, CNL, PID, BLK, BLKTN, SUBTYPE, USE_NEW_KEY, PRE_BLOCK, POST_BLOCK, BLK_SIZE, BLK_ENCR_LEN)
# define DBG_RECORD_BLOCK_WRITE(CSD, CSA, CNL, PID, BLK, BLKTN, SUBTYPE, USE_NEW_KEY, PRE_BLOCK, POST_BLOCK, BLK_SIZE, BLK_ENCR_LEN)
# define DBG_RECORD_BLOCK_RETRY(CSD, CSA, CNL, PID)
# define DBG_RECORD_BLOCK_ABORT(CSD, CSA, CNL, PID)
# define DBG_RECORD_CRYPT_UPDATE(CSD, CSA, CNL, PID)
# define DBG_RECORD_CRYPT_RECEIVE(CSD, CSA, CNL, PID, TRANS_INFO)
#endif

/* Mapped space local to each node on the cluster */
typedef struct node_local_struct
{
	unsigned char   label[GDS_LABEL_SZ];			/* 12	signature for GDS shared memory */
	unsigned char	fname[MAX_FN_LEN + 1];			/* 256	filename of corresponding database */
	char		now_running[MAX_REL_NAME];		/* 36	current active GT.M version stamp */
	char		machine_name[MAX_MCNAMELEN];		/* 256	machine name for clustering */
	sm_off_t	bt_header_off;				/* (QW alignment) offset to hash table */
	sm_off_t	bt_base_off;				/* bt first entry */
	sm_off_t	th_base_off;
	sm_off_t	cache_off;
	sm_off_t	cur_lru_cache_rec_off;			/* current LRU cache_rec pointer offset */
	sm_off_t	critical;
	sm_off_t	jnl_buff;
	sm_off_t	shmpool_buffer;				/* Shared memory buffer pool area */
	sm_off_t	lock_addrs;
	sm_off_t	hdr;					/* Offset to file-header (BG mode ONLY!) */
	volatile int4	in_crit;
	int4		in_reinit;
	unsigned short	ccp_cycle;
	unsigned short	filler;					/* Align for ccp_cycle. Not changing to int
								   as that would perturb to many things at this point */
	boolean_t	ccp_crit_blocked;
	int4		ccp_state;
	boolean_t	ccp_jnl_closed;
	boolean_t	glob_sec_init;
	uint4		wtstart_pid[MAX_WTSTART_PID_SLOTS];	/* Maintain pids of wcs_wtstart processes */
	volatile boolean_t wc_blocked;				/* Set to TRUE by process that knows it is leaving the cache in a
								 * possibly inconsistent state. Next process grabbing crit will do
								 * cache recovery. This setting also stops all concurrent writers
								 * from working on the cache. In MM mode, it is used to call
								 * wcs_recover during a file extension */
	global_latch_t	wc_var_lock;                            /* latch used for access to various wc_* ref counters */
	CACHELINE_PAD(SIZEOF(global_latch_t), 1)		/* Keep these two latches in separate cache lines */
	global_latch_t	db_latch;                               /* latch for interlocking on hppa and tandem */
	CACHELINE_PAD(SIZEOF(global_latch_t), 2)
	int4		cache_hits;
	int4		wc_in_free;                             /* number of write cache records in free queue */
	/* All counters below (declared using CNTR4DCL) are 2 or 4-bytes, depending on platform, but always stored in 4 bytes.
	 * CACHELINE_PAD doesn't use SIZEOF because misses any padding added by CNTR4DCL. We want to keep the counters in
	 * separate cachelines on load-lock/store-conditional platforms particularly and on other platforms too, just to be safe.
	 */
	volatile CNTR4DCL(wcs_timers, 1);			/* number of write cache timers in use - 1 */
	CACHELINE_PAD(4, 3)
	volatile CNTR4DCL(wcs_active_lvl, 2);			/* number of entries in active queue */
	CACHELINE_PAD(4, 4)
	volatile CNTR4DCL(wcs_staleness, 3);
	CACHELINE_PAD(4, 5)
	volatile CNTR4DCL(ref_cnt, 4);				/* reference count. How many people are using the database */
	CACHELINE_PAD(4, 6)
	volatile CNTR4DCL(intent_wtstart, 5);			/* Count of processes that INTEND to enter wcs_wtstart code */
	CACHELINE_PAD(4, 7)
	volatile CNTR4DCL(in_wtstart, 6);			/* Count of processes that are INSIDE wcs_wtstart code */
	CACHELINE_PAD(4, 8)
	volatile CNTR4DCL(wcs_phase2_commit_pidcnt, 7);		/* number of processes actively finishing phase2 commit */
	CACHELINE_PAD(4, 9)
	volatile CNTR4DCL(wcs_wip_lvl, 8);			/* number of entries in wip queue */
	CACHELINE_PAD(4, 10)
	volatile int4	wtfini_in_prog;				/* whether wcs_wtfini() is in progress at this time */
	boolean_t	freezer_waited_for_kip;			/* currently used only in dbg code */
	int4            mm_extender_pid;			/* pid of the process executing gdsfilext in MM mode */
	int4            highest_lbm_blk_changed;                /* Records highest local bit map block that
									changed so we know how much of master bit
									map to write out. Modified only under crit */
	int4		nbb;                                    /* Next backup block -- for online backup */
	int4		lockhist_idx;				/* (DW alignment) "circular" index into lockhists array */
	int4		crit_ops_index;				/* "circular" index into crit_ops_array */
	int4		dskread_ops_index;			/* "circular" index into dskread_ops_array */
	int4		ftok_ops_index;				/* "circular" index into ftok_ops_array */
	int4		wcs_ops_index;				/* "circular" index into wcs_ops_array */
	lockhist	lockhists[LOCKHIST_ARRAY_SIZE];		/* Keep lock histories here */
	crit_trace	crit_ops_array[CRIT_OPS_ARRAY_SIZE];	/* space for CRIT_TRACE macro to record info */
	dskread_trace	dskread_ops_array[DSKREAD_OPS_ARRAY_SIZE];	/* space for DSKREAD_TRACE macro to record info */
	wcs_ops_trace_t	wcs_ops_array[WCS_OPS_ARRAY_SIZE];	/* space for WCS_OPS_TRACE macro to record info */
	unique_file_id	unique_id;
	uint4		owner_node;
	volatile int4   wcsflu_pid;				/* pid of the process executing wcs_flu in BG mode */
	int4		creation_date_time4;			/* Lower order 4-bytes of database's creation time to be
								 * compared at sm attach time */
	int4		inhibit_kills;				/* inhibit new KILLs while MUPIP BACKUP, INTEG or FREEZE are
					 			 * waiting for kill-in-progress to become zero
					 			 */
	boolean_t	remove_shm;				/* can this shm be removed by the last process to rundown */
	union
	{
		gds_file_id	jnl_file_id;  	/* needed on UNIX to hold space */
		unix_file_id	u;		/* from gdsroot.h even for VMS */
	} jnl_file;	/* Note that in versions before V4.3-001B, "jnl_file" used to be a member of sgmnt_data.
			 * Now it is a filler there and rightly used here since it is non-zero only when shared memory is active.
			 */
	boolean_t	donotflush_dbjnl; /* whether database and journal can be flushed to disk or not (TRUE for mupip recover) */
	int4		n_pre_read;
	char		replinstfilename[MAX_FN_LEN + 1];/* 256 : Name of the replication instance file corresponding to this db */
	char		statsdb_fname[MAX_FN_LEN + 1];	/* Is empty-string if IS_RDBF_STATSDB(csd) is FALSE.
							 * Is name of the statsdb corresponding to this basedb otherwise.
							 */
	gvstats_rec_t	gvstats_rec;
	trans_num	last_wcsflu_tn;			/* curr_tn when last wcs_flu was done on this database */
	trans_num	last_wcs_recover_tn;		/* csa->ti->curr_tn of most recent "wcs_recover" */
	sm_off_t	encrypt_glo_buff_off;	/* offset from unencrypted global buffer to its encrypted counterpart */
	global_latch_t	snapshot_crit_latch;	/* To be acquired by any process that wants to clean up an orphaned snapshot or
						 * initiate a new snapshot
						 */
	long		ss_shmid;		 /* Identifier of the shared memory for the snapshot that started
						  * recently.
						  */
	uint4		ss_shmcycle;	 	 /* incremented everytime a new snapshot creates a new shared memory identifier */
	boolean_t	snapshot_in_prog;	 /* Tells GT.M if any snapshots are in progress */
	uint4		num_snapshots_in_effect; /* how many snapshots are currently in place for this region */
	uint4		wbox_test_seq_num;	 /* used to coordinate with sequential testing steps */
	uint4		freeze_online;		 /* for online db freezing, a.k.a. chill.  */
	uint4		kip_pid_array[MAX_KIP_PID_SLOTS];	/* Processes actively doing kill (0 denotes empty slots) */
	gtm_uint64_t	sec_size;	/* Upon going to larger shared memory sizes, we realized that this does not	*/
					/* need	to be in the file header but the node local since it can be calculated	*/
					/* from info in the file header.						*/
	int4		jnlpool_shmid;	/* copy of jnlpool->repl_inst_filehdr->jnlpool_shmid to prevent mixing of multiple
					 * journal pools within the same database.
					 */
	boolean_t	lockspacefull_logged;			/* Avoids flooding syslog with LOCKSPACEFULL messages.
								 * If TRUE: LOCKSPACEFULL is written to the syslog.
								 * If FALSE: Do not write LOCKSPACEFULL to syslog.
								 * Set this to FALSE if free space ratio is above
								 * LOCK_SPACE_FULL_SYSLOG_THRESHOLD (defined in mlk_unlock.h).
								 * We exclude mlk_shrsub, and only consider mlk_prcblk & mlk_shrblk.
								 */
	uint4		trunc_pid;			/* Operating truncate. */
	block_id	highest_lbm_with_busy_blk;	/* Furthest lmap block known to have had a busy block during truncate. */
	ftokhist	ftok_ops_array[FTOK_OPS_ARRAY_SIZE];
	volatile uint4	root_search_cycle;	/* incremented online rollback ends and mu_swap_root */
	volatile uint4	onln_rlbk_cycle;	/* incremented everytime an online rollback ends */
	volatile uint4	db_onln_rlbkd_cycle;	/* incremented everytime an online rollback takes the database back in time */
	volatile uint4	onln_rlbk_pid;		/* process ID of currently running online rollback. */
	uint4		dbrndwn_ftok_skip;	/* # of processes that skipped FTOK semaphore in gds_rundown due to too many MUMPS
						   processes */
	uint4		dbrndwn_access_skip;	/* # of processes that skipped access control semaphore in gds_rundown due to a
						   concurrent online rollback or too many MUMPS processes */
	boolean_t	fastinteg_in_prog;	/* Tells GT.M if fast integrity is in progress */
	uint4           wtstart_errcnt;
	/* Note that although the below two fields are dbg-only, they are defined for pro since we want to keep the shared
	 * memory layout the same for both pro and dbg. There is some code that relies on this assumption.
	 */
	boolean_t	fake_db_enospc;		/* used only by dbg versions to simulate ENOSPC scenarios in the database file */
	boolean_t	fake_jnl_enospc;	/* used only by dbg versions to simulate ENOSPC scenarios in the journal  file */
	boolean_t	doing_epoch;		/* set when performing an epoch */
	uint4		epoch_taper_start_dbuffs; /* wcs_active_lvl at start of taper */
	boolean_t	epoch_taper_need_fsync;
	uint4		wt_pid_array[MAX_WT_PID_SLOTS];		/* Processes with active wcs_timers (0 denotes empty slots)
								 * Note: Unreliable - For Diagnostic Purposes only
								 */
	uint4		reorg_encrypt_pid;	/* indicates whether a MUPIP REORG -ENCRYPT is in progress */
	uint4		reorg_encrypt_cycle;	/* reflects the cycle of database encryption status in a series of
						   MUPIP REORG -ENCRYPTs */
	uint4		mupip_extract_count;	/* count of currently running MUPIP EXTRACTs; to be improved with GTM-8488 */
	/* Below 4 values are cached from the original DB file header that created the shared memory segment. Used by DSE only */
	enum db_acc_method      saved_acc_meth;
	int4			saved_blk_size;
	uint4			saved_lock_space_size;
	int4			saved_jnl_buffer_size;
	/* Miscellaneous flag */
	trans_num	update_underway_tn;
	boolean_t	lastwriterbypas_msg_issued;	/* whether a LASTWRITERBYPAS message has been once issued for this db */
	boolean_t	first_writer_seen;	/* Has a process with read-write access to the database opened it yet */
	boolean_t	first_nonbypas_writer_seen;	/* TRUE when first writer is seen that also does not bypass ftok/access */
	boolean_t	ftok_counter_halted;		/* The ftok semaphore counter reached 32K at some point in time */
	boolean_t	access_counter_halted;		/* The access semaphore counter reached 32K at some point in time */
	boolean_t	statsdb_created;		/* TRUE if a statsdb has been created for this basedb */
	uint4		statsdb_fname_len;		/* length of "cnl->statsdb_fname" */
	boolean_t	statsdb_rundown_clean;		/* TRUE if statsdb "gds_rundown"/"mu_rndwn_file" was clean.
							 *      This means statsdb file can be removed on a clean basedb rundown.
							 */
#	ifdef GTM_CRYPT_UPDATES_REPORT
	blk_info	blk_infos[BLK_INFO_ARRAY_SIZE];
	uint4		blk_info_cnt;
	int4		filler_8byte_align2;
#	endif
	global_latch_t	freeze_latch;		/* Protect freeze/freeze_online field updates */
	gtm_uint64_t	wcs_buffs_freed; /* this is a count of the number of buffers transitioned to the free "queue" */
	volatile gtm_uint64_t	dskspace_next_fire;
	global_latch_t	lock_crit;		/* mutex for LOCK processing */
	volatile block_id	tp_hint;
	int4		filler_8byte_align3;
} node_local;

#define	COPY_STATSDB_FNAME_INTO_STATSREG(statsDBreg, statsDBfname, statsDBfname_len)				\
MBSTART {													\
	unsigned int		fnameLen;									\
														\
	assert(ARRAYSIZE(statsDBreg->dyn.addr->fname) >= ARRAYSIZE(statsDBfname));				\
	fnameLen = MIN(statsDBfname_len, ARRAYSIZE(statsDBreg->dyn.addr->fname) - 1);				\
	assert('\0' == statsDBfname[fnameLen]);									\
	memcpy(statsDBreg->dyn.addr->fname, statsDBfname, fnameLen + 1);	/* copy trailing '\0' too */	\
	statsDBreg->dyn.addr->fname_len = fnameLen;								\
} MBEND

#define	COPY_BASEDB_FNAME_INTO_STATSDB_HDR(statsDBreg, baseDBreg, statsDBcsd)						\
MBSTART {														\
	unsigned int		fname_len;										\
															\
	assert(IS_STATSDB_REG(statsDBreg));										\
	assert(ARRAYSIZE(baseDBreg->dyn.addr->fname) <= ARRAYSIZE(statsDBcsd->basedb_fname));				\
	fname_len = MIN(baseDBreg->dyn.addr->fname_len, ARRAYSIZE(baseDBreg->dyn.addr->fname) - 1);			\
	assert(fname_len);												\
	assert('\0' == baseDBreg->dyn.addr->fname[fname_len]);								\
	memcpy(statsDBcsd->basedb_fname, baseDBreg->dyn.addr->fname, fname_len + 1);	/* copy trailing '\0' too */	\
	statsDBcsd->basedb_fname_len = fname_len;									\
} MBEND

#define	UNLINK_STATSDB_AT_BASEDB_RUNDOWN(CNL)										\
MBSTART {														\
	if (CNL->statsdb_created && CNL->statsdb_rundown_clean)								\
	{														\
		assert(CNL->statsdb_fname_len);	/* "gvcst_init" would not have set CNL->statsdb_created otherwise */	\
		assert('\0' == CNL->statsdb_fname[CNL->statsdb_fname_len]);						\
		rc = UNLINK(CNL->statsdb_fname);									\
		/* If error removing statsdb, ignore as we want to continue rundown of basedb (more important) */	\
		CNL->statsdb_created = FALSE;										\
	}														\
} MBEND

#define ADD_ENT_TO_ACTIVE_QUE_CNT(CNL)		(INCR_CNT((sm_int_ptr_t)(&CNL->wcs_active_lvl),			\
								(sm_global_latch_ptr_t)(&CNL->wc_var_lock)))
#define SUB_ENT_FROM_ACTIVE_QUE_CNT(CNL)	(DECR_CNT((sm_int_ptr_t)(&CNL->wcs_active_lvl),			\
								(sm_global_latch_ptr_t)(&CNL->wc_var_lock)))
#define ADD_ENT_TO_WIP_QUE_CNT(CNL)		(INCR_CNT((sm_int_ptr_t)(&CNL->wcs_wip_lvl),			\
								(sm_global_latch_ptr_t)(&CNL->wc_var_lock)))
#define SUB_ENT_FROM_WIP_QUE_CNT(CNL)		(DECR_CNT((sm_int_ptr_t)(&CNL->wcs_wip_lvl),			\
								(sm_global_latch_ptr_t)(&CNL->wc_var_lock)))
#define ADD_ENT_TO_FREE_QUE_CNT(CNL)		(INCR_CNT((sm_int_ptr_t)(&CNL->wc_in_free),			\
								(sm_global_latch_ptr_t)(&CNL->wc_var_lock)))
#define SUB_ENT_FROM_FREE_QUE_CNT(CNL)		(DECR_CNT((sm_int_ptr_t)(&CNL->wc_in_free),			\
								(sm_global_latch_ptr_t)(&CNL->wc_var_lock)))

#define DSKREAD_TRACE(CSA, CR_OFF, CR_TN, PID, BLK, CYCLE)		\
MBSTART {								\
	int4			doidx;					\
	node_local_ptr_t	cnl;					\
	assert((NULL != CSA)&& (NULL != (CSA->nl)));			\
	cnl = CSA->nl;							\
	doidx = ++cnl->dskread_ops_index;				\
	if (DSKREAD_OPS_ARRAY_SIZE <= doidx)				\
		cnl->dskread_ops_index = doidx = 0;			\
	cnl->dskread_ops_array[doidx].cr_off = CR_OFF;			\
	cnl->dskread_ops_array[doidx].cr_tn = CR_TN;			\
	cnl->dskread_ops_array[doidx].process_id = PID;			\
	cnl->dskread_ops_array[doidx].blk = BLK;			\
	cnl->dskread_ops_array[doidx].cycle = CYCLE;			\
} MBEND

#ifdef DEBUG
/* The following macro does not use a separate semaphore to protect its maintenance of the shared memory
 * value crit_ops_index (which would complicate precisely the situation it was created to examine) therefore,
 * in order to to maximize the chances of gathering meaningful data, it seems better placed after grab_crit
 * and before rel_crit. Also we will increment the index first and cache it so we can shorten our exposure window.
 */
#define CRIT_TRACE(CSA, X)						\
MBSTART {								\
	int4			coidx;					\
	node_local_ptr_t	cnl;					\
	boolean_t		in_ast;					\
	unsigned int		ast_status;				\
									\
	assert((NULL != CSA) && (NULL !=(CSA->nl)));			\
	cnl = CSA->nl;							\
	coidx = ++cnl->crit_ops_index;					\
	if (CRIT_OPS_ARRAY_SIZE <= coidx)				\
		cnl->crit_ops_index = coidx = 0;			\
	cnl->crit_ops_array[coidx].call_from = (caddr_t)caller_id();	\
	cnl->crit_ops_array[coidx].epid = process_id;			\
	cnl->crit_ops_array[coidx].crit_act = (X);			\
	cnl->crit_ops_array[coidx].curr_tn = (NULL != CSA->hdr) ?	\
		CSA->hdr->trans_hist.curr_tn  : 0;			\
} MBEND

/* The following macro checks that curr_tn and early_tn are equal right before beginning a transaction commit.
 * The only exception we know of is if a process in the midst of commit had been killed (kill -9 or STOP/ID)
 * after having incremented early_tn but before it finished the commit (and therefore incremented curr_tn).
 * In that case another process that did a rundown (and executed secshr_db_clnup) at around the same time
 * could have cleaned up the CRIT lock (sensing that the crit holder pid is no longer alive) making the crit
 * lock available for other processes. To check if that is the case, we need to go back the crit_ops_array and check that
 *	a) the most recent crit operation was a grab crit done by the current pid (crit_act == crit_ops_gw) AND
 *	b) the immediately previous crit operation should NOT be a release crit crit_ops_rw but instead should be a crit_ops_gw
 *	c) there are two exceptions to this and they are
 *		(i) that there could be one or more crit_ops_nocrit actions from processes that tried releasing crit
 *			even though they don't own it (cases we know of are in gds_rundown and in t_end/tp_tend if
 *			t_commit_cleanup completes the transaction after a mid-commit error).
 *		(ii) there could be one or more crit_ops_gw/crit_ops_rw pair of operations by a pid in between.
 */
#define	ASSERT_CURR_TN_EQUALS_EARLY_TN(csa, currtn)						\
MBSTART {											\
	GBLREF	uint4 			process_id;						\
												\
	assert((currtn) == csa->ti->curr_tn);							\
	if (csa->ti->early_tn != (currtn))							\
	{											\
		int4			coidx, lcnt;						\
		node_local_ptr_t	cnl;							\
		uint4			expect_gw_pid = 0;					\
												\
		cnl = csa->nl;									\
		assert(NULL != (node_local_ptr_t)cnl);						\
		coidx = cnl->crit_ops_index;							\
		assert(CRIT_OPS_ARRAY_SIZE > coidx);						\
		assert(crit_ops_gw == cnl->crit_ops_array[coidx].crit_act);			\
		assert(process_id == cnl->crit_ops_array[coidx].epid);				\
		for (lcnt = 0; CRIT_OPS_ARRAY_SIZE > lcnt; lcnt++)				\
		{										\
			if (coidx)								\
				coidx--;							\
			else									\
				coidx = CRIT_OPS_ARRAY_SIZE - 1;				\
			if (crit_ops_nocrit == cnl->crit_ops_array[coidx].crit_act)		\
				continue;							\
			if (crit_ops_rw == cnl->crit_ops_array[coidx].crit_act)			\
			{									\
				assert(0 == expect_gw_pid);					\
				expect_gw_pid = cnl->crit_ops_array[coidx].epid;		\
			} else if (crit_ops_gw == cnl->crit_ops_array[coidx].crit_act)		\
			{									\
				if (!expect_gw_pid)						\
					break;	/* found lone grab-crit */			\
				assert(expect_gw_pid == cnl->crit_ops_array[coidx].epid);	\
				expect_gw_pid = 0;/* found paired grab-crit. continue search */	\
			}									\
		}										\
		assert(CRIT_OPS_ARRAY_SIZE > lcnt); /* assert if did not find lone grab-crit */	\
	}											\
} MBEND

/*
 * The following macro places lock history entries in an array for debugging.
 * NOTE: Users of this macro, set either of the following prior to using this macro.
 * 	 (i) gv_cur_region to the region whose history we are storing.
 * 	(ii) global variable "locknl" to correspond to the node-local of the region whose history we are storing.
 * If "locknl" is non-NULL, it is used to store the lock history. If not only then is gv_cur_region used.
 */
#define LOCK_HIST(OP, LOC, ID, CNT)					\
MBSTART {								\
	GBLREF	node_local_ptr_t	locknl;				\
									\
	int			lockidx;				\
	node_local_ptr_t	lcknl;					\
									\
	if (NULL == locknl)						\
	{								\
		assert(NULL != gv_cur_region);				\
		lcknl = FILE_INFO(gv_cur_region)->s_addrs.nl;		\
		assert(NULL != lcknl);					\
	} else								\
		lcknl = locknl;						\
	lockidx = ++lcknl->lockhist_idx;				\
	if (LOCKHIST_ARRAY_SIZE <= lockidx)				\
		lcknl->lockhist_idx = lockidx = 0;			\
	GET_LONGP(&lcknl->lockhists[lockidx].lock_op[0], (OP));		\
	lcknl->lockhists[lockidx].lock_addr = (sm_int_ptr_t)(LOC);	\
	lcknl->lockhists[lockidx].lock_callr = (caddr_t)caller_id();	\
	lcknl->lockhists[lockidx].lock_pid = (int4)(ID);		\
	lcknl->lockhists[lockidx].loop_cnt = (int4)(CNT);		\
} MBEND

#define WCS_OPS_TRACE(CSA, PID, TYPE, BLK, CR_OFF, CR_DIRTY, DETAIL1, DETAIL2)		\
MBSTART {										\
	int4			wtidx;							\
	node_local_ptr_t	cnl;							\
	boolean_t		donotflush_dbjnl;					\
											\
	assert((NULL != CSA) && (NULL != (CSA->nl)));					\
	cnl = CSA->nl;									\
	donotflush_dbjnl = cnl->donotflush_dbjnl;					\
	assert((FALSE == cnl->donotflush_dbjnl) || (TRUE == cnl->donotflush_dbjnl));	\
	wtidx = ++cnl->wcs_ops_index;							\
	if (WCS_OPS_ARRAY_SIZE <= wtidx)						\
		cnl->wcs_ops_index = wtidx = 0;						\
	cnl->wcs_ops_array[wtidx].db_tn = CSA->ti->curr_tn;				\
	cnl->wcs_ops_array[wtidx].process_id = PID;					\
	cnl->wcs_ops_array[wtidx].wc_in_free = cnl->wc_in_free;				\
	cnl->wcs_ops_array[wtidx].wcs_active_lvl = cnl->wcs_active_lvl;			\
	cnl->wcs_ops_array[wtidx].wcs_wip_lvl = cnl->wcs_wip_lvl;			\
	cnl->wcs_ops_array[wtidx].type = TYPE;						\
	cnl->wcs_ops_array[wtidx].blk = BLK;						\
	cnl->wcs_ops_array[wtidx].cr_off = CR_OFF;					\
	cnl->wcs_ops_array[wtidx].cr_dirty = CR_DIRTY;					\
	cnl->wcs_ops_array[wtidx].detail1 = DETAIL1;					\
	cnl->wcs_ops_array[wtidx].detail2 = DETAIL2;					\
	assert((FALSE == cnl->donotflush_dbjnl) || (TRUE == cnl->donotflush_dbjnl));	\
	assert(donotflush_dbjnl == cnl->donotflush_dbjnl);				\
} MBEND

#define DUMP_LOCKHIST() dump_lockhist()
#else
#define CRIT_TRACE(CSA, X)
#define	ASSERT_CURR_TN_EQUALS_EARLY_TN(csa, currtn)
#define LOCK_HIST(OP, LOC, ID, CNT)
#define WCS_OPS_TRACE(CSA, PID, TYPE, BLK, CR_OFF, CR_TN, DETAIL1, DETAIL2)
#define DUMP_LOCKHIST()
#endif

#define FTOK_TRACE(CSA, CR_TN, FTOK_OPER, PID)				\
MBSTART {								\
	node_local_ptr_t        cnl;                                    \
	int4 			foindx;					\
	assert(NULL != CSA);						\
	if (cnl = (CSA->nl))						\
	{								\
		foindx = ++cnl->ftok_ops_index;				\
		if (FTOK_OPS_ARRAY_SIZE <= cnl->ftok_ops_index)		\
			foindx = cnl->ftok_ops_index = 0;		\
		cnl->ftok_ops_array[foindx].process_id = PID;		\
		cnl->ftok_ops_array[foindx].cr_tn = CR_TN;		\
		cnl->ftok_ops_array[foindx].ftok_oper = FTOK_OPER;	\
	}								\
} MBEND

#define BT_NOT_ALIGNED(bt, bt_base)		(!IS_PTR_ALIGNED((bt), (bt_base), SIZEOF(bt_rec)))
#define BT_NOT_IN_RANGE(bt, bt_lo, bt_hi)	(!IS_PTR_IN_RANGE((bt), (bt_lo), (bt_hi)))

#define MIN_SLEEP_CNT				0		/* keep this in sync with any minseg("SLEEP_CNT) in gdeinit.m */
#define MAX_SLEEP_CNT				E_6		/* keep this in sync with any maxseg("SLEEP_CNT") in gdeinit.m */
#define DEFAULT_SLEEP_CNT			0		/* keep this in sync with any tmpseg("SLEEP_CNT") in gdeget.m */
#define SLEEP_SPIN_CNT(CSD)			(CSD)->mutex_spin_parms.mutex_sleep_spin_count
#define MAX_SPIN_SLEEP_MASK			0x3FFFFFFF
#define SPIN_SLEEP_MASK(CSD)			(CSD)->mutex_spin_parms.mutex_spin_sleep_mask
#define HARD_SPIN_COUNT(CSD)			(CSD)->mutex_spin_parms.mutex_hard_spin_count
#define MIN_CRIT_ENTRY				64		/* keep this in sync with gdeinit.m minseg("MUTEX_SLOTS") */
#define MAX_CRIT_ENTRY				32768		/* keep this in sync with gdeinit.m maxseg("MUTEX_SLOTS") */
#define DEFAULT_NUM_CRIT_ENTRY			1024		/* keep this in sync with gdeget.m tmpseg("MUTEX_SLOTS") */
#define NUM_CRIT_ENTRY(CSD)			(CSD)->mutex_spin_parms.mutex_que_entry_space_size
#define CRIT_SPACE(ENTRIES)			((ENTRIES) * SIZEOF(mutex_que_entry) + SIZEOF(mutex_struct))
#define JNLPOOL_CRIT_SPACE			CRIT_SPACE(DEFAULT_NUM_CRIT_ENTRY)
#define NODE_LOCAL_SIZE				(ROUND_UP(SIZEOF(node_local), OS_PAGE_SIZE))
#define NODE_LOCAL_SPACE(CSD)			(ROUND_UP(CRIT_SPACE(NUM_CRIT_ENTRY(CSD)) + NODE_LOCAL_SIZE, OS_PAGE_SIZE))
#define MIN_NODE_LOCAL_SPACE			(ROUND_UP(CRIT_SPACE(MIN_CRIT_ENTRY) + NODE_LOCAL_SIZE, OS_PAGE_SIZE))
/* In order for gtmsecshr not to pull in OTS library, NODE_LOCAL_SIZE_DBS is used in secshr_db_clnup instead of NODE_LOCAL_SIZE */
#define NODE_LOCAL_SIZE_DBS			(ROUND_UP(SIZEOF(node_local), DISK_BLOCK_SIZE))

#define INIT_NUM_CRIT_ENTRY_IF_NEEDED(CSD)											\
MBSTART {															\
	/* The layout of shared memory depends on the number of mutex queue entries specified in the file header. Thus in	\
	 * order to set, for example, csa->critical or csa->shmpool_buffer, we need to know this number. However, this		\
	 * number can be zero if we have not yet done db_auto_upgrade. So go ahead and upgrade to the value that will		\
	 * eventually be used, which is DEFAULT_NUM_CRIT_ENTRY.									\
	 */															\
	/* Be safe in PRO and check if we need to initialize crit entries, even for GDSMV60002 and later. */			\
	if (0 == NUM_CRIT_ENTRY(CSD))												\
		NUM_CRIT_ENTRY(CSD) = DEFAULT_NUM_CRIT_ENTRY;									\
} MBEND

#define ETGENTLE  2
#define ETSLOW    8
#define ETQUICK   16
#define ETFAST    64
#define EPOCH_TAPER_TIME_PCT_DEFAULT 32
#define EPOCH_TAPER_JNL_PCT_DEFAULT 13

#define EPOCH_TAPER_IF_NEEDED(CSA, CSD, CNL, REG, DO_FSYNC, BUFFS_PER_FLUSH, FLUSH_TARGET)					\
MBSTART	{															\
	jnl_tm_t		now;												\
	uint4			epoch_vector, jnl_autoswitchlimit, jnl_space_remaining, jnl_space_taper_interval;		\
	uint4			next_epoch_time, relative_overall_taper, relative_space_taper, relative_time_taper;		\
	uint4			time_taper_interval, tmp_epoch_taper_start_dbuffs;						\
	int4			time_remaining;											\
	jnl_buffer_ptr_t 	etjb; 												\
	etjb = CSA->jnl->jnl_buff;												\
	/* Determine if we are in the time-based epoch taper */									\
	relative_time_taper = 0;												\
	JNL_SHORT_TIME(now);													\
	next_epoch_time = etjb->next_epoch_time;										\
	if (next_epoch_time > now) /* if no db updates next_epoch_time can be in the past */					\
	{															\
		time_remaining = next_epoch_time - now;										\
		/* taper during last epoch_taper_time_pct of interval */							\
		time_taper_interval = etjb->epoch_interval * CSD->epoch_taper_time_pct / 128;					\
		if ((0 <= time_remaining) && (time_remaining < time_taper_interval))						\
			relative_time_taper = MAX(MIN(129 - ((time_remaining * 128) / time_taper_interval), 128), 0);		\
	}															\
	/* Determine if we are in the journal autoswitch (space-based) epoch taper) */						\
	relative_space_taper = 0;												\
	jnl_autoswitchlimit = CSD->autoswitchlimit;										\
	jnl_space_remaining = MAX(1,jnl_autoswitchlimit - (etjb->dskaddr / DISK_BLOCK_SIZE));					\
	jnl_space_taper_interval = (jnl_autoswitchlimit * CSD->epoch_taper_jnl_pct) / 128;					\
	if (jnl_space_remaining < jnl_space_taper_interval)									\
		relative_space_taper = MAX(MIN(129 - ((jnl_space_remaining * 128) / jnl_space_taper_interval), 128), 0);	\
	relative_overall_taper =  MAX(relative_time_taper, relative_space_taper);						\
	if (relative_overall_taper)												\
	{															\
		/* This starting point only needs to be approximate so no locking is needed */					\
		if (0 == CNL->epoch_taper_start_dbuffs)										\
			CNL->epoch_taper_start_dbuffs = CNL->wcs_active_lvl;							\
		tmp_epoch_taper_start_dbuffs = MAX(1,CNL->epoch_taper_start_dbuffs); /* stable value for all calculations */	\
		if ((relative_overall_taper > 64) && (relative_overall_taper < 96)) 						\
			CNL->epoch_taper_need_fsync = TRUE;									\
		if (DO_FSYNC && (relative_overall_taper > 96) && CNL->epoch_taper_need_fsync)					\
		{														\
			CNL->epoch_taper_need_fsync = FALSE;									\
			INCR_GVSTATS_COUNTER(CSA, CNL, n_db_fsync, 1);								\
			fsync(FILE_INFO(REG)->fd);										\
		}														\
		FLUSH_TARGET = MIN(tmp_epoch_taper_start_dbuffs, MAX(1,(tmp_epoch_taper_start_dbuffs *				\
				(129 - relative_overall_taper)) / 128));							\
		if (CNL->wcs_active_lvl > FLUSH_TARGET)										\
		{														\
			if (relative_overall_taper > 96)									\
				epoch_vector =											\
					(((CNL->wcs_active_lvl - FLUSH_TARGET) * 128 / FLUSH_TARGET) > 64) ? ETFAST : ETQUICK;	\
			else if (relative_overall_taper > 64)									\
				epoch_vector =											\
					(((CNL->wcs_active_lvl - FLUSH_TARGET) * 128 / FLUSH_TARGET) > 64) ? ETQUICK : ETSLOW;	\
			else													\
				epoch_vector = (relative_overall_taper > 32) ? ETSLOW : ETGENTLE;				\
			BUFFS_PER_FLUSH = CSD->n_wrt_per_flu * epoch_vector;							\
		}														\
	}															\
	else															\
	{															\
		CNL->epoch_taper_start_dbuffs = 0;										\
		CNL->epoch_taper_need_fsync = FALSE;										\
	}															\
} MBEND

/* Define pointer types for above structures that may be in shared memory and need 64
   bit pointers. */
#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef bt_rec	*bt_rec_ptr_t;
typedef th_rec	*th_rec_ptr_t;
typedef th_index *th_index_ptr_t;
typedef mutex_struct *mutex_struct_ptr_t;
typedef mutex_spin_parms_struct *mutex_spin_parms_ptr_t;
typedef mutex_que_entry	*mutex_que_entry_ptr_t;
typedef node_local *node_local_ptr_t;

#define OLDEST_HIST_TN(CSA)		(DBG_ASSERT(CSA->hdr) DBG_ASSERT(CSA->hdr->acc_meth != dba_mm)	\
						((th_rec_ptr_t)((sm_uc_ptr_t)CSA->th_base + CSA->th_base->tnque.fl))->tn)

#define SET_OLDEST_HIST_TN(CSA, TN)	(DBG_ASSERT(CSA->hdr) DBG_ASSERT(CSA->hdr->acc_meth != dba_mm)	\
						((th_rec_ptr_t)((sm_uc_ptr_t)CSA->th_base + CSA->th_base->tnque.fl))->tn = TN)

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

#include "cdb_sc.h"

bt_rec_ptr_t bt_get(int4 block);
void dump_lockhist(void);
void wait_for_block_flush(bt_rec *bt, block_id block);

#endif
