/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc.*
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
#ifdef __linux__
#include "gtm_inet.h"
#else
#include <netinet/in.h>
GBLREF gd_addr	*gd_header;
#endif
#include "min_max.h"
#include "mdef.h"
#include "gt_timer.h"

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
	GTMSOURCE_MODE_ACTIVE
};

enum
{
	ROOTPRIMARY_UNSPECIFIED    = -1, /* if neither -PROPAGATEPRIMARY or -ROOTPRIMARY is explicitly or implicitly specified */
	PROPAGATEPRIMARY_SPECIFIED =  0, /* if -PROPAGATEPRIMARY is explicitly or implicitly specified */
	ROOTPRIMARY_SPECIFIED      =  1  /* if -ROOTPRIMARY is explicitly or implicitly specified */
};

#define SRV_ALIVE		0x0
#define SRV_DEAD		0x1
#define SRV_ERR			0x2

/* The source server can be in any one of the following states. */
typedef enum
{
	GTMSOURCE_DUMMY_STATE = 0,		/* Default state when no source server is up */
	GTMSOURCE_START,			/* Set at source server startup (in gtmsource.c) */
	GTMSOURCE_WAITING_FOR_CONNECTION,	/* Set when waiting for receiver to connect (connection got reset etc.) */
	GTMSOURCE_WAITING_FOR_RESTART,		/* Set when previous state is GTMSOURCE_WAITING_FOR_CONNECTION and
						 * connection gets established with the receiver server. */
	GTMSOURCE_SEARCHING_FOR_RESTART,	/* Set when source server scans the jnl files and determines the resend point */
	GTMSOURCE_SENDING_JNLRECS,		/* Set when source server is sending journal records across */
	GTMSOURCE_WAITING_FOR_XON,		/* Set when source server gets an XOFF from the receiver server */
	GTMSOURCE_CHANGING_MODE,		/* Set when gtmsource_local->mode gets set to PASSIVE (i.e. ACTIVE --> PASSIVE) */
	GTMSOURCE_SEND_NEW_TRIPLE,		/* Set when the source server detects that a REPL_NEW_TRIPLE message has to be
						 * sent across to the secondary before sending the next seqno. */
	GTMSOURCE_NUM_STATES
} gtmsource_state_t;

#define MAX_GTMSOURCE_POLL_WAIT	     	1000000 /* 1s in micro secs */
#define GTMSOURCE_POLL_WAIT	        (MAX_GTMSOURCE_POLL_WAIT - 1) /* micro sec, almost 1s */

#define GTMSOURCE_WAIT_FOR_RECEIVER_TO_QUIT     5 /* seconds */
#define GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN  (1000 - 1) /* ms */
#define GTMSOURCE_WAIT_FOR_JNLOPEN              10 /* ms */
#define GTMSOURCE_WAIT_FOR_JNL_RECS             1 /* ms */
#define GTMSOURCE_WAIT_FOR_SRV_START		10 /* ms */
#define GTMSOURCE_WAIT_FOR_MODE_CHANGE		(1000 - 1) /* ms, almost 1 sec */
#define GTMSOURCE_WAIT_FOR_SHUTDOWN		(1000 - 1) /* ms, almost 1 sec */
#define GTMSOURCE_WAIT_FOR_SOURCESTART		(1000 - 1) /* ms, almost 1 sec */
#define	GTMSOURCE_WAIT_FOR_FIRSTTRIPLE		(1000 - 1) /* ms, almost 1 sec */

/* Wait for a max of 2 minutes on a single region database as all the source server shutdown
 * timeouts seen so far have been on a single region database. For multi-region databases, wait
 * for a max of one and a half minute per region. If ever we see timeouts with multi-region
 * databases, this value needs to be bumped as well.
 */

#define	GTMSOURCE_MAX_SHUTDOWN_WAITLOOP	(MAX(120, (gd_header->n_regions) * 90))

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
	sm_off_t		critical_off;		/* Offset from the start of this structure to "csa->critical" in jnlpool */
	sm_off_t		filehdr_off;		/* Offset to "repl_inst_filehdr" section in jnlpool */
	sm_off_t		srclcl_array_off;	/* Offset to "gtmsrc_lcl_array" section in jnlpool */
	sm_off_t		sourcelocal_array_off;	/* Offset to "gtmsource_local_array" section in jnlpool */
	/* The offsets of the above fields in this structure should not change as "mu_rndwn_replpool" relies on that.
	 * If they change, the macro GDS_RPL_LABEL needs to be changed as well (to effect a journal pool format change).
	 */
	uint4			jnldata_base_off;	/* Journal pool offset from where journal data starts */
	uint4			jnlpool_size;		/* Available space for journal data in bytes */
	seq_num			start_jnl_seqno;	/* The sequence number with which operations started */
	volatile seq_num 	jnl_seqno;		/* Sequence number for transactions. Updated by GTM process */
	volatile seq_num	last_triple_seqno;	/* Starting seqno of the last triple in the instance file.
							 * Set to 0 if there are no triples in the instance file.
							 */
	seq_num			max_zqgblmod_seqno;	/* The current value of max of zqgblmod_seqno of all databases.
							 * Initialized at jnlpool creation time by "gtmsource_seqno_init".
							 * Set to 0 by receiver server when it receives LOSTTNCOMPLETE notification
							 * or by a MUPIP REPLIC -SOURCE -LOSTTNCOMPLETE command on this instance.
							 * Updated by "gtmsource_update_zqgblmod_seqno_and_tn" whenever it resets
							 * the database file header "zqgblmod_seqno" fields on receipt of a
							 * fetchresync rollback message.  This field is always updated while
							 * holding the ftok semaphore on the instance file.
							 */
	volatile seq_num	max_dualsite_resync_seqno;/* Resync Seqno of this instance assuming remote secondary is dualsite */
	volatile qw_off_t	early_write_addr;	/* Virtual address assuming the to-be-written jnl records are already
							 * written into the journal pool. Is equal to write_addr except in the
							 * window when the actual write takes place. */
	volatile qw_off_t	write_addr;		/* Virtual address of the next journal record to be written in the merged
							 * journal file.  Note that the merged journal may not exist.
							 * Updated by GTM process */
	uint4			write;			/* Relative offset from jnldata_base_off for the next journal record to
							 * be written. Updated by GTM process. */
	boolean_t		upd_disabled;		/* Identify whether updates are disabled or not  on the secondary */
	boolean_t		primary_is_dualsite;	/* Has meaning only if "primary_instname" is non-NULL.
							 * Set to -1 at journal pool creation time. Later set to 0 if "proto_ver"
							 * field in initial handshake message from the primary matches the
							 * REPL_PROTO_VER_THIS macro of receiver. Set to 1 if they dont match.
							 * Multiple source servers will NOT be allowed if this is set to 1.
							 */
	boolean_t		secondary_is_dualsite;	/* Has meaning only if secondary is connected. Set to FALSE at journal pool
							 * creation time. Later set to TRUE if "proto_ver" in initial handshake
							 * message from the secondary is REPL_PROTO_VER_DUALSITE. Multiple source
							 * servers will NOT be allowed to start if this is set to TRUE.
							 * This field is updated while holding the ftok lock on the instance file.
                                                         */
	volatile uint4		lastwrite_len;		/* The length of the last transaction written into the journal pool.
							 * Copied to jnldata_hdr.prev_jnldata_len before writing into the pool.
							 * Updated by GTM process. */
	boolean_t		send_losttn_complete;	/* Set to TRUE by MUPIP REPLIC -SOURCE -LOSTTNCOMPLETE command. Set to
							 * FALSE whenever a secondary that was a former root primary does a
							 * -fetchresync rollback. This value is copied over to each gtmsource_local
							 * structure whenever that slot gets used and the corresponding source
							 * server connects to the receiver.
							 */
	unsigned char		primary_instname[MAX_INSTNAME_LEN];/* Name of the primary instance this secondary instance is
								    * connected to. Set to NULL at journal pool creation time.
								    * Initialized when receiver server connects to primary.
								    * Stays NULL and has no meaning if this is a root primary.
								    */
	uint4			gtmrecv_pid;		/* Process identification of receiver server. Copy of that which is stored
							 * in the receive pool, but needed for those processes that do not
							 * attach to the receive pool (e.g. source server) but yet need this info.
							 */
	char			this_proto_ver;		/* The replication communication protocol version this side of the pipe.
							 * All "*_proto_ver" fields need to be "signed char" in order to be able
							 * to do signed comparisons of this with the macros
							 * REPL_PROTO_VER_DUALSITE (0) and REPL_PROTO_VER_UNINITIALIZED (-1) */
	bool			pool_initialized;	/* Set to TRUE only after completely finished with initialization.
							 * Anyone who does a "jnlpool_init" before this will issue a error.
							 */
	unsigned char		filler_align_16[10];	/* Needs 16 byte alignment */
} jnlpool_ctl_struct;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef jnlpool_ctl_struct 		*jnlpool_ctl_ptr_t;
typedef	struct repl_inst_hdr_struct	*repl_inst_hdr_ptr_t;
typedef struct gtmsrc_lcl_struct	*gtmsrc_lcl_ptr_t;

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

/* The following structure contains data items local to one Source Server.
 * It is maintained in the journal pool to provide for persistence across
 * instantiations of the Source Server (due to crashes).
 */
typedef struct
{
	unsigned char		secondary_instname[MAX_INSTNAME_LEN];/* Name of the secondary instance that is connected */
	uint4			gtmsource_pid;	/* Process identification of source server */
	int4			mode; 		/* Active, or Passive */
	gtmsource_state_t	gtmsource_state;/* Current state of the source server. This shared memory flag is maintained in
						 * sync with the source server specific private global variable "gtmsource_state" */
	int4			gtmsrc_lcl_array_index;	/* Index of THIS struct in the array of gtmsource_local_struct in jnlpool */
	int4			repl_zlib_cmp_level;	/* zlib compression level currently used across the replication pipe */
	char			remote_proto_ver;	/* Set to "proto_ver" field of initial receiver server handshake message
							 * All "*_proto_ver" fields need to be "signed char" in order to be able
							 * to do signed comparisons of this with the macros
							 * REPL_PROTO_VER_DUALSITE (0) and REPL_PROTO_VER_UNINITIALIZED (-1) */
	unsigned char		filler1_align_8[3];
	int4			read_state;  	/* From where to read - pool or the file(s)? */
	uint4			read; 		/* Offset relative to jnldata_base_off of the next journal record from the pool */
	qw_off_t		read_addr; 	/* Virtual address of the next journal record in the merged journal file to read */
	seq_num			read_jnl_seqno; /* Next jnl_seqno to be read (corresponds to "gtmsrc_lcl->resync_seqno") */
	seq_num			connect_jnl_seqno;	/* jnl_seqno of the instance when last connection with secondary was made.
							 * Used to determine least recently used slot for reuse. Is maintained in
							 * parallel with the same field in the corresponding "gtmsrc_lcl" structure
							 */
	int4			num_triples;		/* Index of the last triple in the instance file as known to THIS source
							 * server. Usually kept in sync with "repl_inst_filehdr->num_triples".
							 * Note that whenever a triple gets added to the instance file, the file
							 * header field gets updated right away but this gtmsource_local field
							 * gets updated in a deferred fashion whenever the source server detects
							 * (as part of sending records across) that the instance file has changed
							 * (has more triples) since it last knew.
							 */
	int4			next_triple_num;	/* Index of the triple whose "start_seqno" is "next_triple_seqno".
							 * Initialized when the source server connects to the receiver.*/
	seq_num			next_triple_seqno;	/* "start_seqno" of the triple that follows the one corresponding to the
							 * the current "gtmsource_local->read_jnl_seqno". Initialized when the
							 * source server connects to the receiver.
							 */
	seq_num			last_flush_resync_seqno;/* Value of "gtmsource_local->read_jnl_seqno" (or corresponding field
							 * "gtmsrc_lcl->resync_seqno") when corresponding gtmsrc_lcl was last
							 * flushed to disk. Helps avoid redundant flushes.
							 */
	boolean_t		send_new_triple;	/* TRUE if REPL_NEW_TRIPLE needs to be sent before next send */
	boolean_t		send_losttn_complete;	/* TRUE if a REPL_LOSTTN_COMPLETE message needs to be sent across
							 * (set by a MUPIP REPLIC -SOURCE -LOSTTNCOMPLETE command). Set to
							 * FALSE after the message gets sent.
							 */
	char			secondary_host[MAX_HOST_NAME_LEN];	/* hostname of the secondary */
	uint4			secondary_inet_addr;	/* IP address of the secondary */
	uint4			secondary_port;		/* Port at which Receiver is listening */
	boolean_t		child_server_running;	/* Set to FALSE before starting a source server;
							 * Set to TRUE by the source server process after its initialization.
							 * Signal to parent startup command to stop waiting for child.
							 */
	uint4			log_interval;	/* seqno count interval at which src server prints messages about replic traffic */
	char			log_file[MAX_FN_LEN + 1];	/* log file name */
	uint4			changelog; 		/* Change the log file or log interval */
	uint4			statslog;		/* Boolean - detailed log on/off? */
	char			statslog_file[MAX_FN_LEN + 1];	/* stats log file name */
	int4			connect_parms[GTMSOURCE_CONN_PARMS_COUNT]; /* Connect failure tries parms. Add fillers (if needed)
									    * based on GTMSOURCE_CONN_PARMS_COUNT */
	uint4			shutdown;		/* Use to communicate shutdown related values */
	int4			shutdown_time;		/* Time allowed for shutdown in seconds */
	char			filter_cmd[MAX_FILTER_CMD_LEN];	/* command to run to invoke the external filter (if needed) */
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
 *	   jnlpool_ctl_struct
 *	   mutex structure (mutex_struct) [JNLPOOL lock control]
 *	   NUM_CRIT_ENTRY * mutex_que_entry [JNLPOOL lock control]
 *	   mutex spin parms structure (mutex_spin_parms_struct) [JNLPOOL lock control]
 *	   node local structure (node_local) [JNLPOOL lock control]
 *	   repl_inst_hdr
 *	   gtmsrc_lcl[0-15]
 *	   gtmsource_local_struct[0-15]
 *	   zero or more jnlpool_trans_struct
 */

/* Push the jnldata_base_off to be aligned to (~JNL_WRT_END_MASK + 1)-byte boundary */

#define JNLPOOL_CTL_SIZE	ROUND_UP(SIZEOF(jnlpool_ctl_struct), CACHELINE_SIZE)	/* align crit semaphore at cache line */
#define	JNLPOOL_CRIT_SIZE	(CRIT_SPACE + SIZEOF(mutex_spin_parms_struct) + SIZEOF(node_local))
#define JNLDATA_BASE_OFF	(JNLPOOL_CTL_SIZE + JNLPOOL_CRIT_SIZE + REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE + GTMSOURCE_LOCAL_SIZE)

#ifdef VMS
typedef struct
{
	char			name[MAX_GSEC_KEY_LEN];
	struct dsc$descriptor_s desc;
} t_vms_shm_key;
#endif

/* the below structure has various fields that point to different sections of the journal pool */
typedef struct
{
	jnlpool_ctl_ptr_t	jnlpool_ctl;		/* pointer to the journal pool control structure */
	gd_region		*jnlpool_dummy_reg;	/* some functions need gd_region */
	gtmsource_local_ptr_t	gtmsource_local;	/* pointer to the gtmsource_local structure for this source server */
	gtmsource_local_ptr_t	gtmsource_local_array;	/* pointer to the gtmsource_local array section in the journal pool */
	repl_inst_hdr_ptr_t	repl_inst_filehdr;	/* pointer to the instance file header section in the journal pool */
	gtmsrc_lcl_ptr_t	gtmsrc_lcl_array;	/* pointer to the gtmsrc_lcl array section in the journal pool */
	sm_uc_ptr_t		jnldata_base;		/* pointer to the start of the actual journal record data */
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

/* Types of processes that can do jnlpool_init */
typedef enum
{
	GTMPROC,	/* For GT.M */
	GTMSOURCE,	/* For source server */
	GTMRECEIVE	/* For receiver server. Note this name should be different from GTMRECV which is defined to server
			 * a similar purpose in gtmrecv.h for processes that do recvpool_init.
			 */
} jnlpool_user;

typedef struct
{
	boolean_t       start;
	boolean_t	shut_down;
	boolean_t	activate;
	boolean_t	changelog;
	boolean_t       checkhealth;
	boolean_t	deactivate;
	boolean_t	jnlpool;
	boolean_t	showbacklog;
	boolean_t	statslog;
	boolean_t	stopsourcefilter;
	int4		rootprimary;	/* ROOTPRIMARY_SPECIFIED if -ROOTPRIMARY is explicitly or implicitly specified,
					 * PROPAGATEPRIMARY_SPECIFIED if -PROPAGATEPRIMARY is explicitly or implicitly specified.
					 * ROOTPRIMARY_UNSPECIFIED otherwise */
	boolean_t	instsecondary;	/* TRUE if -INSTSECONDARY is explicitly or implicitly specified, FALSE otherwise */
	boolean_t	needrestart;	/* TRUE if -NEEDRESTART was specified, FALSE otherwise */
	boolean_t	losttncomplete;	/* TRUE if -LOSTTNCOMPLETE was specified, FALSE otherwise */
	int4		cmplvl;
	int4		shutdown_time;
	int4		buffsize;
	int4		mode;
	in_addr_t       sec_inet_addr; /* 32 bits */
	int4		secondary_port;
	uint4		src_log_interval;
	int4		connect_parms[GTMSOURCE_CONN_PARMS_COUNT];
	char            filter_cmd[MAX_FILTER_CMD_LEN];
	char            secondary_host[MAX_HOST_NAME_LEN];
	char            log_file[MAX_FN_LEN + 1];
	char		secondary_instname[MAX_INSTNAME_LEN];	/* instance name specified in -INSTSECONDARY qualifier */
} gtmsource_options_t;

#define UPDATE_DUALSITE_RESYNC_SEQNO(REGION, pre_update, post_update)						\
{ /* modifies csa; uses pre_update, post_update */								\
	boolean_t	was_crit;										\
														\
	csa = &FILE_INFO(REGION)->s_addrs;									\
	if (REPL_ALLOWED(csa->hdr))										\
	{ /* Although csa->hdr->dualsite_resync_seqno is only modified by the source				\
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
		FILE_INFO(REGION)->s_addrs.hdr->dualsite_resync_seqno = post_update;				\
		if (!QWCHANGE_IS_READER_CONSISTENT(pre_update, post_update)					\
				&& FALSE == was_crit)								\
			rel_crit(REGION);									\
	}													\
}

/********** Source server function prototypes **********/
int		gtmsource(void);
boolean_t	gtmsource_is_heartbeat_overdue(time_t *now, repl_heartbeat_msg_ptr_t overdue_heartbeat);
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
int		gtmsource_jnlpool(void);
int		gtmsource_end1(boolean_t auto_shutdown);
int		gtmsource_est_conn(struct sockaddr_in *secondary_addr);
int		gtmsource_get_jnlrecs(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multiple);
int		gtmsource_get_opt(void);
int		gtmsource_ipc_cleanup(boolean_t auto_shutdown, int *exit_status, int4 *num_src_servers_running);
int		gtmsource_mode_change(int to_mode);
int		gtmsource_poll_actions(boolean_t poll_secondary);
int		gtmsource_process(void);
int		gtmsource_readfiles(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multiple);
int		gtmsource_readpool(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multiple, qw_num stop_read_at);
int		gtmsource_recv_restart(seq_num *recvd_jnl_seqno, int *msg_type, int *start_flags, boolean_t *rcvr_same_endianness);
int		gtmsource_set_lookback(void);
int		gtmsource_showbacklog(void);
int		gtmsource_shutdown(boolean_t auto_shutdown, int exit_status);
int		gtmsource_srch_restart(seq_num recvd_jnl_seqno, int recvd_start_flags);
int		gtmsource_statslog(void);
int		gtmsource_stopfilter(void);
int		gtmsource_update_zqgblmod_seqno_and_tn(seq_num resync_seqno);
void		gtmsource_end(void);
void		gtmsource_exit(int exit_status);
void		gtmsource_init_sec_addr(struct sockaddr_in *secondary_addr);
void		gtmsource_seqno_init(void);
void		gtmsource_stop(boolean_t exit);
void		gtmsource_sigstop(void);
boolean_t	jnlpool_hasnt_overflowed(jnlpool_ctl_ptr_t jctl, uint4 jnlpool_size, qw_num read_addr);
void		jnlpool_detach(void);
void		jnlpool_init(jnlpool_user pool_user, boolean_t gtmsource_startup, boolean_t *jnlpool_creator);
int		gtmsource_init_heartbeat(void);
int		gtmsource_process_heartbeat(repl_heartbeat_msg_ptr_t heartbeat_msg);
int		gtmsource_send_heartbeat(time_t *now);
int		gtmsource_stop_heartbeat(void);
void		gtmsource_flush_fh(seq_num resync_seqno);
void		gtmsource_reinit_logseqno(void);
void		gtmsource_rootprimary_init(seq_num start_seqno);
int		gtmsource_needrestart(void);
int		gtmsource_losttncomplete(void);
void		gtmsource_jnl_release_timer(TID tid, int4 interval_len, int *interval_ptr);
int		gtmsource_start_jnl_release_timer(void);
int		gtmsource_stop_jnl_release_timer(void);

#endif /* GTMSOURCE_H */
