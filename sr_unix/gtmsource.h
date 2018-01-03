/****************************************************************
 *								*
 * Copyright (c) 2006-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "min_max.h"
#include "mdef.h"
#include "gt_timer.h"
#include "gtm_ipv6.h" /* for union gtm_sockaddr_in46 */
#include "sleep.h"

/* Needs mdef.h, gdsfhead.h and its dependencies */
#define JNLPOOL_DUMMY_REG_NAME		"JNLPOOL_REG"
#define MAX_TLSKEY_LEN			32
#define MAX_FILTER_CMD_LEN		512
#define DEFAULT_JNLPOOL_SIZE		(2 << 25)		/* 64MiB */
#define MIN_JNLPOOL_SIZE		(2 << 19)		/* 1MiB */
#define MAX_JNLPOOL_SIZE		0xFFFFFFF8LL		/* 4GiB - 8 == JNL_WRT_END_MASK */
#define MAX_FREEZE_COMMENT_LEN		1024
/* We need space in the journal pool to let other processes know which error messages should trigger anticipatory freeze.
 * Instead of storing them as a list, allocate one byte for each error message. Currently, the only piece of information
 * associated with each error message is whether it can trigger anticipatory freeze or not.
 */
#define MERRORS_ARRAY_SZ		(2 * 1024)	/* 2k should be enough for a while */

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
	GTMSOURCE_WAITING_FOR_CONNECTION,	/* Set when waiting for receiver to connect e.g. connection got reset etc. */
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

#define	GTMSOURCE_WAIT_FOR_RECEIVER_TO_QUIT	5 /* seconds */
#define	GTMSOURCE_WAIT_FOR_RECEIVER_CLOSE_CONN	(1000 - 1) /* ms */
#define	GTMSOURCE_WAIT_FOR_JNLOPEN		10 /* ms */
#define	LOG_WAIT_FOR_JNLOPEN_PERIOD		(50 * 1000) /* ms */
#define	GTMSOURCE_WAIT_FOR_JNL_RECS		1 /* ms */
#define	LOG_WAIT_FOR_JNL_RECS_PERIOD		(50 * 1000) /* ms */
#define	LOG_WAIT_FOR_JNL_FLUSH_PERIOD		(8 * 1000) /* ms */
#define	GTMSOURCE_WAIT_FOR_SRV_START		10 /* ms */
#define	GTMSOURCE_WAIT_FOR_MODE_CHANGE		(1000 - 1) /* ms, almost 1 sec */
#define	GTMSOURCE_WAIT_FOR_SHUTDOWN		(1000 - 1) /* ms, almost 1 sec */
#define	GTMSOURCE_WAIT_FOR_SOURCESTART		(1000 - 1) /* ms, almost 1 sec */
#define	GTMSOURCE_WAIT_FOR_FIRSTHISTINFO	(1000 - 1) /* ms, almost 1 sec */
#define	LOG_WAIT_FOR_JNLOPEN_TIMES		5 /* Number of times the source logs wait_for_jnlopen */

/* Wait for a max of 2 minutes on a single region database as all the source server shutdown
 * timeouts seen so far have been on a single region database. For multi-region databases, wait
 * for a max of one and a half minute per region. If ever we see timeouts with multi-region
 * databases, this value needs to be bumped as well.
 */

#define	GTMSOURCE_MAX_SHUTDOWN_WAITLOOP(gdheader)	(MAX(120, (gdheader->n_regions) * 90))

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

#define	JPL_PHASE2_COMMIT_ARRAY_SIZE	16384	/* Max # of jnlpool commits that can still be in phase2.
						 * Note that even if a phase2 commit is complete, the slot it occupies
						 * cannot be reused by anyone else until all phase2 commit slots that
						 * started before this slot are also done.
						 */
/* Individual structure describing an active phase2 jnlpool commit in jnlpool shared memory */
typedef struct
{
	seq_num		jnl_seqno;
	seq_num		strm_seqno;
	qw_off_t	start_write_addr;	/* jpl->rsrv_write_addr at start of this commit */
	uint4		process_id;
	uint4		tot_jrec_len;		/* total length of jnl records corresponding to this seqno */
	uint4		prev_jrec_len;		/* total length of jnl records corresponding to the previous seqno */
	boolean_t	write_complete;		/* TRUE if this pid is done writing jnl records to jnlbuff */
} jpl_phase2_in_prog_t;

/* Structure recording a particular type of event in this jnlpool */
typedef struct
{
	gtm_uint64_t	cntr;	/* # of times this event was encountered in the life of this jnlpool */
	seq_num		seqno;	/* seqno when this event was last encountered */
} jpl_trc_rec_t;


#define	JPL_TRACE_PRO(JPL, TRC)		{ JPL->TRC.cntr++; JPL->TRC.seqno = JPL->jnl_seqno; }

#define TAB_JPL_TRC_REC(A,B)	B,
enum jpl_trc_rec_type
{
#include "tab_jpl_trc_rec.h"
n_jpl_trc_rec_types
};
#undef TAB_JPL_TRC_REC

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
	seq_num			strm_seqno[MAX_SUPPL_STRMS];		/* the current jnl seqno of each stream */
	repl_conn_info_t	this_side;		/* Replication connection details of this side/instance */
	volatile qw_off_t	write_addr;		/* Virtual address of the next journal record to be written in the merged
							 * journal file.  Note that the merged journal may not exist.
							 * Updated by GTM process.
							 */
	volatile qw_off_t	rsrv_write_addr;	/* Similar to "write_addr" but space is reserved in phase1 of commit
							 * and actual copy to jnlpool happens in phase2 outside of jnlpool crit.
							 * If no commits are active, "rsrv_write_addr == write_addr". Else
							 * "rsrv_write_addr > write_addr".
							 */
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
	volatile uint4		onln_rlbk_pid;		/* process ID of currently running ONLINE ROLLBACK. 0 if none. */
	volatile uint4		onln_rlbk_cycle;	/* incremented everytime an ONLINE ROLLBACK ends */
	boolean_t		freeze;			/* Freeze all regions in this instance. */
	char			freeze_comment[MAX_FREEZE_COMMENT_LEN];	/* Text explaining reason for freeze */
	boolean_t		instfreeze_environ_inited;
	unsigned char		merrors_array[MERRORS_ARRAY_SZ];
	boolean_t		ftok_counter_halted;
	uint4			phase2_commit_index1;
	uint4			phase2_commit_index2;
	char			filler_16bytealign1[8];
	/************* JPL_TRC_REC RELATED FIELDS -- begin -- ***********/
#	define TAB_JPL_TRC_REC(A,B)	jpl_trc_rec_t	B;
#	include "tab_jpl_trc_rec.h"
#	undef TAB_JPL_TRC_REC
	/************* JPL_TRC_REC RELATED FIELDS -- end -- ***********/
	jpl_phase2_in_prog_t	phase2_commit_array[JPL_PHASE2_COMMIT_ARRAY_SIZE];
	CACHELINE_PAD(SIZEOF(global_latch_t), 0)	/* start next latch at a different cacheline than previous fields */
	global_latch_t		phase2_commit_latch;	/* Used by "repl_phase2_complete" to update "phase2_commit_index1" */
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

/* Macro for use before calling a routine (gtmsource_recv_ctl_nowait(), gtmsource_poll_actions()),
 * directly or indirectly, which may cause a switch to a different state. For now we only care about
 * occurrences which affect call paths through gtmsource_readfiles.
 * We treat GTMSOURCE_SEARCHING_FOR_RESTART differently than other states, as there is some special
 * handling to do. However, we only enter this state in gtmsource_recv_ctl() (which is called by
 * gtmsource_recv_ctl_nowait()) when the prior state was GTMSOURCE_WAITING_FOR_XON, as is asserted
 * there. Since we don't send transactions while in GTMSOURCE_WAITING_FOR_XON, we won't be in that
 * state in gtmsource_readfiles, so it will never be the prior state. We assert this fact before
 * saving the prior state here. Since GTMSOURCE_WAITING_FOR_XON was not the prior state at the
 * points where we save it, we can skip checking for the GTMSOURCE_SEARCHING_FOR_RESTART state below.
 * Similarly, repl_tls.renegotiate_state should not be REPLTLS_WAITING_FOR_RENEG_ACK while sending
 * transactions, so assert the fact and skip saving this state.
 */
#define GTMSOURCE_SAVE_STATE(STATEVAR)							\
MBSTART {										\
	assert(GTMSOURCE_WAITING_FOR_XON != gtmsource_state);				\
	assert(REPLTLS_WAITING_FOR_RENEG_ACK != repl_tls.renegotiate_state);		\
	STATEVAR = gtmsource_state;							\
} MBEND

/* Macros for use in a test after calling a routine (gtmsource_recv_ctl_nowait(), gtmsource_poll_actions()),
 * directly or indirectly, which may cause a switch to a different state.
 * For now we only care about occurrences which affect call paths through gtmsource_readfiles.
 * See above comments about GTMSOURCE_SEARCHING_FOR_RESTART and REPLTLS_WAITING_FOR_RENEG_ACK.
 */
#define GTMSOURCE_CHANGED_STATE(STATEVAR)	(((STATEVAR) != gtmsource_state) && ((STATEVAR) != GTMSOURCE_DUMMY_STATE))
/* By "transitional" state here we mean a state which may result from the above mentioned
 * routines when called while sending journal records which cause us to stop sending journal
 * records.
 */
#define GTMSOURCE_IS_TRANSITIONAL_STATE()								\
		((GTMSOURCE_CHANGING_MODE == gtmsource_state) 						\
			|| (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)			\
			|| (GTMSOURCE_WAITING_FOR_XON == gtmsource_state))

#define GTMSOURCE_NOW_TRANSITIONAL(STATEVAR)								\
		((GTMSOURCE_CHANGED_STATE(STATEVAR) && GTMSOURCE_IS_TRANSITIONAL_STATE())		\
			GTMTLS_ONLY(|| (REPLTLS_WAITING_FOR_RENEG_ACK == repl_tls.renegotiate_state))	\
		 	|| (GTMSOURCE_HANDLE_ONLN_RLBK == gtmsource_state))

#define	GTMSOURCE_SET_READ_ADDR(GTMSOURCE_LOCAL, JNLPOOL)						\
MBSTART {												\
	qw_off_t	rsrv_write_addr;								\
													\
	assert(JNLPOOL && JNLPOOL->jnlpool_dummy_reg && JNLPOOL->jnlpool_ctl && GTMSOURCE_LOCAL);	\
	assert((&FILE_INFO(JNLPOOL->jnlpool_dummy_reg)->s_addrs)->now_crit);          			\
	rsrv_write_addr = JNLPOOL->jnlpool_ctl->rsrv_write_addr;					\
	GTMSOURCE_LOCAL->read_addr = rsrv_write_addr;							\
	GTMSOURCE_LOCAL->read = rsrv_write_addr % JNLPOOL->jnlpool_ctl->jnlpool_size;			\
} MBEND

#define	SET_JPL_WRITE_ADDR(JPL, NEW_WRITE_ADDR)										\
{															\
	/* For systems with UNORDERED memory access (example, ALPHA, POWER4, PA-RISC 2.0), on a				\
	 * multi processor system, it is possible that the source server notices the change in				\
	 * rsrv_write_addr before seeing the change to jnlheader->jnldata_len, leading it to read an			\
	 * invalid transaction length. To avoid such conditions, we should commit the order of				\
	 * shared memory updates before we update rsrv_write_addr. This ensures that the source server			\
	 * sees all shared memory updates related to a transaction before the change in rsrv_write_addr			\
	 */														\
	SHM_WRITE_MEMORY_BARRIER;											\
	JPL->write_addr = NEW_WRITE_ADDR;										\
}

#define	SET_JNL_SEQNO(JPL, TEMP_JNL_SEQNO, SUPPLEMENTARY, STRM_SEQNO, STRM_INDEX, NEXT_STRM_SEQNO)	\
{													\
	GBLREF	jnl_gbls_t	jgbl;									\
													\
	assert(!jgbl.forw_phase_recovery);								\
	TEMP_JNL_SEQNO++;										\
	JPL->jnl_seqno = TEMP_JNL_SEQNO;								\
	if (SUPPLEMENTARY)										\
	{												\
		NEXT_STRM_SEQNO = STRM_SEQNO + 1;							\
		JPL->strm_seqno[STRM_INDEX] = NEXT_STRM_SEQNO;						\
	}												\
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
	boolean_t		jnlfileonly;		/* Current source server is only reading from journal files */
#	ifdef GTM_TLS
	uint4			next_renegotiate_time;	/* Time (in future) at which the next SSL/TLS renegotiation happens. */
	int4			num_renegotiations;	/* Number of SSL/TLS renegotiations that happened so far. */
#	endif
	int4			filler_8byte_align;	/* Keep size % 8 == 0 */
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
#define	REPLCSA2JPL(CSA)	(jnlpool_ctl_ptr_t)((sm_uc_ptr_t)CSA->critical - JNLPOOL_CTL_SIZE) /* see "jnlpool_init" for
										 * relationship between critical and jpl.
										 */
/* The below is a structure capturing the details of space reserved in the journal pool corresponding to a transaction.
 * Space for the transaction in the jnlpool is reserved in phase1 ("jnl_write_reserve"0 while holding crit and actual
 * memcpy of journal records into jnlpool happens in phase2 outside of crit ("jnl_write_phase2").
 */
typedef struct
{
	qw_off_t	start_write_addr;	/* jpl->write_addr at which this transaction started */
	qw_off_t	cur_write_addr;		/* current value of jpl->write_addr as we progress writing journal records
						 * into jnlpool in phase2 of commit.
						 */
	uint4		tot_jrec_len;		/* Total length (in bytes) of jnl records we expect to be written for this seqno */
	uint4		write_total;		/* Total length of jnl records actually used up for this seqno */
	boolean_t	memcpy_skipped;		/* we skipped "memcpy" at least once in "jnl_pool_write" */
	uint4		phase2_commit_index;	/* index into jpl->phase2_commit_array[] corresponding to this commit */
	uint4		num_tcoms;		/* If curr tn is a TP tn, then # of tcom records seen till now */
	char		filler_8byte_align[4];
} jpl_rsrv_struct_t;

/* The below structure has various fields that point to different sections of the journal pool
 * and a few fields that point to private memory.
 */
typedef struct jnlpool_addrs_struct
{
	jnlpool_ctl_ptr_t	jnlpool_ctl;		/* pointer to the journal pool control structure */
	gd_region		*jnlpool_dummy_reg;	/* some functions need gd_region */
	gtmsource_local_ptr_t	gtmsource_local;	/* pointer to the gtmsource_local structure for this source server */
	gtmsource_local_ptr_t	gtmsource_local_array;	/* pointer to the gtmsource_local array section in the journal pool */
	repl_inst_hdr_ptr_t	repl_inst_filehdr;	/* pointer to the instance file header section in the journal pool */
	gtmsrc_lcl_ptr_t	gtmsrc_lcl_array;	/* pointer to the gtmsrc_lcl array section in the journal pool */
	sm_uc_ptr_t		jnldata_base;		/* pointer to the start of the actual journal record data */
	jpl_rsrv_struct_t	jrs;
	boolean_t		pool_init;		/* this jnlpool_addrs is active */
	boolean_t		recv_pool;		/* this jnlpool is the same instance as recvpool */
	boolean_t		relaxed;		/* created with jnlpool_user GTMRELAXED */
	struct jnlpool_addrs_struct	*next;
	gd_inst_info		*gd_instinfo;		/* global directory not gtm_repl_instance */
	gd_addr			*gd_ptr;		/* pointer to global directory */
} jnlpool_addrs;

#define JNLPOOL_FROM(CSA)	(!IS_GTM_IMAGE ? jnlpool : (!(CSA) ? NULL : ((CSA)->jnlpool ? (CSA)->jnlpool : jnlpool)))

#define	UPDATE_JPL_RSRV_WRITE_ADDR(JPL, JNLPOOL, TN_JRECLEN)							\
MBSTART {													\
	qw_off_t		rsrv_write_addr;								\
	int			nextIndex, endIndex;								\
	jpl_phase2_in_prog_t	*phs2cmt;									\
														\
	GBLREF	uint4		process_id;									\
														\
	assert((NULL != JNLPOOL) && (NULL != JNLPOOL->jnlpool_dummy_reg));					\
	assert(JPL == JNLPOOL->jnlpool_ctl);									\
	assert((&FILE_INFO(JNLPOOL->jnlpool_dummy_reg)->s_addrs)->now_crit);					\
	/* Allocate a slot. But before that, check if the slot array is full.					\
	 * endIndex + 1 == first_index implies full.								\
	 * endIndex     == first_index implies empty.								\
	 */													\
	endIndex = JPL->phase2_commit_index2;									\
	nextIndex = endIndex;											\
	INCR_PHASE2_COMMIT_INDEX(nextIndex, JPL_PHASE2_COMMIT_ARRAY_SIZE);					\
	if (nextIndex == JPL->phase2_commit_index1)								\
	{	/* Slot array is full. Wait for phase2 to finish. */						\
		do												\
		{												\
			repl_phase2_cleanup(JNLPOOL);								\
			if (nextIndex != JPL->phase2_commit_index1)						\
				break;										\
			JPL_TRACE_PRO(JPL, jnl_pool_write_sleep);						\
			SLEEP_USEC(1, FALSE);									\
		} while (nextIndex == JPL->phase2_commit_index1);						\
	}													\
	ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(endIndex, JPL_PHASE2_COMMIT_ARRAY_SIZE);			\
	phs2cmt = &JPL->phase2_commit_array[endIndex];								\
	assert(JPL->jnl_seqno == jnl_fence_ctl.token);								\
	phs2cmt->jnl_seqno = jnl_fence_ctl.token;								\
	phs2cmt->strm_seqno = jnl_fence_ctl.strm_seqno;								\
	phs2cmt->process_id = process_id;									\
	rsrv_write_addr = JPL->rsrv_write_addr;									\
	phs2cmt->start_write_addr = rsrv_write_addr;								\
	assert(TN_JRECLEN);											\
	assert(0 == (TN_JRECLEN % JNL_REC_START_BNDRY));							\
	assert(NULL_RECLEN <= TN_JRECLEN);	/* see "repl_phase2_salvage" for why this is needed */		\
	phs2cmt->tot_jrec_len = TN_JRECLEN;									\
	phs2cmt->prev_jrec_len = JPL->lastwrite_len;								\
	phs2cmt->write_complete = FALSE;									\
	JNLPOOL->jrs.start_write_addr = rsrv_write_addr;								\
	JNLPOOL->jrs.cur_write_addr = rsrv_write_addr + SIZEOF(jnldata_hdr_struct);				\
	JNLPOOL->jrs.tot_jrec_len = TN_JRECLEN;									\
	JNLPOOL->jrs.write_total = SIZEOF(jnldata_hdr_struct);	/* will be incremented as we copy		\
								 * each jnlrec into jnlpool in phase2		\
								 */						\
	JNLPOOL->jrs.memcpy_skipped = FALSE;									\
	JNLPOOL->jrs.phase2_commit_index = endIndex;								\
	JNLPOOL->jrs.num_tcoms = 0;										\
	/* Note: "mutex_salvage" and "repl_phase2_cleanup" rely on the below order of sets */			\
	JPL->lastwrite_len = TN_JRECLEN;									\
	JPL->rsrv_write_addr = rsrv_write_addr + TN_JRECLEN;							\
	SHM_WRITE_MEMORY_BARRIER; /* see corresponding SHM_READ_MEMORY_BARRIER in "repl_phase2_cleanup" */	\
	JPL->phase2_commit_index2 = nextIndex;									\
} MBEND

error_def(ERR_JNLPOOLRECOVERY);

#define	JPL_PHASE2_WRITE_COMPLETE(JNLPOOL)									\
MBSTART {													\
	int			index;										\
	jpl_phase2_in_prog_t	*phs2cmt;									\
	jnldata_hdr_ptr_t	jnl_header;									\
	jrec_prefix		*prefix;									\
	uint4			tot_jrec_len;									\
														\
	GBLREF	uint4		process_id;									\
	GBLREF	jnl_gbls_t	jgbl;										\
														\
	tot_jrec_len = JNLPOOL->jrs.tot_jrec_len;								\
	assert(tot_jrec_len);											\
	index = JNLPOOL->jrs.phase2_commit_index;								\
	ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(index, JPL_PHASE2_COMMIT_ARRAY_SIZE);				\
	phs2cmt = &JNLPOOL->jnlpool_ctl->phase2_commit_array[index];						\
	assert(phs2cmt->process_id == process_id);								\
	assert(FALSE == phs2cmt->write_complete);								\
	assert(phs2cmt->tot_jrec_len == tot_jrec_len);								\
	assert(jgbl.cumul_index == jgbl.cu_jnl_index);								\
	if (!JNLPOOL->jrs.memcpy_skipped)									\
	{													\
		assert(JNLPOOL->jrs.start_write_addr >= JNLPOOL->jnlpool_ctl->write_addr);			\
		assert(JNLPOOL->jrs.start_write_addr < JNLPOOL->jnlpool_ctl->rsrv_write_addr);			\
		jnl_header = (jnldata_hdr_ptr_t)(JNLPOOL->jnldata_base						\
				+ (JNLPOOL->jrs.start_write_addr % JNLPOOL->jnlpool_ctl->jnlpool_size));		\
		jnl_header->jnldata_len = tot_jrec_len;								\
		assert(0 == (phs2cmt->prev_jrec_len % JNL_REC_START_BNDRY));					\
		jnl_header->prev_jnldata_len = phs2cmt->prev_jrec_len;						\
		DEBUG_ONLY(prefix = (jrec_prefix *)(JNLPOOL->jnldata_base					\
					+ (JNLPOOL->jrs.start_write_addr + SIZEOF(jnldata_hdr_struct))		\
							% JNLPOOL->jnlpool_ctl->jnlpool_size));			\
		assert(JRT_BAD != prefix->jrec_type);								\
		if ((JNLPOOL->jrs.write_total != tot_jrec_len)							\
			DEBUG_ONLY(|| ((0 != TREF(gtm_test_jnlpool_sync))					\
					&& (0 == (phs2cmt->jnl_seqno % TREF(gtm_test_jnlpool_sync))))))		\
		{	/* This is an out-of-sync situation. "tot_jrec_len" (computed in phase1) is not equal	\
			 * to "write_total" (computed in phase2). Not sure how this can happen but recover	\
			 * from this situation by replacing the first record in the reserved space with a	\
			 * JRT_BAD rectype. That way the source server knows this is a transaction that it	\
			 * has to read from the jnlfiles and not the jnlpool.					\
			 */											\
			assert((0 != TREF(gtm_test_jnlpool_sync))						\
					&& (0 == (phs2cmt->jnl_seqno % TREF(gtm_test_jnlpool_sync))));		\
			assert(tot_jrec_len >= (SIZEOF(jnldata_hdr_struct) + SIZEOF(jrec_prefix)));		\
			/* Note that it is possible jnl_header is 8 bytes shy of the jnlpool end in which case	\
			 * "prefix" below would end up going outside the jnlpool range hence a simple		\
			 * (jnl_header + 1) would not work to set prefix (and instead the % needed below).	\
			 */											\
			prefix = (jrec_prefix *)(JNLPOOL->jnldata_base						\
					+ (JNLPOOL->jrs.start_write_addr + SIZEOF(jnldata_hdr_struct))		\
							% JNLPOOL->jnlpool_ctl->jnlpool_size);			\
			prefix->jrec_type = JRT_BAD;								\
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLRECOVERY, 4,				\
				tot_jrec_len, JNLPOOL->jrs.write_total,						\
				&phs2cmt->jnl_seqno, JNLPOOL->jnlpool_ctl->jnlpool_id.instfilename);		\
			/* Now that JRT_BAD is set, fix cur_write_addr so it is set back in sync		\
			 * (so later assert can succeed).							\
			 */											\
			DEBUG_ONLY(JNLPOOL->jrs.cur_write_addr = (JNLPOOL->jrs.start_write_addr + tot_jrec_len));	\
		}												\
		/* Need to make sure the writes of jnl_header->jnldata_len & jnl_header->prev_jnldata_len	\
		 * happen BEFORE the write of phs2cmt->write_complete in that order. Hence need the write	\
		 * memory barrier. Not doing this could cause another process in "repl_phase2_cleanup" to	\
		 * see phs2cmt->write_complete as TRUE and update jnlpool_ctl->write_addr to reflect this	\
		 * particular seqno even though the jnl_header write has not still happened. This could cause	\
		 * a concurrently running source server to decide to read this seqno in "gtmsource_readpool"	\
		 * and read garbage lengths in the jnl_header section.						\
		 */												\
		SHM_WRITE_MEMORY_BARRIER;									\
	}													\
	assert((JNLPOOL->jrs.start_write_addr + tot_jrec_len) == JNLPOOL->jrs.cur_write_addr);			\
	phs2cmt->write_complete = TRUE;										\
	JNLPOOL->jrs.tot_jrec_len = 0;	/* reset needed to prevent duplicate calls (e.g. "secshr_db_clnup") */	\
	/* Invoke "repl_phase2_cleanup" sparingly as it calls "grab_latch". So we do it twice.			\
	 * Once at half-way mark and once when a wrap occurs.							\
	 */													\
	if (!index || ((JPL_PHASE2_COMMIT_ARRAY_SIZE / 2) == index))						\
		repl_phase2_cleanup(JNLPOOL);									\
} MBEND

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
	boolean_t	jnlfileonly;	/* TRUE if -JNLFILEONLY was specified, FALSE otherwise */
	boolean_t	zerobacklog;   	/* TRUE if -ZEROBACKLOG was specified, FALSE otherwise */
	int4		cmplvl;
	int4		shutdown_time;
	uint4		buffsize;
	int4		mode;
	int4		secondary_port;
	uint4		src_log_interval;
	int4		connect_parms[GTMSOURCE_CONN_PARMS_COUNT];
	char            filter_cmd[MAX_FILTER_CMD_LEN];
	char            secondary_host[MAX_HOST_NAME_LEN];
	char            log_file[MAX_FN_LEN + 1];
	char		secondary_instname[MAX_INSTNAME_LEN];	/* instance name specified in -INSTSECONDARY qualifier */
	char		freeze_comment[MAX_FREEZE_COMMENT_LEN];
#	ifdef GTM_TLS
	char		tlsid[MAX_TLSKEY_LEN];
	int4		renegotiate_interval;
#	endif
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
void		gtmsource_recv_ctl(void);
boolean_t	gtmsource_recv_ctl_nowait(void);
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
void		jnlpool_init(jnlpool_user pool_user, boolean_t gtmsource_startup, boolean_t *jnlpool_creator, gd_addr *gd_ptr);
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
seq_num 	gtmsource_checkforbacklog(void);
#ifdef GTM_TLS
boolean_t	gtmsource_exchange_tls_info(void);
#endif

/********** Miscellaneous prototypes **********/
void		repl_phase2_cleanup(jnlpool_addrs *jpa);
void		repl_phase2_salvage(jnlpool_addrs *jpa, jnlpool_ctl_ptr_t jpl, jpl_phase2_in_prog_t *deadCmt);

#endif /* GTMSOURCE_H */
