/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMRECV_H
#define GTMRECV_H

/* Needs mdef.h, gdsfhead.h and its dependencies, and iosp.h */

#define DEFAULT_RECVPOOL_SIZE		(64 * 1024 * 1024) /* bytes */
#define DEFAULT_SHUTDOWN_TIMEOUT	30  /* seconds */
#define	MAX_FILTER_CMD_LEN		512 /* characters */
#define UPD_HELPERS_DELIM		','
#define MAX_UPD_HELPERS			128 /* Max helper process (incl. readers and writers) one instance can support */
#define MIN_UPD_HELPERS			1   /* Minimum number of helper processes, one for reading or writing */

#define DEFAULT_UPD_HELPERS		8	/* If value for -HELPERS is not specified, start these many helpers. Change
						 * DEFAULT_UPD_HELPERS_STR if you change DEFAULT_UPD_HELPERS */
#define DEFAULT_UPD_HELP_READERS	5	/* If -HELPERS is not specified, or specified as -HELPERS=,n start these many
						 * readers. Change DEFAULT_UPD_HELPERS_STR if you change DEFAULT_UPD_HELP_READERS */
#define DEFAULT_UPD_HELPERS_STR		"8,5"	/* Built as "DEFAULT_UPD_HELPERS,DEFAULT_UPD_HELP_READERS". Maintain DEFAULT for
						 * /helpers in vvms:mupip_cmd.cld in sync with DEFAULT_UPD_HELPERS_STR */

#ifdef VMS
#define MAX_GSEC_KEY_LEN		32 /* 31 is allowed + 1 for NULL terminator */
#endif

typedef enum
{
	GTMRECV_DUMMY_STATE = 0,
	GTMRECV_START,
	GTMRECV_WAITING_FOR_CONNECTION,
	GTMRECV_RECEIVING_MSGS,
	GTMRECV_WAITING_FOR_UPD_CRASH_RESTART,
	GTMRECV_WAITING_FOR_UPD_SHUT_RESTART
} gtmrecv_state_t;

enum
{
	UPDPROC_STARTED,
	UPDPROC_START,
	UPDPROC_EXISTS,
	UPDPROC_START_ERR
};

enum
{
	GTMRECV_NO_RESTART,
	GTMRECV_RCVR_RESTARTED,
	GTMRECV_UPD_RESTARTED
};

enum
{
	HELPER_REAP_NONE = 0,
	HELPER_REAP_NOWAIT,
	HELPER_REAP_WAIT
};

#define GTMRECV_WAIT_FOR_PROC_SLOTS     1 /* s */
#define GTMRECV_WAIT_FOR_UPDSTART	(1000 - 1) /* ms */
#define GTMRECV_WAIT_FOR_UPD_SHUTDOWN	10 /* ms */
#define GTMRECV_MAX_UPDSTART_ATTEMPTS   16
#define GTMRECV_WAIT_FOR_RECVSTART      (1000 - 1) /* ms */
#define	GTMRECV_WAIT_FOR_SRV_START	10 /* ms */
#define GTMRECV_REAP_HELPERS_INTERVAL	300 /* s */


#define SRV_ALIVE		0x0
#define SRV_DEAD		0x1
#define SRV_ERR			0x2

/* The exit status of checkhealth is BIT-OR of the Receiver status and the
 * Update status */
#define	RECEIVER_SRV_ALIVE		0x00
#define RECEIVER_SRV_DEAD		0x01
#define RECEIVER_CHECKHEALTH_ERR	0x02
#define UPDATE_PROC_ALIVE		0x00
#define UPDATE_PROC_DEAD		0x04
#define UPDATE_CHECKHEALTH_ERR		0x08
#define RECVPOOL_SEGMENT		'R'
#define MIN_RECVPOOL_SIZE		(1024 * 1024)

#define GTMRECV_MIN_TCP_SEND_BUFSIZE	(512)		/* anything less than this, issue a warning */
#define GTMRECV_TCP_SEND_BUFSIZE	(1024)		/* not much outbound traffic, we can live with a low limit */
#define GTMRECV_MIN_TCP_RECV_BUFSIZE	(16   * 1024)	/* anything less than this, issue a warning */
#define GTMRECV_TCP_RECV_BUFSIZE_INCR	(32   * 1024)	/* attempt to get a larger buffer with this increment */
#define GTMRECV_TCP_RECV_BUFSIZE	(1024 * 1024)	/* desirable to set the buffer size to be able to receive large chunks */

/* Note:  fields shared between the receiver and update processes
	  really need to have memory barriers or other appropriate
	  synchronization constructs to ensure changes by one
	  process are actually seen by the other process.  Cache
	  line spacing should also be taken into account.
	  Adding volatile is only a start at this.
*/

typedef struct
{
	replpool_identifier	recvpool_id;	/* Shared memory identification */
	volatile seq_num	start_jnl_seqno;/* The sequence number with which operations started. Initialized by recvr srvr */
	volatile seq_num	jnl_seqno; 	/* Sequence number of the next transaction expected to be received from source
			    	 		 * server. Updated by Receiver Server */
	seq_num			old_jnl_seqno;	/* Stores the value of jnl_seqno before it is set to 0 when upd crash/shut */
	boolean_t		std_null_coll;	/* Null collation setting for secondary, set by update process, used by recv srvr */
	uint4			recvdata_base_off; 	/* Receive pool offset from where journal data starts */
	uint4			recvpool_size; 	/* Available space for journal data in bytes */
	volatile uint4 		write;		/* Relative offset from recvdata_base_off for for the next journal record to be
						 * written. Updated by Receiver Server */
	volatile uint4		write_wrap;	/* Relative offset from recvdata_base_off where write was wrapped by recvr srvr */
	volatile uint4		wrapped;	/* Boolean, set by Receiver Server when it wraps. Reset by Update Process when it
						 * wraps. Used for detecting space used in the receive pool */
	uint4			initialized;	/* Boolean, has receive pool been initialized? */
	uint4			fresh_start;	/* Boolean, fresh_start or crash_start? */
} recvpool_ctl_struct;

/*
 * The following structure contains Update Process related data items.
 * Maintaining this structure in the Receive pool provides for
 * persistence across instantiations of the Update Process (across crashes,
 * the receive pool is preserved)
 */

typedef struct
{
	uint4		upd_proc_pid;		/* Process identification of update server */
	uint4		upd_proc_pid_prev;      /* Save for reporting old pid if we fail */
	volatile seq_num read_jnl_seqno;	/* Next jnl_seqno to be read; keep aligned at 8 byte boundary for performance */
	volatile uint4	read; 			/* Relative offset from recvdata_base_off of the next journal record to be
						 * read from the receive pool */
	volatile uint4	upd_proc_shutdown;      /* Used to communicate shutdown related values between Receiver and Update */
	volatile int4	upd_proc_shutdown_time; /* Time allowed for update process to shut down */
	volatile uint4	bad_trans;		/* Boolean, set by Update Process that it received a bad transaction record */
	volatile uint4	changelog;		/* Boolean - change the log file */
	int4		start_upd;		/* Used to communicate upd only startup values */
	boolean_t	updateresync;		/* Same as gtmrecv_options update resync */
	volatile uint4	log_interval;		/* Interval (in seqnos) at which update process logs its progress */
	char		log_file[MAX_FN_LEN + 1];
} upd_proc_local_struct;

/*
 * The following structure contains data items local to the Receiver Server,
 * but are in the Receive Pool to provide for persistence across instantiations
 * of the Receiver Server (across Receiver Server crashes, the Receive
 * Pool is preserved).
 */
typedef struct
{
	uint4		recv_serv_pid;		/* Process identification of receiver server */
	int4		lastrecvd_time;		/* unused */
	/* Data items used in communicating action qualifiers (show statistics, shutdown) and
	 * qualifier values (log file, shutdown time, etc). */
	volatile uint4	statslog;		/* Boolean - detailed log on/off? */
	volatile uint4	shutdown;		/* Used to communicate shutdown related values between process initiating shutdown
					 	 * and Receiver Server */
	int4		shutdown_time;		/* Time allowed for shutdown in seconds */
	int4		listen_port;		/* Port at which the Receiver Server is listening */
	volatile uint4	restart;		/* Used by receiver server to coordinate crash restart with update process */
	volatile uint4	changelog;		/* Boolean - change the log file */
	volatile uint4	log_interval;		/* Interval (in seqnos) at which receiver logs its progress */
	char		filter_cmd[MAX_FILTER_CMD_LEN];	/* Receiver filters incoming records using this process */
	char		log_file[MAX_FN_LEN + 1];	/* File to log receiver progress */
	char		statslog_file[MAX_FN_LEN + 1];	/* File to log statistics */
} gtmrecv_local_struct;

#ifdef VMS
typedef struct
{
	char			name[MAX_GSEC_KEY_LEN];
	struct dsc$descriptor_s desc;
	char			filler[3];
} vms_shm_key;
#endif

/*
 * The following structure contains data items local to the Update Helpers,
 * but are in the Receive Pool to provide for persistence across instantiations
 * of the Helpers (the Receive Pool is preserved across helper crashes).
 */

typedef struct
{
	uint4		helper_pid;	/* Owner of this entry. Non-zero indicates entry occupied */
	uint4		helper_pid_prev;/* Copy of helper_pid, used to recognize helpers that are now gone and salvage entries */
	uint4		helper_type;	/* READER or WRITER */
	volatile uint4	helper_shutdown;/* used to communicate to the helpers to shut down */
} upd_helper_entry_struct;

typedef struct
{
	global_latch_t		pre_read_lock;		/* operated by pre-readers. Used to control access to next_read_offset */
	volatile uint4		pre_read_offset;	/* updated by updproc, read-only by pre-readers */
	volatile boolean_t	first_done;		/* pre-readers use this to elect ONE that computes where to begin/resume */
	volatile uint4		next_read_offset;	/* offset in recvpool of the next record to be pre-read by pre-readers */
	uint4			start_helpers;		/* TRUE: receiver to start helpers, FALSE: receiver finished helper start */
	uint4			start_n_readers;	/* start/started these many readers */
	uint4			start_n_writers;	/* start/started these many writers */
	uint4			reap_helpers;		/* receiver to salvage slots vacated by dead helpers */
	upd_helper_entry_struct	helper_list[MAX_UPD_HELPERS];	/* helper information */
} upd_helper_ctl_struct;

/*
 * Receive pool shared memory layout -
 *
 * recvpool_ctl_struct
 * upd_proc_local_struct
 * gtmrecv_local_struct
 * upd_helper_ctl_struct
 * zero or more journal records
 */

#define RECVPOOL_CTL_SIZE	ROUND_UP(SIZEOF(recvpool_ctl_struct),   CACHELINE_SIZE)
#define UPD_PROC_LOCAL_SIZE	ROUND_UP(SIZEOF(upd_proc_local_struct), CACHELINE_SIZE)
#define GTMRECV_LOCAL_SIZE	ROUND_UP(SIZEOF(gtmrecv_local_struct),  CACHELINE_SIZE)
#define UPD_HELPER_CTL_SIZE	ROUND_UP(SIZEOF(upd_helper_ctl_struct), CACHELINE_SIZE)

#define RECVDATA_BASE_OFF	ROUND_UP(RECVPOOL_CTL_SIZE + UPD_HELPER_CTL_SIZE + GTMRECV_LOCAL_SIZE + UPD_HELPER_CTL_SIZE, \
						JNL_REC_START_BNDRY)

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef recvpool_ctl_struct	*recvpool_ctl_ptr_t;
typedef upd_proc_local_struct	*upd_proc_local_ptr_t;
typedef gtmrecv_local_struct	*gtmrecv_local_ptr_t;
typedef	upd_helper_entry_struct	*upd_helper_entry_ptr_t;
typedef	upd_helper_ctl_struct	*upd_helper_ctl_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

typedef struct
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	sm_uc_ptr_t		recvdata_base;
#ifdef UNIX
	gd_region		*recvpool_dummy_reg;
#elif VMS
	int4			shm_range[2];
	int4			shm_lockid;
	vms_shm_key		vms_recvpool_key;
#endif
} recvpool_addrs;

typedef enum
{
	UPDPROC,
	UPD_HELPER_READER,
	UPD_HELPER_WRITER,
	GTMRECV
#ifdef VMS
	, GTMRECV_CHILD
#endif
} recvpool_user;

typedef struct
{
	boolean_t	start;
	boolean_t	shut_down;
	boolean_t	checkhealth;
	boolean_t	statslog;
	boolean_t	showbacklog;
	boolean_t	updateonly;
	boolean_t	stopsourcefilter;
	boolean_t	changelog;
	int4		buffsize;
	int4		shutdown_time;
	int4		listen_port;
	boolean_t	updateresync;
	uint4		rcvr_log_interval;
	uint4		upd_log_interval;
	boolean_t	helpers;
	int4		n_readers;
	int4		n_writers;
	char            log_file[MAX_FN_LEN + 1];
	char            filter_cmd[MAX_FILTER_CMD_LEN];
} gtmrecv_options_t;

#include "gtm_inet.h"

/********** Receiver server function prototypes **********/
int	gtmrecv(void);
int	gtmrecv_changelog(void);
int	gtmrecv_checkhealth(void);
int	gtmrecv_comm_init(in_port_t port);
int	gtmrecv_end1(boolean_t auto_shutdown);
int	gtmrecv_endupd(void);
void	gtmrecv_end(void);
int	gtmrecv_get_opt(void);
int	gtmrecv_poll_actions1(int *pending_data_len, int *buff_unprocessed, unsigned char *buffp);
int	gtmrecv_poll_actions(int pending_data_len, int buff_unprocessed, unsigned char *buffp);
void	gtmrecv_process(boolean_t crash_restart);
int	gtmrecv_showbacklog(void);
int	gtmrecv_shutdown(boolean_t auto_shutdown, int exit_status);
void	gtmrecv_sigstop(void);
void	gtmrecv_autoshutdown(void);
int	gtmrecv_statslog(void);
int	gtmrecv_ipc_cleanup(boolean_t auto_shutdown, int *exit_status);
int	gtmrecv_start_updonly(void);
int	gtmrecv_upd_proc_init(boolean_t fresh_start);
int	gtmrecv_wait_for_detach(void);
void	gtmrecv_exit(int exit_status);
int	gtmrecv_alloc_msgbuff(void);
void	gtmrecv_free_msgbuff(void);
int	gtmrecv_alloc_filter_buff(int bufsiz);
void	gtmrecv_free_filter_buff(void);
int	is_updproc_alive(void);
int	is_srv_alive(int srv_type);
int	is_recv_srv_alive(void);
void	recvpool_init(recvpool_user pool_user, boolean_t gtmrecv_startup, boolean_t lock_opt_sem);
void	gtmrecv_reinit_logseqno(void);
int	gtmrecv_helpers_init(int n_readers, int n_writers);
int	gtmrecv_start_helpers(int n_readers, int n_writers);
void	gtmrecv_reap_helpers(boolean_t wait);
int	gtmrecv_end_helpers(boolean_t is_rcvr_srvr);

#endif
