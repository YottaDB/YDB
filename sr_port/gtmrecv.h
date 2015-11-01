/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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

#define	MAX_FILTER_CMD_LEN	512

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

#define GTMRECV_WAIT_FOR_PROC_SLOTS     1 /* s */
#define GTMRECV_MAX_UPDSTART_ATTEMPTS   16
#define GTMRECV_WAIT_FOR_RECVSTART      (1000 - 1) /* ms */
#define	GTMRECV_WAIT_FOR_SRV_START	10 /* ms */

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
	replpool_identifier recvpool_id;
	volatile seq_num	start_jnl_seqno;	/* The sequence number with which operations
				 * started.  Initialized by receiver server */
	volatile seq_num	jnl_seqno; 	/* Sequence number of the next transaction
				 * expected to be received from Source Server.
			    	 * Updated by Receiver Server */
	seq_num	old_jnl_seqno;	/* Stores the value of jnl_seqno before it
				   is set to 0 when upd crash/shut */
	seq_num	filler_seqno;
	uint4	recvdata_base_off; /* Receive pool offset from where journal
				    * data starts */
	uint4	recvpool_size; 	/* Available space for journal data in bytes */
	volatile uint4 	write; 	/* Relative offset from recvdata_base_off for
				 * for the next journal record to be written.
				 * Updated by Receiver Server */
	volatile uint4	write_wrap;	/* Relative offset from recvdata_base_off
				 * where write was wrapped by Receiver Server */

	volatile uint4	wrapped;	/* Boolean, set by Receiver Server when it wraps
				 * Reset by Update Process when it wraps. Used
				 * for detecting space used in the receive
				 * pool */
	uint4	initialized;	/* Boolean, has receive pool been inited? */
	uint4	fresh_start;	/* Boolean, fresh_start or crash_start? */
} recvpool_ctl_struct;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef recvpool_ctl_struct	*recvpool_ctl_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

/*
 * The following structure contains Update Process related data items.
 * Maintaining this structure in the Receive pool provides for
 * persistence across instantiations of the Update Process (across crashes,
 * the receive pool is preserved)
 */
typedef struct
{
	uint4		upd_proc_pid;
	uint4		upd_proc_pid_prev;      /* save for reporting old pid if we fail */
	volatile uint4	read; 			/* Relative offset from
						 * recvdata_base_off of the
						 * next journal record to be
						 * read from the receive pool */
	volatile seq_num	read_jnl_seqno;	/* Next jnl_seqno to be read */
	volatile uint4	upd_proc_shutdown;      /* Used to communicate shutdown
						 * related values between
						 * Receiver Server and Update
						 * Process */
	volatile int4	upd_proc_shutdown_time; /* Time allowed for update
						 * process to shut down */
	volatile uint4	bad_trans;		/* Boolean, set by Update
						 * Process that it received
						 * a bad transaction record */
	volatile uint4	changelog;		/* Boolean - change the log
						   file */
	int4		start_upd;		/* Used to communicate upd only
						 * startup values */
	boolean_t	updateresync;		/* Same as gtmrecv_options update resync */
	char		log_file[MAX_FN_LEN + 1];
} upd_proc_local_struct;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef upd_proc_local_struct	*upd_proc_local_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

/*
 * The following structure contains data items local to the Receiver Server,
 * but are in the Receive Pool to provide for persistence across instantiations
 * of the Receiver Server (across Receiver Server crashes, the Receive
 * Pool is preserved).
 */
typedef struct
{
	uint4			recv_serv_pid;
	int4			primary_inet_addr; /* IP address of the
						    * primary system */
	int4			lastrecvd_time;
	uint4			filler;
	/*
 	 * Data items used in communicating action qualifiers (show statistics,
	 * shutdown) and qualifier values (log file, shutdown time, etc).
 	 */
	volatile uint4		statslog; /* Boolean - detailed log on/off? */
	volatile uint4		shutdown; /* Used to communicate shutdown
					   * related values between process
					   * initiating shutdown and Receiver
					   * Server */
	int4			shutdown_time; /* Time allowed for shutdown
						* in seconds */
	int4			listen_port;	/* Port at which the Receiver
						 * Server is listening */
	volatile uint4		restart;	/* Used by receiver server to
						 * coordinate crash restart
						 * with update process */
	volatile uint4		changelog;	/* Boolean - change the log
						 * file */
	char			filter_cmd[MAX_FILTER_CMD_LEN];
	char			log_file[MAX_FN_LEN + 1];
	char			statslog_file[MAX_FN_LEN + 1];
} gtmrecv_local_struct;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef gtmrecv_local_struct	*gtmrecv_local_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

#ifdef VMS
typedef struct
{
	char			name[MAX_GSEC_KEY_LEN];
	struct dsc$descriptor_s desc;
	char			filler[3];
} vms_shm_key;
#endif

/*
 * Receive pool shared memory layout -
 *
 * recvpool_ctl_struct
 * upd_proc_local_struct
 * gtmrecv_local_struct
 * zero or more journal records
 */

#define RECVDATA_BASE_OFF	((sizeof(recvpool_ctl_struct) + \
				  sizeof(upd_proc_local_struct) + \
				  sizeof(gtmrecv_local_struct)+ \
				  ~JNL_WRT_END_MASK) & JNL_WRT_END_MASK)
typedef struct
{
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	sm_uc_ptr_t		recvdata_base;
#ifdef UNIX
	gd_region		*recvpool_dummy_reg;
#elif VMS
	int4			shm_range[2];
	int4			shm_lockid;
	vms_shm_key		vms_recvpool_key;
#endif
} recvpool_addrs;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(save)
# pragma pointer_size(long)
#endif

typedef recvpool_addrs	*recvpool_addrs_ptr_t;

#if defined(__osf__) && defined(__alpha)
# pragma pointer_size(restore)
#endif

typedef enum
{
	UPDPROC,
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

#endif
