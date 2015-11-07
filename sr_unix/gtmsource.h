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
GBLREF gd_addr	*gd_header;
#include "min_max.h"
#include "mdef.h"
#include "gt_timer.h"
#include "gtm_ipv6.h" /* for union gtm_sockaddr_in46 */

/* Needs mdef.h, gdsfhead.h and its dependencies */
#define JNLPOOL_DUMMY_REG_NAME		"JNLPOOL_REG"
#define MAX_FILTER_CMD_LEN		512
#define MIN_JNLPOOL_SIZE		(1 * 1024 * 1024)
#define MAX_FREEZE_COMMENT_LEN		1024
/* We need space in the journal pool to let other processes know which error messages should trigger anticipatory freeze.
 * Instead of storing them as a list, allocate one byte for each error message. Currently, the only piece of information
 * associated with each error message is whether it can trigger anticipatory freeze or not.
 */
#define MERRORS_ARRAY_SZ		(2 * 1024)	/* 2k should be enough for a while */

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

enum
{
	ROOTPRIMARY_UNSPECIFIED    = -1, /* if neither -PROPAGATEPRIMARY or -ROOTPRIMARY is explicitly or implicitly specified */
	PROPAGATEPRIMARY_SPECIFIED =  0, /* if -PROPAGATEPRIMARY or -UPDNOTOK is explicitly or implicitly specified */
	ROOTPRIMARY_SPECIFIED      =  1  /* if -ROOTPRIMARY or -UPDOK is explicitly or implicitly specified */
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
	GTMSOURCE_SEND_NEW_HISTINFO,		/* Set when the source server detects it has to send a REPL_HISTREC message
						 * to the secondary before sending the next seqno. */
	GTMSOURCE_HANDLE_ONLN_RLBK,		/* Set when the source server detects an online rollback and hence has close and
						 * restart the connection
						 */
	GTMSOURCE_NUM_STATES
} gtmsource_state_t;

#define GTMSOURCE_WAIT_FOR_RECEIVER_TO_QUIT     5 /* seconds */
#define GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN  (1000 - 1) /* ms */
#define GTMSOURCE_WAIT_FOR_JNLOPEN              10 /* ms */
#define GTMSOURCE_WAIT_FOR_JNL_RECS             1 /* ms */
#define GTMSOURCE_WAIT_FOR_SRV_START		10 /* ms */
#define GTMSOURCE_WAIT_FOR_MODE_CHANGE		(1000 - 1) /* ms, almost 1 sec */
#define GTMSOURCE_WAIT_FOR_SHUTDOWN		(1000 - 1) /* ms, almost 1 sec */
#define GTMSOURCE_WAIT_FOR_SOURCESTART		(1000 - 1) /* ms, almost 1 sec */
#define	GTMSOURCE_WAIT_FOR_FIRSTHISTINFO	(1000 - 1) /* ms, almost 1 sec */
#define LOG_WAIT_FOR_JNLOPEN_TIMES		5 /* Number of times the source logs wait_for_jnlopen */

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
{ 	/* IMPORTANT : all fields that are used by the source server reading from pool logic must be defined VOLATILE to avoid
	 * compiler optimization, forcing fresh load on every access.
	 */
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
	volatile seq_num	last_histinfo_seqno;	/* Starting seqno of the last histinfo in the instance file.
							 * Set to 0 if there are no histinfo records in the instance file.
							 */
	seq_num			max_zqgblmod_seqno;	/* The current value of max of zqgblmod_seqno of all databases.
							 * Initialized at jnlpool creation time by "gtmsource_seqno_init".
							 * Set to 0 by receiver server when it receives LOSTTNCOMPLETE notification
							 * or by a MUPIP REPLIC -SOURCE -LOSTTNCOMPLETE command on this instance.
							 * Updated by "gtmsource_update_zqgblmod_seqno_and_tn" whenever it resets
							 * the database file header "zqgblmod_seqno" fields on receipt of a
							 * fetchresync rollback message.  This field is always updated while
							 * holding the journal pool lock
							 */
	volatile qw_off_t	early_write_addr;	/* Virtual address assuming the to-be-written jnl records are already
							 * written into the journal pool. Is equal to write_addr except in the
							 * window when the actual write takes place. */
	volatile qw_off_t	write_addr;		/* Virtual address of the next journal record to be written in the merged
							 * journal file.  Note that the merged journal may not exist.
							 * Updated by GTM process */
	uint4			write;			/* Relative offset from jnldata_base_off for the next journal record to
							 * be written. Updated by GTM process. */
	boolean_t		upd_disabled;		/* Identify whether updates are disabled or not  on the secondary */
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
	volatile jnl_tm_t	prev_jnlseqno_time;	/* To ensure that time never decreases across successive jnl records
							 * across ALL replicated regions attached to this journal pool.
							 */
	boolean_t		pool_initialized;	/* Set to TRUE only after completely finished with initialization.
							 * Anyone who does a "jnlpool_init" before this will issue a error.
							 */
	uint4			jnlpool_creator_pid;	/* DEBUG-ONLY field used for fake ENOSPC testing */
	repl_conn_info_t	this_side;		/* Replication connection details of this side/instance */
	seq_num			strm_seqno[MAX_SUPPL_STRMS];		/* the current jnl seqno of each stream */
	volatile uint4		onln_rlbk_pid;		/* process ID of currently running ONLINE ROLLBACK. 0 if none. */
	volatile uint4		onln_rlbk_cycle;	/* incremented everytime an ONLINE ROLLBACK ends */
	boolean_t		freeze;			/* Freeze all regions in this instance. */
	char			freeze_comment[MAX_FREEZE_COMMENT_LEN];	/* Text explaining reason for freeze */
	boolean_t		instfreeze_environ_inited;
	unsigned char		merrors_array[MERRORS_ARRAY_SZ];
	/* Note: while adding fields to this structure, keep in mind that it needs to be 16-byte aligned so add filler bytes
	 * as necessary
	 */
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
 *	jnldata_hdr_struct	jnldata_hdr; 		- jnldata_hdr.jnldata_len
 *						  	  is the length of journal
 *						  	  data of a transaction
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
	uint4 		jnldata_len;		/* length of the journal data of a transaction in bytes */
	uint4		prev_jnldata_len;	/* length of the journal data of the previous transaction in the
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
#define REPL_MAX_CONN_HARD_TRIES_PERIOD		1000    /* ms */
#define REPL_MAX_LOG_PERIOD		        150     /* sec Maximum logging period */

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

/*************** Macro to send a REPL_HISTREC message, given an histinfo type of record ***************/
/* Note that HSTINFO.start_seqno is modified by this macro */
#define	GTMSOURCE_SEND_REPL_HISTREC(HSTINFO, GTMSRCLCL, RCVR_CROSS_ENDIAN)							\
{																\
	repl_histrec_msg_t	histrec_msg;											\
																\
	memset(&histrec_msg, 0, SIZEOF(repl_histrec_msg_t));									\
	histrec_msg.type = REPL_HISTREC;											\
	histrec_msg.len = SIZEOF(repl_histrec_msg_t);										\
	histrec_msg.histjrec.jrec_type = JRT_HISTREC;										\
	/* Update history record's start_seqno to reflect the starting point of transmission */					\
	HSTINFO.start_seqno = GTMSRCLCL->read_jnl_seqno;									\
	histrec_msg.histjrec.histcontent = HSTINFO;										\
	if (RCVR_CROSS_ENDIAN && (this_side->jnl_ver < remote_side->jnl_ver))							\
	{															\
		histrec_msg.histjrec.forwptr = GTM_BYTESWAP_24(SIZEOF(repl_histrec_jnl_t));					\
		ENDIAN_CONVERT_REPL_HISTINFO(&histrec_msg.histjrec.histcontent);						\
	} else															\
		histrec_msg.histjrec.forwptr = SIZEOF(repl_histrec_jnl_t);							\
	gtmsource_repl_send((repl_msg_ptr_t)&histrec_msg, "REPL_HISTREC", GTMSRCLCL->read_jnl_seqno, INVALID_SUPPL_STRM);	\
}

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
	unsigned char		filler1_align_8[4];
	int4			read_state;  	/* From where to read - pool or the file(s)? */
	uint4			read; 		/* Offset relative to jnldata_base_off of the next journal record from the pool */
	repl_conn_info_t	remote_side;	/* Details of the remote side connection */
	qw_off_t		read_addr; 	/* Virtual address of the next journal record in the merged journal file to read */
	seq_num			read_jnl_seqno; /* Next jnl_seqno to be read (corresponds to "gtmsrc_lcl->resync_seqno") */
	seq_num			connect_jnl_seqno;	/* jnl_seqno of the instance when last connection with secondary was made.
							 * Used to determine least recently used slot for reuse. Is maintained in
							 * parallel with the same field in the corresponding "gtmsrc_lcl" structure
							 */
	int4			num_histinfo;		/* Index of the last histinfo in the instance file as known to THIS source
							 * server. Usually kept in sync with "repl_inst_filehdr->num_histinfo".
							 * Note that whenever a histinfo record gets added to the instance file,
							 * the file header field gets updated right away but this gtmsource_local
							 * field gets updated in a deferred fashion whenever the source server
							 * detects (as part of sending records across) that the instance file has
							 * changed (has more histinfo records) since it last knew.
							 */
	int4			next_histinfo_num;	/* Index of the histinfo record whose "start_seqno" is "next_histinfo_seqno"
							 * Initialized when the source server connects to the receiver.*/
	seq_num			next_histinfo_seqno;	/* "start_seqno" of the histinfo record that follows the one corresponding
							 * to the the current "gtmsource_local->read_jnl_seqno". Initialized when
							 * the source server connects to the receiver.
							 */
	seq_num			last_flush_resync_seqno;/* Value of "gtmsource_local->read_jnl_seqno" (or corresponding field
							 * "gtmsrc_lcl->resync_seqno") when corresponding gtmsrc_lcl was last
							 * flushed to disk. Helps avoid redundant flushes.
							 */
	boolean_t		send_new_histrec;	/* TRUE if REPL_HISTREC needs to be sent before next send */
	boolean_t		send_losttn_complete;	/* TRUE if a REPL_LOSTTN_COMPLETE message needs to be sent across
							 * (set by a MUPIP REPLIC -SOURCE -LOSTTNCOMPLETE command). Set to
							 * FALSE after the message gets sent.
							 */
	char			secondary_host[MAX_HOST_NAME_LEN];	/* hostname of the secondary */
	union gtm_sockaddr_in46	secondary_inet_addr;	/* IP address of the secondary */
	int			secondary_af;		/* address family of the seconary */
	int			secondary_addrlen;	/* length of the secondary address */
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
	global_latch_t		gtmsource_srv_latch;
#if 0
	int4			padding;		/* Pad structure out to multiple of 8 bytes - un-"#if 0" if needed */
#endif
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
#define	JNLPOOL_CRIT_SIZE	(JNLPOOL_CRIT_SPACE + SIZEOF(mutex_spin_parms_struct) + SIZEOF(node_local))
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
	GTMPROC,	/* For GT.M and Update Process */
	GTMSOURCE,	/* For Source Server */
	GTMRECEIVE,	/* For Receiver Server. Note this name should be different from GTMRECV which is defined to serve
			 * a similar purpose in gtmrecv.h for processes that do recvpool_init.
			 */
	GTMRELAXED,	/* For processes which want to a attach to an existing journal pool without the usual validations (currently
			 * NOJNLPOOL is the only validation that is skipped)
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
	int4		rootprimary;	/* ROOTPRIMARY_SPECIFIED if -ROOTPRIMARY or -UPDOK is explicitly or implicitly specified,
					 * PROPAGATEPRIMARY_SPECIFIED if -PROPAGATEPRIMARY or -UPDNOTOK is explicitly or
					 *	implicitly specified.
					 * ROOTPRIMARY_UNSPECIFIED otherwise */
	boolean_t	instsecondary;	/* TRUE if -INSTSECONDARY is explicitly or implicitly specified, FALSE otherwise */
	boolean_t	needrestart;	/* TRUE if -NEEDRESTART was specified, FALSE otherwise */
	boolean_t	losttncomplete;	/* TRUE if -LOSTTNCOMPLETE was specified, FALSE otherwise */
	boolean_t	showfreeze;	/* TRUE if -FREEZE was specified with no value, FALSE otherwise */
	boolean_t	setfreeze;	/* TRUE if -FREEZE was specified with a value, FALSE otherwise */
	boolean_t	freezeval;	/* TRUE for -FREEZE=ON, FALSE for -FREEZE=OFF */
	boolean_t	setcomment;	/* TRUE if -COMMENT was specified, FALSE otherwise */
	int4		cmplvl;
	int4		shutdown_time;
	int4		buffsize;
	int4		mode;
	int4		secondary_port;
	uint4		src_log_interval;
	int4		connect_parms[GTMSOURCE_CONN_PARMS_COUNT];
	char            filter_cmd[MAX_FILTER_CMD_LEN];
	char            secondary_host[MAX_HOST_NAME_LEN];
	char            log_file[MAX_FN_LEN + 1];
	char		secondary_instname[MAX_INSTNAME_LEN];	/* instance name specified in -INSTSECONDARY qualifier */
	char		freeze_comment[MAX_FREEZE_COMMENT_LEN];
} gtmsource_options_t;

/********** Source server function prototypes **********/
int		gtmsource(void);
boolean_t	gtmsource_is_heartbeat_overdue(time_t *now, repl_heartbeat_msg_ptr_t overdue_heartbeat);
int		gtmsource_alloc_filter_buff(int bufsiz);
int		gtmsource_alloc_msgbuff(int maxbuffsize, boolean_t discard_oldbuff);
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
int		gtmsource_est_conn(void);
int		gtmsource_get_jnlrecs(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multiple);
int		gtmsource_get_opt(void);
int		gtmsource_ipc_cleanup(boolean_t auto_shutdown, int *exit_status, int4 *num_src_servers_running);
int		gtmsource_mode_change(int to_mode);
int		gtmsource_poll_actions(boolean_t poll_secondary);
int		gtmsource_process(void);
int		gtmsource_readfiles(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multiple);
int		gtmsource_readpool(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multiple, qw_num stop_read_at);
int		gtmsource_recv_restart(seq_num *recvd_jnl_seqno, int *msg_type, int *start_flags);
int		gtmsource_set_lookback(void);
int		gtmsource_showbacklog(void);
int		gtmsource_shutdown(boolean_t auto_shutdown, int exit_status);
int		gtmsource_srch_restart(seq_num recvd_jnl_seqno, int recvd_start_flags);
int		gtmsource_statslog(void);
int		gtmsource_stopfilter(void);
int		gtmsource_update_zqgblmod_seqno_and_tn(seq_num resync_seqno);
void		gtmsource_end(void);
void		gtmsource_exit(int exit_status);
void		gtmsource_seqno_init(boolean_t this_side_std_null_coll);
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
void		gtmsource_onln_rlbk_clnup(void);
int		gtmsource_showfreeze(void);
int		gtmsource_setfreeze(void);

#endif /* GTMSOURCE_H */
