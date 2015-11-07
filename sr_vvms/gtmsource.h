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

#ifndef GTMSOURCE_H
#define GTMSOURCE_H

/* for in_addr_t typedef on Linux */
#include "gtm_inet.h"
#include "gtm_ipv6.h"
#include <gtm_socket.h>

/* Needs mdef.h, gdsfhead.h and its dependencies */
#define JNLPOOL_DUMMY_REG_NAME		"JNLPOOL_REG"
#define MAX_FILTER_CMD_LEN		512
#define MIN_JNLPOOL_SIZE		(1 * 1024 * 1024)

#ifdef VMS
#define MAX_GSEC_KEY_LEN		32 /* 31 is allowed + 1 for NULL terminator */
#endif

enum
{
	READ_POOL,
	READ_FILE
};

enum
{
	GTMSOURCE_MODE_PASSIVE,
	GTMSOURCE_MODE_ACTIVE,
	GTMSOURCE_MODE_PASSIVE_REQUESTED,
	GTMSOURCE_MODE_ACTIVE_REQUESTED
};

#define SRV_ALIVE		0x0
#define SRV_DEAD		0x1
#define SRV_ERR			0x2

typedef enum
{
	GTMSOURCE_DUMMY_STATE = 0,
	GTMSOURCE_START,
	GTMSOURCE_WAITING_FOR_CONNECTION,
	GTMSOURCE_WAITING_FOR_RESTART,
	GTMSOURCE_SEARCHING_FOR_RESTART,
	GTMSOURCE_SENDING_JNLRECS,
	GTMSOURCE_WAITING_FOR_XON,
	GTMSOURCE_CHANGING_MODE
} gtmsource_state_t;

#define GTMSOURCE_WAIT_FOR_RECEIVER_TO_QUIT     5 /* seconds */
#define GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN  (1000 - 1) /* ms */
#define GTMSOURCE_WAIT_FOR_JNLOPEN              10 /* ms */
#define GTMSOURCE_WAIT_FOR_JNL_RECS             1 /* ms */
#define GTMSOURCE_WAIT_FOR_SRV_START		10 /* ms */
#define GTMSOURCE_WAIT_FOR_MODE_CHANGE	(1000 - 1) /* ms, almost 1 s */
#define GTMSOURCE_WAIT_FOR_SHUTDOWN	(1000 - 1) /* ms, almost 1 s */
#define GTMSOURCE_WAIT_FOR_SOURCESTART	(1000 - 1) /* ms */

#define GTMSOURCE_SHUTDOWN_PAD_TIME		5 /* seconds */

#define GTMSOURCE_SENT_THRESHOLD_FOR_RECV	(1 * 1024 * 1024)
#define BACKLOG_BYTES_THRESHOLD			(1 * 1024 * 1024)
#define BACKLOG_COUNT_THRESHOLD			(10 * 1024)

#define GTMSOURCE_MIN_TCP_SEND_BUFSIZE	(16   * 1024)	/* anything less than this, issue a warning */
#define GTMSOURCE_TCP_SEND_BUFSIZE_INCR	(32   * 1024)	/* attempt to get a larger buffer with this increment */
#define GTMSOURCE_TCP_SEND_BUFSIZE	(1024 * 1024)	/* desirable to set the buffer size to be able to send large chunks */
#define GTMSOURCE_MIN_TCP_RECV_BUFSIZE	(512)		/* anything less than this, issue a warning */
#define GTMSOURCE_TCP_RECV_BUFSIZE	(1024)		/* not much inbound traffic, we can live with a low limit */

#define GTMSOURCE_FH_FLUSH_INTERVAL	60 /* seconds, if required, flush file header(s) every these many seconds */

typedef struct
{ /* IMPORTANT : all fields that are used by the source server reading from pool logic must be defined VOLATILE to avoid compiler
   * optimization, forcing fresh load on every access */
	replpool_identifier	jnlpool_id;
	seq_num			start_jnl_seqno;	/* The sequence number with which operations started */
	volatile seq_num 	jnl_seqno; 		/* Sequence number for transactions. Updated by GTM process */
	uint4			jnldata_base_off;	/* Journal pool offset from where journal data starts */
	uint4			jnlpool_size; 		/* Available space for journal data in bytes */
	uint4			write; 			/* Relative offset from jnldata_base_off for the next journal record to
							 * be written. Updated by GTM process. */
	volatile uint4		lastwrite_len;  	/* The length of the last transaction written into the journal pool.
							 * Copied to jnldata_hdr.prev_jnldata_len before writing into the pool.
							 * Updated by GTM process. */
	volatile qw_off_t	early_write_addr;	/* Virtual address assuming the to-be-written jnl records are already
							 * written into the journal pool. Is equal to write_addr except in the
							 * window when the actual write takes place. */
	volatile qw_off_t	write_addr;		/* Virtual address of the next journal record to be written in the merged
							 * journal file.  Note that the merged journal may not exist.
			     	 			 * Updated by GTM process */
	boolean_t		upd_disabled;		/* Identify whether updates are disabled or not  on the secondary */
	volatile jnl_tm_t	prev_jnlseqno_time;	/* To ensure that time never decreases across successive jnl records
							 * across ALL replicated regions attached to this journal pool.
							 */
} jnlpool_ctl_struct;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef jnlpool_ctl_struct 	*jnlpool_ctl_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

/*
 * When a GTM process writes journal data into the journal pool, it uses the
 * following layout at location write
 *
 * struct jnlpool_trans_struct
 * {
 *	jnldata_hdr_struct	jnldata_hdr; 	- jnldata_hdr.jnldata_len
 *						  is the length of journal
 *						  data of a transaction
 * 	uchar			jnldata[jnldata_len]; 	- transaction journal
 * 							  data
 * };
 */

/********************** VERY IMPORTANT ********************************
 * Keep SIZEOF(jnldata_hdr_struct) == ~JNL_WRT_END_MASK + 1 and
 * jnlpool_size should be a multiple of (~JNL_WRT_END_MASK + 1).
 * This is to avoid jnldata_hdr_struct from wrapping, so that fields
 * remain contiguous.
 **********************************************************************/
typedef struct
{
	uint4 		jnldata_len;	/* length of the journal data of a
					 * a transaction in bytes */
	uint4		prev_jnldata_len; /* length of the journal data of
					   * the previous transaction in the
					   * journal pool (in bytes) */
} jnldata_hdr_struct;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef jnldata_hdr_struct 	*jnldata_hdr_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

#define REPL_CONN_HARD_TRIES_COUNT		5	/* Default number of connection hard tries */
#define REPL_CONN_HARD_TRIES_PERIOD		500	/* msec Default connection hard try period */
#define REPL_CONN_SOFT_TRIES_PERIOD		5	/* sec Default connection soft try period*/
#define REPL_CONN_ALERT_ALERT_PERIOD		30	/* sec Default alert period*/
#define REPL_CONN_HEARTBEAT_PERIOD		15	/* sec Default heartbeat period */
#define REPL_CONN_HEARTBEAT_MAX_WAIT		60	/* sec Default heartbeat maximum waiting period */
#define REPL_MAX_CONN_HARD_TRIES_PERIOD		1000 /* ms */

#define REPL_MAX_LOG_PERIOD					150 /*sec Maximum logging period */

enum
{
	GTMSOURCE_CONN_HARD_TRIES_COUNT = 0,
	GTMSOURCE_CONN_HARD_TRIES_PERIOD,
	GTMSOURCE_CONN_SOFT_TRIES_PERIOD,
	GTMSOURCE_CONN_ALERT_PERIOD,
	GTMSOURCE_CONN_HEARTBEAT_PERIOD,
	GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT,
	GTMSOURCE_CONN_PARMS_COUNT
};

#define	GTMSOURCE_CONN_PARMS_DELIM	","
#define JNLPOOL_SEGMENT			'J'

/*
 * The following structure contains data items local to the Source Server.
 * It is maintained in the journal pool to provide for persistence across
 * instantiations of the Source Server (due to crashes).
 */
typedef struct
{
	uint4		gtmsource_pid;	/* Process identification of source server */
	int4		filler1;	/* Keep read_addr 8-byte aligned */

 	/* Control fields */
	qw_off_t 	read_addr; 	/* Virtual address of the next journal record in the merged journal file to read */
	seq_num		read_jnl_seqno; /* Next jnl_seqno to be read */
	uint4		read; 		/* Offset relative to jnldata_base_off of the next journal record from the journal pool */
	int4 		read_state;  	/* From where to read - pool or the file(s)? */
	int4		mode; 		/* Active, or Passive */
	int4		lastsent_time;	/* Currently unused, Vinaya 2005/02/08 */
	seq_num		lastsent_jnl_seqno; /* Currently unsued, Vinaya 2005/02/08 */

	/* Data items used in communicating action qualifiers (show statistics, shutdown, etc) and qualifier values
	 * (log file, shutdown time, the identity of the secondary system, etc).
 	 */

	uint4		statslog;	/* Boolean - detailed log on/off? */
	uint4		shutdown;	/* Use to communicate shutdown related values */
	int4		shutdown_time;	/* Time allowed for shutdown in seconds */
	uint4		filler4;	/* Keep secondary_inet_addr aligned */
	union gtm_sockaddr_in46	secondary_inet_addr;	/* IP address of the secondary */
	int			secondary_af;		/* address family of the seconary */
	int			secondary_addrlen;	/* length of the secondary address */
	uint4		secondary_port;	/* Port at which Receiver is listening */
	uint4		changelog; 	/* change the log file or log interval */
	uint4		log_interval;	/* seqno count interval at which source server prints messages about replic traffic */
	uint4		filler2;	/* make gtmsource_local_struct size multiple of 8 bytes */
	int4		connect_parms[GTMSOURCE_CONN_PARMS_COUNT]; /* Connect failure tries parms. Add fillers (if necessary)
								    * based on GTMSOURCE_CONN_PARMS_COUNT */
	uint4		filler3;	/* extra space after connect_parms */
	char            secondary_host[MAX_HOST_NAME_LEN];
	char		filter_cmd[MAX_FILTER_CMD_LEN];
	char		log_file[MAX_FN_LEN + 1];
	char		statslog_file[MAX_FN_LEN + 1];
} gtmsource_local_struct;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef gtmsource_local_struct *gtmsource_local_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

/*
 * Journal pool shared memory layout -
 *
 * jnlpool_ctl_struct
 * mutex structure (mutex_struct) [JNLPOOL lock control]
 * NUM_CRIT_ENTRY * mutex_que_entry [JNLPOOL lock control]
 * mutex spin parms structure (mutex_spin_parms_struct) [JNLPOOL lock control]
 * node local structure (node_local) [JNLPOOL lock control]
 * gtmsource_local_struct
 * zero or more jnlpool_trans_struct
 */

/*
 * Push the jnldata_base_off to be aligned
 * to (~JNL_WRT_END_MASK + 1)-byte boundary
 */

#define JNLPOOL_CTL_SIZE	ROUND_UP(SIZEOF(jnlpool_ctl_struct), CACHELINE_SIZE) /* align crit semaphore at cache line */
#define JNLDATA_BASE_OFF	((JNLPOOL_CTL_SIZE +\
				  JNLPOOL_CRIT_SPACE +\
				  SIZEOF(mutex_spin_parms_struct) +\
				  SIZEOF(node_local) +\
				  SIZEOF(gtmsource_local_struct) +\
				  ~JNL_WRT_END_MASK) & JNL_WRT_END_MASK)
#ifdef VMS
typedef struct
{
	char			name[MAX_GSEC_KEY_LEN];
	struct dsc$descriptor_s desc;
} t_vms_shm_key;
#endif

typedef struct
{
	jnlpool_ctl_ptr_t	jnlpool_ctl;
	gd_region		*jnlpool_dummy_reg; /* some functions need gd_region */
	gtmsource_local_ptr_t	gtmsource_local;
	sm_uc_ptr_t		jnldata_base;
#ifdef VMS
	sm_uc_ptr_t		shm_range[2];
	t_vms_shm_key		vms_jnlpool_key;
	int4			shm_lockid;
#endif
} jnlpool_addrs;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef jnlpool_addrs	*jnlpool_addrs_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

typedef enum
{
	GTMPROC,
	GTMSOURCE
} jnlpool_user;

typedef struct
{
	boolean_t       start;
	boolean_t	shut_down;
	boolean_t	activate;
	boolean_t	deactivate;
	boolean_t       checkhealth;
	boolean_t	statslog;
	boolean_t	showbacklog;
	boolean_t	changelog;
	boolean_t	stopsourcefilter;
	boolean_t	update; /* A new qualifier for updates on secondary */
	int4		shutdown_time;
	int4		buffsize;
	int4		mode;
	int4		secondary_port;
	uint4		src_log_interval;
	int4		connect_parms[GTMSOURCE_CONN_PARMS_COUNT];
	char            filter_cmd[MAX_FILTER_CMD_LEN];
	char            secondary_host[MAX_HOST_NAME_LEN];
	char            log_file[MAX_FN_LEN + 1];
} gtmsource_options_t;

#define EXIT_IF_REPLOFF_JNLON(gd_header)									\
	region_top = gd_header->regions + gd_header->n_regions;							\
	for (reg = gd_header->regions; reg < region_top; reg++)							\
	{													\
		csa = &FILE_INFO(reg)->s_addrs;									\
		if (!REPL_ALLOWED(csa) && JNL_ALLOWED(csa))							\
		{												\
		 	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_REPLOFFJNLON, 2, DB_LEN_STR(reg));	\
			gtmsource_autoshutdown();								\
		}												\
	}

#define UPDATE_RESYNC_SEQNO(REGION, pre_update, post_update)							\
{ /* modifies csa, was_crit; uses pre_update, post_update */							\
	csa = &FILE_INFO(REGION)->s_addrs;									\
	if (REPL_ALLOWED(csa->hdr))										\
	{ /* Although csa->hdr->resync_seqno is only modified by the source					\
	   * server and never concurrently, it is accessed by fileheader_sync() which				\
	   * does it while in crit. To avoid the latter from reading an inconsistent				\
	   * value (i.e. neither the pre-update nor the post-update value), which is				\
	   * possible if the 8-byte operation is not atomic but a sequence of two				\
	   * 4-byte operations AND if the pre-update and post-update value differ in				\
	   * their most significant 4-bytes, we grab crit.							\
	   *													\
	   * For native INT8 platforms, we expect the compiler to optimize the "if" away.			\
	   *													\
	   * Note: the ordering of operands in the below if check is the way it is because			\
	   * a. the more frequent case is QWCHANGE_IS_READER_CONSISTENT returning TRUE.				\
	   * b. source server does not hold crit coming here (but we are covering the case			\
	   *    when it may)											\
	   */													\
		if (!QWCHANGE_IS_READER_CONSISTENT(pre_update, post_update)					\
				&& FALSE == (was_crit = csa->now_crit))						\
			grab_crit(REGION);									\
		FILE_INFO(REGION)->s_addrs.hdr->resync_seqno = post_update;					\
		if (!QWCHANGE_IS_READER_CONSISTENT(pre_update, post_update)					\
				&& FALSE == was_crit)								\
			rel_crit(REGION);									\
	}													\
}

/********** Source server function prototypes **********/
int		gtmsource(void);
boolean_t	gtmsource_is_heartbeat_overdue(time_t *now, repl_heartbeat_msg_t *overdue_heartbeat);
int		gtmsource_alloc_filter_buff(int bufsiz);
int		gtmsource_alloc_msgbuff(int maxbuffsize);
int		gtmsource_alloc_tcombuff(void);
void		gtmsource_free_filter_buff(void);
void		gtmsource_free_msgbuff(void);
void		gtmsource_free_tcombuff(void);
int		gtmsource_changelog(void);
int		gtmsource_checkhealth(void);
int		gtmsource_comm_init(void);
int		gtmsource_ctl_close(void);
int		gtmsource_ctl_init(void);
int		gtmsource_end1(boolean_t auto_shutdown);
int		gtmsource_est_conn(void);
int		gtmsource_get_jnlrecs(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multiple);
int		gtmsource_get_opt(void);
int		gtmsource_ipc_cleanup(boolean_t auto_shutdown, int *exit_status);
int		gtmsource_mode_change(int to_mode);
int		gtmsource_poll_actions(boolean_t poll_secondary);
int		gtmsource_process(void);
int		gtmsource_readfiles(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multiple);
int		gtmsource_readpool(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multiple, qw_num stop_read_at);
int		gtmsource_recv_restart(seq_num *recvd_jnl_seqno, int *msg_type, int *start_flags);
int		gtmsource_secnd_update(boolean_t print_message);
int		gtmsource_set_lookback(void);
int		gtmsource_showbacklog(void);
int		gtmsource_shutdown(boolean_t auto_shutdown, int exit_status);
int		gtmsource_srch_restart(seq_num recvd_jnl_seqno, int recvd_start_flags);
int		gtmsource_statslog(void);
int		gtmsource_stopfilter(void);
int		gtmsource_update_resync_tn(seq_num resync_seqno);
void		gtmsource_autoshutdown(void);
void		gtmsource_end(void);
void		gtmsource_exit(int exit_status);
void		gtmsource_seqno_init(void);
void		gtmsource_sigstop(void);
boolean_t	jnlpool_hasnt_overflowed(jnlpool_ctl_ptr_t jctl, uint4 jnlpool_size, qw_num read_addr);
void		jnlpool_detach(void);
void		jnlpool_init(jnlpool_user pool_user, boolean_t gtmsource_startup, boolean_t *jnlpool_initialized);
int		gtmsource_init_heartbeat(void);
int		gtmsource_process_heartbeat(repl_heartbeat_msg_t *heartbeat_msg);
int		gtmsource_send_heartbeat(time_t *now);
int		gtmsource_stop_heartbeat(void);
void		gtmsource_flush_fh(seq_num resync_seqno);
void		gtmsource_reinit_logseqno(void);

#endif /* GTMSOURCE_H */
