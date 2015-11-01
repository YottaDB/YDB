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

 /* General repository for global variable definitions. This keeps us from
   pulling in modules and all their references when all we wanted was the
   global data def.. */

#include "mdef.h"

#include "gtm_inet.h"
#include "gtm_iconv.h"
#include "gtm_socket.h"
#include "gtm_unistd.h"

#include <limits.h>
#include <signal.h>
#include <netinet/in.h>		/* Required for gtmsource.h */
#ifdef __MVS__
#include <time.h>      /* required for fd_set */
#include <sys/time.h>
#endif
#ifdef UNIX
# include <sys/un.h>
#endif
#ifdef VMS
# include <descrip.h>		/* Required for gtmsource.h */
# include <ssdef.h>
# include "desblk.h"
#endif
#include "gdsroot.h"
#include "gdskill.h"
#include "ccp.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "cache.h"
#include "comline.h"
#include "compiler.h"
#include "hashdef.h"
#include "hashtab.h"		/* needed also for tp.h */
#include "cmd_qlf.h"
#include "io.h"
#include "iosp.h"
#include "jnl.h"
#include "lv_val.h"
#include "sbs_blk.h"
#include "mdq.h"
#include "mprof.h"
#include "mv_stent.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "mlkdef.h"
#include "zshow.h"
#include "zwrite.h"
#include "zbreak.h"
#include "fnpc.h"
#include "mmseg.h"
#ifndef VMS
# include "gtmsiginfo.h"
#endif
#include "gtmimagename.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"	/* needed for socket_pool */
#include "ctrlc_handler_dummy.h"
#include "unw_prof_frame_dummy.h"
#include "op.h"
#include "gtmsecshr.h"
#include "error_trap.h"

/* FOR REPLICATION RELATED GLOBALS */
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"

/* FOR MERGE RELATED GLOBALS */
#include "subscript.h"
#include "lvname_info.h"
#include "gvname_info.h"
#include "op_merge.h"

#ifdef UNIX
#include "invocation_mode.h"
#endif

#define DEFAULT_ZERROR_STR	"Unprocessed $ZERROR, see $ZSTATUS"
#define DEFAULT_ZERROR_LEN	(sizeof(DEFAULT_ZERROR_STR) - 1)

GBLDEF	gd_region		*gv_cur_region, *db_init_region;
GBLDEF	sgmnt_data_ptr_t	cs_data;
GBLDEF	sgmnt_addrs		*cs_addrs;

GBLDEF	seq_num		start_jnl_seqno;
GBLDEF	seq_num		max_resync_seqno;
GBLDEF	seq_num		consist_jnl_seqno;
GBLDEF	unsigned char	proc_act_type;
GBLDEF	volatile bool	ctrlc_pending;
GBLDEF	volatile int4	ctrap_action_is;
GBLDEF	bool		out_of_time;
GBLDEF	io_pair		io_curr_device;		/* current device	*/
GBLDEF	io_pair		io_std_device;		/* standard device	*/
GBLDEF	io_log_name	*dollar_principal;	/* pointer to log name GTM$PRINCIPAL if defined */
GBLDEF	bool		prin_in_dev_failure = FALSE;
GBLDEF	bool		prin_out_dev_failure = FALSE;
GBLDEF	io_desc		*active_device;

GBLDEF	bool		error_mupip = FALSE,
			compile_time = FALSE,
			file_backed_up = FALSE,
			gv_replopen_error = FALSE,
			gv_replication_error = FALSE,
			incremental = FALSE,
			jobpid = FALSE,
			online = FALSE,
			record = FALSE,
			run_time = FALSE,
			is_standalone = FALSE,
			is_db_updater = FALSE,
			std_dev_outbnd = FALSE,
			in_mupip_freeze = FALSE,
			in_backup = FALSE;

GBLDEF	boolean_t	crit_in_flux =  FALSE,
			is_updproc = FALSE,
			mupip_jnl_recover = FALSE,
			copy_jnl_record	= FALSE,
			set_resync_to_region = FALSE,
			jnlfile_truncation = FALSE,
			repl_enabled = FALSE,
			unhandled_stale_timer_pop = FALSE,
			gtcm_connection = FALSE,
			dollar_truth = TRUE;

GBLDEF	VSIG_ATOMIC_T	forced_exit = FALSE;	/* Asynchronous signal/interrupt handler sets this variable to TRUE,
						 * hence the VSIG_ATOMIC_T type in the definition.
						 */
GBLDEF	unsigned char	*msp,
			*mubbuf,
			*restart_ctxt,
			*stackbase,
			*stacktop,
			*stackwarn;
GBLDEF	int4		backup_close_errno,
			backup_write_errno,
			mubmaxblk,
			forced_exit_err,
			exit_state,
			restore_read_errno;
GBLDEF	volatile int4	outofband;
GBLDEF	int		mumps_status = SS_NORMAL,
			restart_pc,
			stp_array_size = 0;
GBLDEF	cache_entry	*cache_entry_base, *cache_entry_top, *cache_hashent, *cache_stealp, cache_temps;
GBLDEF	cache_tabent	*cache_tabent_base;
GBLDEF	int		cache_hits, cache_fails, cache_temp_cnt;
GBLDEF	gvzwrite_struct	gvzwrite_block;
GBLDEF	io_log_name	*io_root_log_name;
GBLDEF	hashtab		*stp_duptbl = NULL;
GBLDEF	lvzwrite_struct	lvzwrite_block;
GBLDEF	mliteral	literal_chain;
GBLDEF	mstr		*comline_base,
			dollar_zsource,
			*err_act,
			**stp_array,
			extnam_str = {0, NULL},
			env_gtm_env_xlate = {0, NULL};
GBLDEF int              (*gtm_env_xlate_entry)() = NULL;
GBLDEF	mval		dollar_zgbldir,
			dollar_zstatus,
			dollar_zstep = DEFINE_MVAL_LITERAL(MV_STR | MV_NM | MV_INT | MV_NUM_APPROX, 0, 0, 1, "B", 0, 0),
			dollar_ztrap,
			ztrap_pop2level = DEFINE_MVAL_LITERAL(MV_INT, 0, 0, 0, 0, 0, 0),
			zstep_action,
			dollar_system,
			dollar_estack_delta = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, 0, NULL, 0, 0),
			dollar_etrap,
			dollar_zerror = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, DEFAULT_ZERROR_LEN, DEFAULT_ZERROR_STR, 0, 0),
			dollar_zyerror;
GBLDEF	ecode_list	dollar_ecode_base = {0, 0, NULL, NULL},
			*dollar_ecode_list = &dollar_ecode_base;
GBLDEF	int		error_level = 0;		/* execution level where last error occurred */
GBLDEF	int4		error_last_ecode;		/* last error code number */
GBLDEF	unsigned char	*error_last_mpc_err;		/* ptr to original mpc when error occurred */
GBLDEF	unsigned char	*error_last_ctxt_err;		/* ptr to original ctxt when error occurred */
GBLDEF	stack_frame	*error_last_frame_err;		/* ptr to frame where error occurred */
GBLDEF	unsigned char	*error_last_b_line;		/* ptr to beginning of line where error occurred */
GBLDEF	mv_stent	*mv_chain;
GBLDEF	sgm_info	*first_sgm_info;
GBLDEF	spdesc		indr_stringpool,
			rts_stringpool,
			stringpool;
GBLDEF	stack_frame	*frame_pointer, *error_frame;
GBLDEF	stack_frame	*zyerr_frame = NULL;
GBLDEF	symval		*curr_symval;
GBLDEF	tp_frame	*tp_pointer;
GBLDEF	tp_region	*halt_ptr,
			*grlist;
GBLDEF	trans_num	local_tn;	/* transaction number for THIS PROCESS (starts at 0 each time) */
GBLDEF	gv_namehead	*gv_target;
GBLDEF	gv_namehead	*gv_target_list;
GBLDEF	int4		exi_condition;
GBLDEF	uint4		gtmDebugLevel;
GBLDEF	int		process_exiting;
GBLDEF	int4		dollar_zsystem;
GBLDEF	int4		dollar_zeditor;
GBLDEF	boolean_t	sem_incremented = FALSE;
GBLDEF	mval		**ind_result_array, **ind_result_sp, **ind_result_top;
GBLDEF	mval		**ind_source_array, **ind_source_sp, **ind_source_top;
GBLDEF	rtn_tables	*rtn_fst_table, *rtn_names, *rtn_names_top, *rtn_names_end;
GBLDEF	int4		break_message_mask;
GBLDEF	bool		rc_locked = FALSE,
			certify_all_blocks = FALSE;	/* If flag is set all blocks are checked after they are
							 * written to the database.  Upon error we stay critical
							 * and report.  This flag can be set via the MUMPS command
							 * VIEW 1. */
GBLDEF	bool		caller_id_flag = TRUE;
GBLDEF	mval		curr_gbl_root;
GBLDEF	gd_addr		*original_header;
GBLDEF	mem_list	*mem_list_head;
GBLDEF	boolean_t	debug_mupip;
GBLDEF	unsigned char	t_fail_hist[CDB_MAX_TRIES];
GBLDEF	cache_rec_ptr_t	cr_array[((MAX_BT_DEPTH * 2) - 1) * 2];	/* Maximum number of blocks that can be in transaction */
GBLDEF	unsigned int	cr_array_index;
GBLDEF	boolean_t	need_core;		/* Core file should be created */
GBLDEF	boolean_t	created_core;		/* core file was created */
GBLDEF	boolean_t	core_in_progress;	/* creating core NOW */
GBLDEF	boolean_t	dont_want_core;		/* Higher level flag overrides need_core set by lower level rtns */
GBLDEF	boolean_t	exit_handler_active;	/* recursion prevention */
GBLDEF	boolean_t	block_saved;
GBLDEF	iconv_t		dse_over_cvtcd = (iconv_t)0;
GBLDEF	short int	last_source_column;
GBLDEF	char		window_token;
GBLDEF	mval		window_mval;
GBLDEF	mident		window_ident;
GBLDEF	char		director_token;
GBLDEF	mval		director_mval;
GBLDEF	mident		director_ident;
GBLDEF	char		*lexical_ptr;
GBLDEF	int4		aligned_source_buffer[MAX_SRCLINE / sizeof(int4) + 1];
GBLDEF	unsigned char	*source_buffer = (unsigned char *)aligned_source_buffer;
GBLDEF	int4		source_error_found;
GBLDEF	src_line_struct	src_head;
GBLDEF	bool		code_generated;
GBLDEF	short int	source_column, source_line;
GBLDEF	bool		devctlexp;

GBLDEF	int		cmd_cnt;

GBLDEF	command_qualifier	glb_cmd_qlf = { CQ_DEFAULT },
				cmd_qlf = { CQ_DEFAULT };
#ifdef __osf__
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif
GBLDEF	char		**cmd_arg;
#ifdef __osf__
#pragma pointer_size (restore)
#endif

#ifndef __vax
GBLDEF	fnpc_area	fnpca;			/* $piece cache structure area */
#endif

#ifdef MUTEX_MSEM_WAKE
GBLDEF	volatile uint4	heartbeat_counter = 0;
#endif

/* DEFERRED EVENTS */
GBLDEF	int		dollar_zmaxtptime = 0;
GBLDEF	bool		licensed = TRUE;

#if defined(UNIX)
GBLDEF	volatile int4		num_deferred;
#elif defined(VMS)
GBLDEF	volatile short		num_deferred;
GBLDEF	int4 			lkid, lid;
GBLDEF	desblk			exi_blk;
GBLDEF	struct chf$signal_array	*tp_restart_fail_sig;
GBLDEF	boolean_t		tp_restart_fail_sig_used;
#else
# error "Unsupported Platform"
#endif

GBLDEF	volatile	int4		fast_lock_count = 0;	/* Used in wcs_stale */

/* REPLICATION RELATED GLOBALS */
GBLDEF gtmsource_options_t      gtmsource_options;

#ifdef INT8_SUPPORTED
	GBLDEF	const seq_num	seq_num_zero = 0;
	GBLDEF	const seq_num	seq_num_one = 1;
	GBLDEF	const seq_num	seq_num_minus_one = (seq_num)-1;
#else
	GBLDEF	const seq_num	seq_num_zero = {0, 0};
	GBLDEF	const seq_num	seq_num_minus_one = {(uint4)-1, (uint4)-1};
#	ifdef BIGENDIAN
		GBLDEF	const seq_num	seq_num_one = {0, 1};
#	else
		GBLDEF	const seq_num	seq_num_one = {1, 0};
#	endif
#endif

GBLDEF	unsigned char		*profstack_base, *profstack_top, *prof_msp, *profstack_warn;
GBLDEF	unsigned char		*prof_stackptr;
GBLDEF	boolean_t		is_tracing_on;
GBLDEF	stack_frame_prof	*prof_fp;
GBLDEF	void			(*tp_timeout_start_timer_ptr)(int4 tmout_sec) = tp_start_timer_dummy;
GBLDEF	void			(*tp_timeout_clear_ptr)(void) = tp_clear_timeout_dummy;
GBLDEF	void			(*tp_timeout_action_ptr)(void) = tp_timeout_action_dummy;
GBLDEF	void			(*ctrlc_handler_ptr)() = ctrlc_handler_dummy;
GBLDEF	int			(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace) = op_open_dummy;
GBLDEF	void			(*unw_prof_frame_ptr)(void) = unw_prof_frame_dummy;
GBLDEF	boolean_t		mu_reorg_process = FALSE;
GBLDEF	gv_key			*gv_altkey, *gv_currkey, *gv_currkey_next_reorg;
GBLDEF	gv_namehead		*reorg_gv_target;

#ifdef UNIX
GBLDEF	struct sockaddr_un	gtmsecshr_sock_name;
GBLDEF	struct sockaddr_un	gtmsecshr_cli_sock_name;
GBLDEF	key_t			gtmsecshr_key;
#endif
GBLDEF	int			gtmsecshr_sockpath_len;
GBLDEF	int			gtmsecshr_cli_sockpath_len;
GBLDEF	mstr			gtmsecshr_pathname;
GBLDEF	int			server_start_tries;
GBLDEF	int			gtmsecshr_log_file;
GBLDEF	int			gtmsecshr_sockfd = -1;
GBLDEF	boolean_t		gtmsecshr_sock_init_done = FALSE;
GBLDEF	char			muext_code[MUEXT_MAX_TYPES][2] =
				{	{'0', '0'},
					{'0', '1'},
					{'0', '2'},
					{'0', '3'},
					{'0', '4'},
					{'0', '5'},
					{'0', '6'},
					{'0', '7'},
					{'0', '8'},
					{'0', '9'},
					{'1', '0'}
				};
GBLDEF	int			patch_is_fdmp;
GBLDEF	int			patch_fdmp_recs;
GBLDEF	boolean_t		horiz_growth = FALSE;
GBLDEF	int4			prev_first_off, prev_next_off;
				/* these two globals store the values of first_off and next_off in cse,
				 * when there is a blk split at index level. This is to permit rollback
				 * to intermediate states */
GBLDEF	boolean_t		lv_dupcheck = FALSE;
GBLDEF	sm_uc_ptr_t		min_mmseg;
GBLDEF	sm_uc_ptr_t		max_mmseg;
GBLDEF	mmseg			*mmseg_head;
GBLDEF	ua_list			*first_ua, *curr_ua;
GBLDEF	char			*update_array, *update_array_ptr;
GBLDEF	int			gv_fillfactor = 100,
				update_array_size = 0,
				cumul_update_array_size = 0,    /* the current total size of the update array */
				rc_set_fragment;       /* Contains offset within data at which data fragment starts */
GBLDEF	kill_set		*kill_set_tail;
GBLDEF	boolean_t		pool_init = FALSE;
GBLDEF	boolean_t		is_src_server = FALSE;
GBLDEF	boolean_t		is_rcvr_server = FALSE;
GBLDEF	jnl_format_buffer	*non_tp_jfb_ptr = NULL;
GBLDEF	unsigned char		*non_tp_jfb_buff_ptr;
GBLDEF	boolean_t		dse_running = FALSE;
GBLDEF	inctn_opcode_t		inctn_opcode = inctn_invalid_op;
GBLDEF	jnlpool_addrs		jnlpool;
GBLDEF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLDEF	sm_uc_ptr_t		jnldata_base;
GBLDEF	int4			jnlpool_shmid = INVALID_SHMID;
GBLDEF	recvpool_addrs		recvpool;
GBLDEF	int			recvpool_shmid = INVALID_SHMID;
GBLDEF	int			gtmsource_srv_count = 0;
GBLDEF	int			gtmrecv_srv_count = 0;

/* The following _in_prog counters are needed to prevent deadlocks while doing jnl-qio (timer & non-timer). */
GBLDEF	volatile int4		db_fsync_in_prog;
GBLDEF	volatile int4		jnl_qio_in_prog;

/* gbl_jrec_time is
 *	updated    by t_end, tp_tend
 *	referenced by jnl_write_pblk, jnl_write_epoch, jnl_write_logical, jnl_write (in jnl_output.c) and jnl_put_jrt_pini()
 */
GBLDEF	uint4			gbl_jrec_time;
GBLDEF	uint4			zts_jrec_time;	/* time when the ztstart journal record was written. used by op_ztcommit */
GBLDEF	uint4			cur_logirec_short_time; /* Time of last logical jouranl record processed by recover */
#ifdef UNIX
GBLDEF	gtmsiginfo_t		signal_info;
GBLDEF	boolean_t		mutex_salvaged;
#ifndef MUTEX_MSEM_WAKE
GBLDEF	int			mutex_sock_fd = -1;
GBLDEF	struct sockaddr_un	mutex_sock_address;
GBLDEF	struct sockaddr_un	mutex_wake_this_proc;
GBLDEF	int			mutex_wake_this_proc_len;
GBLDEF	int			mutex_wake_this_proc_prefix_len;
GBLDEF	fd_set			mutex_wait_on_descs;
#endif
#endif
GBLDEF	void			(*call_on_signal)();
GBLDEF	gtmImageName		gtmImageNames[n_image_types] =
{
#define IMAGE_TABLE_ENTRY(A,B)	{LIT_AND_LEN(B)},
#include "gtmimagetable.h"
#undef IMAGE_TABLE_ENTRY
};
GBLDEF	enum gtmImageTypes	image_type;	/* initialized at startup i.e. in dse.c, lke.c, gtm.c, mupip.c, gtmsecshr.c etc. */
GBLDEF	volatile boolean_t	semwt2long;

#ifdef UNIX
GBLDEF	unsigned int		invocation_mode = MUMPS_COMPILE; /* how mumps has been invoked */
#endif

/* this array is indexed by file descriptor */
GBLDEF	boolean_t		*lseekIoInProgress_flags = (boolean_t *)0;

#if defined(UNIX)
/* Latch variable for Unix implementations. Used in SUN and HP */
GBLDEF	global_latch_t		defer_latch;
#endif

GBLDEF	int			num_additional_processors;
GBLDEF	int			gtm_errno = -1;		/* holds the errno (unix) in case of an rts_error */
GBLDEF	int4			error_condition = 0;
GBLDEF	global_tlvl_info	*global_tlvl_info_head;
GBLDEF	buddy_list		*global_tlvl_info_list;
GBLDEF	boolean_t		rename_changes_jnllink = TRUE;
GBLDEF	boolean_t		job_try_again;
GBLDEF	volatile int4		gtmMallocDepth;		/* Recursion indicator */
GBLDEF	d_socket_struct		*socket_pool;
GBLDEF	boolean_t		disable_sigcont = FALSE;
GBLDEF	boolean_t		mu_star_specified;

#ifndef VMS
GBLDEF	volatile int		suspend_status = NO_SUSPEND;
#endif

GBLDEF	gv_namehead		*reset_gv_target = INVALID_GV_TARGET;
GBLDEF	VSIG_ATOMIC_T		util_interrupt = 0;
GBLDEF	boolean_t		kip_incremented;
GBLDEF	boolean_t		need_kip_incr;
GBLDEF	int			merge_args = 0;
GBLDEF	merge_glvn_ptr		mglvnp = NULL;
GBLDEF	int			ztrap_form;
GBLDEF	boolean_t		ztrap_new;
GBLDEF	int4			wtfini_in_prog;
/* items for $piece stats */
#ifdef DEBUG
GBLDEF	int	c_miss;				/* cache misses (debug) */
GBLDEF	int	c_hit;				/* cache hits (debug) */
GBLDEF	int	c_small;			/* scanned small string brute force */
GBLDEF	int	c_small_pcs;			/* chars scanned by small scan */
GBLDEF	int	c_pskip;			/* number of pieces "skipped" */
GBLDEF	int	c_pscan;			/* number of pieces "scanned" */
GBLDEF	int	c_parscan;			/* number of partial scans (partial cache hits) */
GBLDEF	int	cs_miss;			/* cache misses (debug) */
GBLDEF	int	cs_hit;				/* cache hits (debug) */
GBLDEF	int	cs_small;			/* scanned small string brute force */
GBLDEF	int	cs_small_pcs;			/* chars scanned by small scan */
GBLDEF	int	cs_pskip;			/* number of pieces "skipped" */
GBLDEF	int	cs_pscan;			/* number of pieces "scanned" */
GBLDEF	int	cs_parscan;			/* number of partial scans (partial cache hits) */
GBLDEF	int	c_clear;			/* cleared due to (possible) value change */
GBLDEF	boolean_t	setp_work;
#endif
GBLDEF z_records	zbrk_recs = {0, 0, 0, 0};

#ifdef UNIX
GBLDEF	ipcs_mesg	db_ipcs;		/* For requesting gtmsecshr to update ipc fields */
GBLDEF	gd_region	*ftok_sem_reg = NULL;	/* Last region for which ftok semaphore is grabbed */
GBLDEF	gd_region	*standalone_reg = NULL;	/* We have standalone access for this region */
#endif

GBLDEF	fixed_jrec_tp_kill_set 	mur_jrec_fixed_field;	/* Recover/Rollback uses to copy the journal record fields */
GBLDEF	struct_jrec_tcom 	mur_jrec_fixed_tcom;	/* For copying tcom journal record fields */
GBLDEF	boolean_t		write_after_image = FALSE;	/* true for after-image jnlrecord writing by recover/rollback */
GBLDEF	boolean_t		got_repl_standalone_access = FALSE;
GBLDEF	int			iott_write_error;
GBLDEF  boolean_t               recovery_success = FALSE; /* To Indicate successful recovery */
GBLDEF	int4			write_filter;
GBLDEF	boolean_t		need_no_standalone = FALSE;
GBLDEF	boolean_t		forw_phase_recovery = FALSE; /* To inidicate the forward phase recovery */

GBLDEF	int4	zdir_form = ZDIR_FORM_FULLPATH; /* $ZDIR shows full path including DEVICE and DIRECTORY */
GBLDEF	mval	dollar_zdir = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, 0, NULL, 0, 0);
