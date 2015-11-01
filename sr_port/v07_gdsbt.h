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

/* this requires gdsroot.h */

#include <sys/types.h>
#include <sys/mman.h>

#define CR_NOTVALID (-1)

#define WC_MAX_BUFFS 64*1024
#define WC_DEF_BUFFS 128
#define WC_MIN_BUFFS 64

#define MAX_LOCK_SPACE 1000 	/* need to change these whenever global directory defaults change */
#define MIN_LOCK_SPACE 10


#define BT_FACTOR(X) (X)
#define FLUSH_FACTOR(X) ((X)-(X)/16)
#define BT_QUEHEAD (-2)
#define BT_NOTVALID (-1)
#define BT_MAXRETRY 3
#define BT_SIZE(X) ((((sgmnt_data_ptr_t)X)->bt_buckets + 1 + ((sgmnt_data_ptr_t)X)->n_bts) * sizeof(bt_rec)) /* parameter is *sgmnt_data*/
			/* note that the + 1 above is for the th_queue head which falls between the hash table and the actual bts */
#define HEADER_UPDATE_COUNT 1024
#define WARNING_TN 4000000000
#define BUF_OWNER_STUCK 620

typedef struct
{
	trans_num	curr_tn;
	trans_num	early_tn;
	trans_num	last_mm_sync;		/* Last tn where a full mm sync was done */
	trans_num	header_open_tn;		/* Tn to be compared against jnl tn on open */
	trans_num	mm_tn;			/* Used to see if CCP must update master map */
	uint4		lock_sequence;		/* Used to see if CCP must update lock section */
	uint4		ccp_jnl_filesize;	/* Passes size of journal file if extended */
	uint4		total_blks;		/* Placed here so can be passed to other machines on cluster */
	uint4		free_blocks;
} th_index;

typedef struct
{
	struct
	{
		int4 		fl;
		int4		bl;	/* self-relative queue entry */
	} blkque, tnque;		/* for block number hash, lru queue */
	trans_num	tn;		/* transaction # #*/
	block_id	blk;		/* block #*/
	int4		cache_index;
	bool		flushing;	/* buffer is being flushed after a machine switch on a cluster */
	char		filler[3];	/* must be quad word aligned */
} bt_rec;				/* block table record */

/* This structure is used to access the transaction queue.  It points at all but the
   first two longwords of a bt_rec.  CAUTION:  there is no such thing as a queue of
   th_recs, they are always bt_recs, and the extra two longwords are always there */

typedef struct
{
	struct
	{
		int4 		fl;
		int4		bl;
	} tnque;
	trans_num	tn;
	block_id	blk;
	int4		cache_index;
	bool		flushing;
	char		filler[3];	/* must be quad word aligned */
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
		int4	fl,
			bl;
	}		que;
	int4		pid;
	void		*super_crit;
#if defined(UNIX)
	/*
	 * If the following fields are to be made part of VMS too, change
	 * size of mutex_que_entry in mutex.mar (lines 118, 152).
	 * Make sure that the size of mutex_que_entry is a multiple of 8 bytes
	 * for quadword alignment requirements of remqhi and insqti.
	 */
	int4		mutex_wake_instance;
	int4		filler1; /* for quadword alignment 		 */
#ifdef MUTEX_MSEM_WAKE
	msemaphore	mutex_wake_msem; /* Two ints (incidentally two int4s) */
#else
	int4		filler2; /* filler2 and 3 are to make size of    */
	int4		filler3; /* mutex_que_entry a power of 2 -       */
				 /* see usage of sizeof(mutex_que_entry) */
				 /* in mutex.c:crash_initialize()	 */
#endif
#endif
} mutex_que_entry;

typedef struct
{
#if defined(UNIX)
	global_latch_t         semaphore_latch;    /* 16 bytes */
	latch_t 	semaphore;
	CACHELINE_PAD(sizeof(latch_t)+sizeof(global_latch_t), 1)
	global_latch_t         wrtpnd_latch;
	ulatch_t 	wrtpnd;
	CACHELINE_PAD(sizeof(ulatch_t)+sizeof(global_latch_t), 2)
        global_latch_t         crashcnt_latch;
	latch_t 	crashcnt;
	CACHELINE_PAD(sizeof(latch_t)+sizeof(global_latch_t), 3)
        global_latch_t  mutex_q_latch;
        CACHELINE_PAD(sizeof(global_latch_t), 4)
	latch_t 	queslots;
	latch_t 	filler; /* for alignment */
#elif defined(VMS)
	short		semaphore;
	unsigned short	wrtpnd;
	short		crashcnt,
			queslots;
#else
#error UNSUPPORTED PLATFORM
#endif
	mutex_que_entry	prochead,
			freehead;
} mutex_struct;

typedef struct {
	int4	mutex_hard_spin_count;
	int4	mutex_sleep_spin_count;
	int4	mutex_spin_sleep_mask;
	int4	filler1;
} mutex_spin_parms_struct; /* Unix only */

enum crit_ops
{	crit_ops_gw = 1,	/* grab [write] crit */
	crit_ops_rw,		/* rel [write] crit */
	crit_ops_gr,		/* grab read crit */
	crit_ops_rr,		/* rel read crit */
	crit_ops_dr,		/* convert read to write */
	crit_ops_nocrit		/* did a rel_crit when now_crit flag was off */
};

typedef struct
{
	caddr_t		call_from;
	enum crit_ops	crit_act;
	int4		epid;
} crit_trace;

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

#define CRIT_OPS_ARRAY_SIZE	500
#define LOCKHIST_ARRAY_SIZE	500

/* Mapped space local to each node on the cluster */
typedef struct
{
	volatile int4	in_crit;
	int4		in_reinit;
	unsigned short	ccp_cycle;
	bool		ccp_crit_blocked;
	char		ccp_state;
	bool		ccp_jnl_closed;
	bool		glob_sec_init;
	char		filler1[2];				/* DW alignment for following */
	int4		lockhist_idx;				/* "circular" index into lockhists array */
	int4		crit_ops_index;				/* "circular" index into crit_ops_array */
	lockhist	lockhists[LOCKHIST_ARRAY_SIZE];		/* Keep lock histories here */
	crit_trace	crit_ops_array[CRIT_OPS_ARRAY_SIZE];	/* space for CRIT_TRACE macro to record info */
} node_local;

#ifdef DEBUG
#define CRIT_TRACE(X) {if (csa && csa->nl) {assert(NULL != (node_local_ptr_t)csa->nl); \
			coidx = ++csa->nl->crit_ops_index; \
			if (CRIT_OPS_ARRAY_SIZE <= coidx) coidx = csa->nl->crit_ops_index = 0; \
			csa->nl->crit_ops_array[coidx].call_from = (caddr_t)caller_id(); \
			csa->nl->crit_ops_array[coidx].epid = process_id; \
			csa->nl->crit_ops_array[coidx].crit_act = (X);}}
	/* the above macro does not use a separate semaphore to protect its maintenance of the shared memory value crit_ops_index
	 * (which would complicate precisely the situation it was created to examine)
	 * therefore, in order to to maximize the chances of gathering meaningful data,
	 * it seems better placed after grab_[read_]crit and before rel_[read_]crit,
	 * but since multiple processes can do grab_read_crit and rel_read_crit simultaneously,
	 * we will increment the index first and cache it so we can shorten our exposure window.
	 */
/*
 * The following macro places lock history entries in an array for debugging.
 * NOTE: Users of this macro, set gv_cur_region to the region whose history
 * we are storing prior to using this macro.
 */
#define LOCK_HIST(OP, LOC, ID, CNT) {int lockidx; \
			node_local_ptr_t locknl; \
			assert(NULL != gv_cur_region); \
			locknl = FILE_INFO(gv_cur_region)->s_addrs.nl; \
			assert(NULL != locknl); \
			lockidx = ++locknl->lockhist_idx; \
			if (LOCKHIST_ARRAY_SIZE <= lockidx) lockidx = locknl->lockhist_idx = 0; \
			GET_LONGP(&locknl->lockhists[lockidx].lock_op[0], (OP)); \
			locknl->lockhists[lockidx].lock_addr = (sm_int_ptr_t)(LOC); \
			locknl->lockhists[lockidx].lock_callr = (caddr_t)caller_id(); \
			locknl->lockhists[lockidx].lock_pid = (int4)(ID); \
			locknl->lockhists[lockidx].loop_cnt = (int4)(CNT);}
#define DUMP_LOCKHIST() dump_lockhist()
#else
#define CRIT_TRACE(X)
#define LOCK_HIST(OP, LOC, ID, CNT)
#define DUMP_LOCKHIST()
#endif

#define NUM_CRIT_ENTRY		512
#define CRIT_SPACE		(NUM_CRIT_ENTRY * sizeof(mutex_que_entry) + sizeof(mutex_struct) + 8)	/* +8 is not needed */
#define NODE_LOCAL_SPACE	(ROUND_UP(CRIT_SPACE + sizeof(node_local), DISK_BLOCK_SIZE))

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

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

/* Functional interface. For Unix definitions see file mutex.h */

#include "v07_cdb_sc.h"

#ifdef VMS

/*
 * Initialize a mutex with n que entries. If crash is TRUE, then this is
 * a "crash" reinitialization;  otherwise it's a "clean" initialization.
 */
void		mutex_init(mutex_struct_ptr_t addr, int n, bool crash);

/* mutex_lockr - read access to mutex at addr */
enum	cdb_sc	mutex_lockr(mutex_struct_ptr_t addr, short crash_count, uint4 *read_lock);

/* mutex_lockw - write access to mutex at addr */
enum	cdb_sc	mutex_lockw(mutex_struct_ptr_t addr, short crash_count, uint4 *write_lock);

/*
 * mutex_lockwim - write access to mutex at addr; if cannot lock,
 *                 immediately return cdb_sc_nolock
 */
enum	cdb_sc	mutex_lockwim(mutex_struct_ptr_t addr, short crash_count, uint4 *write_lock);

/*
 * mutex_lockw_ccp - write access to mutex at addr; if cannot lock,
 *                   queue CCP for "wakeup" and return
 *                   cdb_sc_nolock (do NOT hiber)
 */
enum	cdb_sc	mutex_lockw_ccp(mutex_struct_ptr_t addr, short crash_count, uint4 *write_lock, void *super_crit);

/* mutex_unlockr - unlock read access to mutex at addr */
enum	cdb_sc	mutex_unlockr(mutex_struct_ptr_t addr, short crash_count, uint4 *read_lock);

/* mutex_unlockw - unlock write access to mutex at addr */
enum	cdb_sc	mutex_unlockw(mutex_struct_ptr_t addr, short crash_count, uint4 *write_lock);

/*
 * mutex_wtor - change write access to mutex at addr to read access,
 * 	        do a wakeup
 */
enum	cdb_sc	mutex_wtor(mutex_struct_ptr_t addr, uint4 *read_lock, uint4 *write_lock);

#endif
