/****************************************************************
 *								*
 *	Copyright 2009, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef DB_SNAPSHOT_H
#define DB_SNAPSHOT_H

#include "gtm_limits.h"

#define		SNAPSHOT_HDR_LABEL		"SNAPSHOTV1"

#define SET_FAST_INTEG(X)	((X)->proc_property |= 0x0001)
#define SET_NORM_INTEG(X)	((X)->proc_property &= 0xfffe)
#define FASTINTEG_IN_PROG(X)	((X)->proc_property & 0x0001)

/* Possible states when snapshot is being init'ed */
typedef enum
{
	SNAPSHOT_NOT_INITED = 0,
	SNAPSHOT_SHM_ATTACH_FAIL,
	SHADOW_FIL_OPEN_FAIL,
	BEFORE_SHADOW_FIL_CREAT,
	AFTER_SHADOW_FIL_CREAT,
	AFTER_SHM_CREAT,
	SNAPSHOT_INIT_DONE,
	SS_NUM_PROC_STATUS
} ss_proc_status;

typedef struct snapshot_info_struct
{
	uint4		ss_pid;				/* PID of the process doing the snapshot */
	trans_num	snapshot_tn;			/* Transaction number at which the snapshot started */
	uint4		db_blk_size; 			/* Database block size */
	uint4		free_blks;			/* Free blocks at the time of snapshot */
	uint4		total_blks;			/* Total blocks at the time of snapshot */
	char		shadow_file[GTM_PATH_MAX];	/* Temporary file that will contain the before images */
	int4		shadow_vbn;			/* Starting VBN of the shadow file */
	long		ss_shmid;			/* Shared memory identifier created by snapshot initiating process */
	int		ss_shmsize;			/* Size of the shared memory newly created by snapshot initiating process */
} snapshot_info_t;

typedef struct shm_snapshot_struct
{
	snapshot_info_t	ss_info;
	volatile uint4	failure_errno;		/* the status code of the failure. */
	volatile pid_t	failed_pid;		/* the process_id that encountered the snapshot failure */
	int		in_use;
	boolean_t	preserve_snapshot;
	global_latch_t	bitmap_latch;		/* latch to be passed on to add_inter while modifying the shadow bitmap */
	trans_num	ss_tn_count;		/* count of transactions after the snapshot started */
} shm_snapshot_t;

typedef shm_snapshot_t		*shm_snapshot_ptr_t;

/* Utilities (MUPIP, DSE, LKE) specific snapshot structure */
typedef struct util_snapshot_struct
{
	unsigned char		*master_map;
	sgmnt_data_ptr_t	header;
	gtm_uint64_t		native_size;
} util_snapshot_t;

typedef	struct snapshot_context_struct
{
	int			shdw_fd;		/* open file descriptor for the shadow file */
	long			nl_shmid;
	long			attach_shmid;
	int			ss_shmcycle;
	uint4			total_blks;
	uint4			failure_errno;
	shm_snapshot_ptr_t	ss_shm_ptr;
	sm_uc_ptr_t		start_shmaddr;
	void			*bitmap_addr;
	int4			shadow_vbn;
	char			shadow_file[GTM_PATH_MAX];
	ss_proc_status		cur_state;
	/* the property of process triggering snapshot. At present only last bit is used to indicate property of integ process:
         * 0x0000: Normal Integ, 0x0001:Fast Integ
	 */
	uint4			proc_property;
} snapshot_context_t;

typedef struct snapshot_filehdr_struct
{
	char		label[SIZEOF(SNAPSHOT_HDR_LABEL) - 1];
	snapshot_info_t	ss_info;
	int4		shadow_file_len;
	unsigned char	filler[976];
} snapshot_filhdr_t;

typedef	snapshot_filhdr_t	*snapshot_filhdr_ptr_t;
typedef util_snapshot_t		*util_snapshot_ptr_t;
typedef snapshot_context_t	*snapshot_context_ptr_t;

#define DEFAULT_INIT_SS_CTX(lcl_ss_ctx)					\
{									\
	lcl_ss_ctx->shdw_fd = FD_INVALID;				\
	lcl_ss_ctx->nl_shmid = INVALID_SHMID;				\
	lcl_ss_ctx->attach_shmid = INVALID_SHMID;			\
	lcl_ss_ctx->ss_shmcycle = 0;					\
	lcl_ss_ctx->total_blks = 0;					\
	lcl_ss_ctx->failure_errno = 0;					\
	lcl_ss_ctx->ss_shm_ptr = NULL;					\
	lcl_ss_ctx->start_shmaddr = lcl_ss_ctx->bitmap_addr = NULL;	\
	lcl_ss_ctx->shadow_vbn = 0;					\
	memset(lcl_ss_ctx->shadow_file, 0, GTM_PATH_MAX);		\
	lcl_ss_ctx->cur_state = SNAPSHOT_NOT_INITED;			\
}

#define SS_DEFAULT_INIT_INFO(ss_ptr)				\
{								\
	ss_ptr->ss_info.ss_pid = 0;				\
	ss_ptr->ss_info.snapshot_tn = 0;			\
	ss_ptr->ss_info.db_blk_size = 0;			\
	ss_ptr->ss_info.free_blks = 0;				\
	ss_ptr->ss_info.total_blks = 0;				\
	memset(ss_ptr->ss_info.shadow_file, 0, GTM_PATH_MAX);	\
	ss_ptr->ss_info.ss_shmid = INVALID_SHMID;		\
	ss_ptr->ss_info.ss_shmsize = 0;				\
}

#define SS_DEFAULT_INIT_POOL(ss_shm_ptr)		\
{							\
	SS_DEFAULT_INIT_INFO(ss_shm_ptr);		\
	ss_shm_ptr->failure_errno = 0;			\
	ss_shm_ptr->failed_pid = 0;			\
	ss_shm_ptr->in_use = 0;				\
	ss_shm_ptr->preserve_snapshot = FALSE;		\
	ss_shm_ptr->ss_tn_count = 0;			\
}

#define 	MAX_SNAPSHOTS			1
#define		SNAPSHOT_HDR_SIZE		SIZEOF(snapshot_filhdr_t)
#define		SINGLE_SHM_SNAPSHOT_SIZE	ROUND_UP(SIZEOF(shm_snapshot_t), OS_PAGE_SIZE)
#define		SS_CTX_CAST(X)			((snapshot_context_ptr_t)(X))
#define		BLKS_PER_WORD			32
#define		SS_GETSTARTPTR(CSA)		(((sm_uc_ptr_t)CSA->shmpool_buffer) + SHMPOOL_BUFFER_SIZE)
#define		SS_IDX2ABS(CSA, N)		((shm_snapshot_ptr_t)(SS_GETSTARTPTR(CSA) + SNAPSHOT_SECTION_SIZE))
#ifdef DEBUG
#	define	DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(CSA, PTR)		\
		assert((PTR) >= SS_GETSTARTPTR(CSA) && ((PTR) < SS_GETSTARTPTR(CSA) + SHMPOOL_SECTION_SIZE))
#else
#	define	DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(CSA, PTR)
#endif

#ifdef GTM_SNAPSHOT
#	define	SNAPSHOT_SECTION_SIZE	(ROUND_UP((MAX_SNAPSHOTS * SINGLE_SHM_SNAPSHOT_SIZE), OS_PAGE_SIZE))
#else
#	define	SNAPSHOT_SECTION_SIZE	0
#endif
#define SHMPOOL_SECTION_SIZE	(ROUND_UP((SHMPOOL_BUFFER_SIZE + SNAPSHOT_SECTION_SIZE), OS_PAGE_SIZE))

boolean_t	ss_initiate(gd_region *, util_snapshot_ptr_t, snapshot_context_ptr_t *, boolean_t, char *);

void		ss_release(snapshot_context_ptr_t *);

boolean_t 	ss_get_block(sgmnt_addrs *, block_id, sm_uc_ptr_t);

void		ss_read_block(snapshot_context_ptr_t, block_id, sm_uc_ptr_t);

boolean_t	ss_write_block(sgmnt_addrs *, block_id, cache_rec_ptr_t, sm_uc_ptr_t, snapshot_context_ptr_t);

void		ss_set_shdw_bitmap(sgmnt_addrs *, snapshot_context_ptr_t, block_id);

boolean_t	ss_chk_shdw_bitmap(sgmnt_addrs *, snapshot_context_ptr_t, block_id);

boolean_t	ss_create_context(snapshot_context_ptr_t, int);

boolean_t	ss_destroy_context(snapshot_context_ptr_t );

void		ss_anal_shdw_file(char *, int);

void		ss_initiate_call_on_signal(void);

#endif
