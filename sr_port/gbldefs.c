/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

 /* General repository for global variable definitions. This keeps us from
  * pulling in modules and all their references when all we wanted was the
  * global data def.
  *
  * Note, all GBLDEF fields are automatically cleared to zeroes. No initialization to
  * zero (0) or NULL or any other value that translates to zeroes is necessary on these
  * fields and should be avoided as it creates extra linker and image startup processing
  * that can only slow things down even if only by a little.
  */

#include "mdef.h"

#include "gtm_inet.h"
#include "gtm_iconv.h"
#include "gtm_socket.h"
#include "gtm_unistd.h"
#include "gtm_limits.h"

#include <signal.h>
#include <sys/time.h>
#ifdef UNIX
# include <sys/un.h>
#endif
#ifdef VMS
# include <descrip.h>		/* Required for gtmsource.h */
# include <ssdef.h>
# include <fab.h>
# include "desblk.h"
#endif
#ifdef GTM_PTHREAD
# include <pthread.h>
#endif
#include "cache.h"
#include "hashtab_addr.h"
#include "hashtab_int4.h"
#include "hashtab_int8.h"
#include "hashtab_mname.h"
#include "hashtab_str.h"
#include "hashtab_objcode.h"
/* The define of CHEXPAND below causes error.h to create GBLDEFs */
#define CHEXPAND
#include "error.h"
#include <rtnhdr.h>
#include "gdsroot.h"
#include "gdskill.h"
#include "ccp.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "comline.h"
#include "compiler.h"
#include "cmd_qlf.h"
#include "io.h"
#include "iosp.h"
#include "jnl.h"
#include "lv_val.h"
#include "mdq.h"
#include "mprof.h"
#include "mv_stent.h"
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
#include "mmseg.h"
#ifndef VMS
# include "gtmsiginfo.h"
#endif
#include "gtmimagename.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"	/* needed for socket_pool and MAX_N_SOCKETS */
#include "ctrlc_handler_dummy.h"
#include "unw_prof_frame_dummy.h"
#include "op.h"
#include "gtmsecshr.h"
#include "error_trap.h"
#include "patcode.h"	/* for pat_everything and sizeof_pat_everything */
#include "source_file.h"	/* for REV_TIME_BUFF_LEN */
#include "mupipbckup.h"
#include "dpgbldir.h"
#include "mmemory.h"
#include "have_crit.h"
#include "alias.h"
/* FOR REPLICATION RELATED GLOBALS */
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
/* FOR MERGE RELATED GLOBALS */
#include "gvname_info.h"
#include "op_merge.h"
#ifdef UNIX
#include "cli.h"
#include "invocation_mode.h"
#include "fgncal.h"
#include "parse_file.h"		/* for MAX_FBUFF */
#include "repl_sem.h"
#include "gtm_zlib.h"
#include "anticipatory_freeze.h"
#endif
#include "jnl_typedef.h"
#include "repl_ctl.h"
#ifdef VMS
#include "gtm_logicals.h"	/* for GTM_MEMORY_NOACCESS_COUNT */
#endif
#include "gds_blk_upgrade.h"	/* for UPGRADE_IF_NEEDED flag */
#include "cws_insert.h"		/* for CWS_REORG_ARRAYSIZE */
#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#include "gtm_conv.h"
#endif
# ifdef GTM_CRYPT
# include "gtmcrypt.h"
# include "gdsblk.h"
# include "muextr.h"
# endif
#ifdef GTM_TRIGGER
#include "gv_trigger.h"
#include "gtm_trigger.h"
#endif
#define DEFAULT_ZERROR_STR	"Unprocessed $ZERROR, see $ZSTATUS"
#define DEFAULT_ZERROR_LEN	(SIZEOF(DEFAULT_ZERROR_STR) - 1)

GBLDEF	gd_region		*db_init_region;
GBLDEF	sgmnt_data_ptr_t	cs_data;
GBLDEF	sgmnt_addrs		*cs_addrs;
GBLDEF	sgmnt_addrs		*cs_addrs_list;	/* linked list of csa corresponding to all currently open databases */

GBLDEF	unsigned short	proc_act_type;
GBLDEF	volatile bool	ctrlc_pending;
GBLDEF	bool		undef_inhibit;
GBLDEF	volatile int4	ctrap_action_is;
GBLDEF	bool		out_of_time;
GBLDEF	io_pair		io_curr_device;		/* current device	*/
GBLDEF	io_pair		io_std_device;		/* standard device	*/
GBLDEF	io_log_name	*dollar_principal;	/* pointer to log name GTM$PRINCIPAL if defined */
GBLDEF	bool		prin_in_dev_failure;
GBLDEF	bool		prin_out_dev_failure;
GBLDEF	io_desc		*active_device;
GBLDEF	bool		error_mupip,
			file_backed_up,
			gv_replopen_error,
			gv_replication_error,
			incremental,
			jobpid,
			online,
			record,
			std_dev_outbnd,
			in_mupip_freeze,
	                in_backup,
	                view_debug1,
	                view_debug2,
	                view_debug3,
	                view_debug4,
			mupip_error_occurred,
	                dec_nofac;
GBLDEF	boolean_t	is_updproc,
			is_updhelper,
			mupip_jnl_recover,
			suspend_lvgcol,
			run_time,
			unhandled_stale_timer_pop,
			gtcm_connection,
			is_replicator,		/* TRUE => this process can write jnl records to the jnlpool for replicated db */
	                tp_in_use,		/* TRUE => TP has been used by this process and is thus initialized */
			dollar_truth = TRUE,
			gtm_stdxkill,		/* TRUE => Use M Standard X-KILL - FALSE use historical GTM X-KILL (default) */
			in_timed_tn,		/* TRUE => Timed TP transaction in progress */
			tp_timeout_deferred;	/* TRUE => A TP timeout has occurred but is deferred */
GBLDEF	volatile boolean_t tp_timeout_set_xfer;	/* TRUE => A timeout succeeded in setting xfer table intercepts. This flag stays
						 * a true global unless each thread gets its own xfer table.
						 */
GBLDEF	VSIG_ATOMIC_T	forced_exit;		/* Asynchronous signal/interrupt handler sets this variable to TRUE,
						 * hence the VSIG_ATOMIC_T type in the definition.
						 */
GBLDEF	intrpt_state_t	intrpt_ok_state = INTRPT_OK_TO_INTERRUPT;	/* any other value implies it is not ok to interrupt */
GBLDEF	unsigned char	*msp,
			*mubbuf,
			*restart_ctxt,
			*stackbase,
			*stacktop,
			*stackwarn,
			*restart_pc;
GBLDEF	int4		backup_close_errno,
			backup_write_errno,
			mubmaxblk,
			forced_exit_err,
			exit_state,
			restore_read_errno;
GBLDEF	volatile int4	outofband, crit_count;
GBLDEF	int		mumps_status = SS_NORMAL,
			stp_array_size;
GBLDEF	gvzwrite_datablk	*gvzwrite_block;
GBLDEF	lvzwrite_datablk	*lvzwrite_block;
GBLDEF	io_log_name	*io_root_log_name;
GBLDEF	mliteral	literal_chain;
GBLDEF	mstr		*comline_base,
			*err_act,
			**stp_array,
			extnam_str,
			env_gtm_env_xlate;
GBLDEF MSTR_CONST(default_sysid, "gtm_sysid");
GBLDEF	mval		dollar_zgbldir,
			dollar_zsource = DEFINE_MVAL_STRING(MV_STR, 0, 0, 0, NULL, 0, 0),
			dollar_zstatus,
			dollar_zstep = DEFINE_MVAL_STRING(MV_STR | MV_NM | MV_INT | MV_NUM_APPROX, 0, 0, 1, "B", 0, 0),
			dollar_ztrap,
			ztrap_pop2level = DEFINE_MVAL_STRING(MV_NM | MV_INT, 0, 0, 0, 0, 0, 0),
			zstep_action,
			dollar_system,
			dollar_estack_delta = DEFINE_MVAL_STRING(MV_STR, 0, 0, 0, NULL, 0, 0),
			dollar_etrap,
			dollar_zerror = DEFINE_MVAL_STRING(MV_STR, 0, 0, DEFAULT_ZERROR_LEN, DEFAULT_ZERROR_STR, 0, 0),
			dollar_zyerror,
			dollar_ztexit = DEFINE_MVAL_STRING(MV_STR, 0, 0, 0, NULL, 0, 0);
GBLDEF  uint4		dollar_zjob;
GBLDEF	mval		dollar_zinterrupt;
GBLDEF	boolean_t	dollar_zininterrupt;
GBLDEF	boolean_t	dollar_ztexit_bool; /* Truth value of dollar_ztexit when coerced to boolean */
GBLDEF	boolean_t	dollar_zquit_anyway;
GBLDEF	mv_stent	*mv_chain;
GBLDEF	sgm_info	*first_sgm_info;	/* List of participating regions in the TP transaction with NO ftok ordering */
GBLDEF	sgm_info	*first_tp_si_by_ftok;	/* List of participating regions in the TP transaction sorted on ftok order */
GBLDEF	spdesc		indr_stringpool,
			rts_stringpool,
			stringpool;
GBLDEF	stack_frame	*frame_pointer;
GBLDEF	stack_frame	*zyerr_frame;
GBLDEF	symval		*curr_symval;
GBLDEF	tp_frame	*tp_pointer;
GBLDEF	tp_region	*halt_ptr,
			*grlist;
GBLDEF	trans_num	local_tn;		/* transaction number for THIS PROCESS (starts at 0 each time) */
GBLDEF	trans_num	tstart_local_tn;	/* copy of global variable "local_tn" at op_tstart time */
GBLDEF	gv_namehead	*gv_target;
GBLDEF	gv_namehead	*gv_target_list;	/* List of ALL gvts that were allocated (in targ_alloc) by this process */
GBLDEF	gv_namehead	*gvt_tp_list;		/* List of gvts that were referenced in the current TP transaction */
GBLDEF	gvt_container	*gvt_pending_list;	/* list of gvts that need to be re-examined/re-allocated when region is opened */
GBLDEF	buddy_list	*gvt_pending_buddy_list;/* buddy_list for maintaining memory for gv_targets to be re-examined/allocated */
GBLDEF	int4		exi_condition;
GBLDEF	uint4		gtmDebugLevel;
GBLDEF	caddr_t		smCallerId;			/* Caller of top level malloc/free */
GBLDEF	int		process_exiting;
GBLDEF	int4		dollar_zsystem;
GBLDEF	int4		dollar_zeditor;
GBLDEF	rtn_tabent	*rtn_fst_table, *rtn_names, *rtn_names_top, *rtn_names_end;
GBLDEF	int4		break_message_mask;
GBLDEF	bool		rc_locked;
GBLDEF	boolean_t	certify_all_blocks;		/* If flag is set all blocks are checked after they are
							 * written to the database.  Upon error we stay critical
							 * and report.  This flag can be set via the MUMPS command
							 * VIEW "GDSCERT":1. */
GBLDEF	mval		curr_gbl_root;
GBLDEF	gd_addr		*original_header;
GBLDEF	hash_table_str	*complits_hashtab;
GBLDEF	hash_table_str	*compsyms_hashtab;
GBLDEF	mem_list	*mem_list_head;
GBLDEF	boolean_t	debug_mupip;
GBLDEF	unsigned char	t_fail_hist[CDB_MAX_TRIES];	/* type has to be unsigned char and not enum cdb_sc to ensure single byte */
GBLDEF	cache_rec_ptr_t	cr_array[((MAX_BT_DEPTH * 2) - 1) * 2];	/* Maximum number of blocks that can be in transaction */
GBLDEF	unsigned int	cr_array_index;
GBLDEF	boolean_t	need_core;		/* Core file should be created */
GBLDEF	boolean_t	created_core;		/* core file was created */
GBLDEF	unsigned int	core_in_progress;	/* creating core NOW if > 0 */
GBLDEF	boolean_t	dont_want_core;		/* Higher level flag overrides need_core set by lower level rtns */
GBLDEF	boolean_t	exit_handler_active;	/* recursion prevention */
GBLDEF	boolean_t	block_saved;
#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
GBLDEF	iconv_t		dse_over_cvtcd = (iconv_t)0;
#endif
GBLDEF	gtm_chset_t	dse_over_chset = CHSET_M;
LITDEF	MIDENT_DEF(zero_ident, 0, NULL);		/* the null mident */
GBLDEF	char		*lexical_ptr;
GBLDEF	int4		aligned_source_buffer[MAX_SRCLINE / SIZEOF(int4) + 1];
GBLDEF	unsigned char	*source_buffer = (unsigned char *)aligned_source_buffer;
GBLDEF	src_line_struct	src_head;
GBLDEF	short int	source_column, source_line;
GBLDEF	bool		devctlexp;
GBLDEF 	char		cg_phase;       /* code generation phase */
/* Previous code generation phase: Only used by emit_code.c to initialize the push list at the
 * beginning of each phase (bug fix: C9D12-002478) */
GBLDEF 	char		cg_phase_last;
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
#ifdef UNIX
GBLDEF	volatile uint4	heartbeat_counter;
GBLDEF	boolean_t	heartbeat_started;
#endif
/* DEFERRED EVENTS */
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
GBLDEF	volatile	int4	fast_lock_count;	/* Used in wcs_stale */
/* REPLICATION RELATED GLOBALS */
GBLDEF	gtmsource_options_t	gtmsource_options;
GBLDEF	gtmrecv_options_t	gtmrecv_options;

GBLDEF	boolean_t		is_tracing_on;
GBLDEF	void			(*tp_timeout_start_timer_ptr)(int4 tmout_sec) = tp_start_timer_dummy;
GBLDEF	void			(*tp_timeout_clear_ptr)(void) = tp_clear_timeout_dummy;
GBLDEF	void			(*tp_timeout_action_ptr)(void) = tp_timeout_action_dummy;
GBLDEF	void			(*ctrlc_handler_ptr)() = ctrlc_handler_dummy;
GBLDEF	int			(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace) = op_open_dummy;
GBLDEF	void			(*unw_prof_frame_ptr)(void) = unw_prof_frame_dummy;
GBLDEF	void			(*heartbeat_timer_ptr)(void);	/* Initialized only in gtm_startup() */
#ifdef UNICODE_SUPPORTED
GBLDEF	u_casemap_t 		gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif
GBLDEF	boolean_t		mu_reorg_process;		/* set to TRUE by MUPIP REORG */
GBLDEF	boolean_t		mu_reorg_in_swap_blk;		/* set to TRUE for the duration of the call to "mu_swap_blk" */
GBLDEF	boolean_t		mu_rndwn_process;
GBLDEF	gv_key			*gv_currkey_next_reorg;
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
GBLDEF	int			gtmsecshr_sockfd = FD_INVALID;
GBLDEF	boolean_t		gtmsecshr_sock_init_done;
GBLDEF	char			muext_code[MUEXT_MAX_TYPES][2] =
{
#	define MUEXT_TABLE_ENTRY(muext_rectype, code0, code1)	{code0, code1},
#	include "muext_rec_table.h"
#	undef MUEXT_TABLE_ENTRY
};
GBLDEF	int			patch_is_fdmp;
GBLDEF	int			patch_fdmp_recs;
GBLDEF	boolean_t		horiz_growth;
GBLDEF	int4			prev_first_off, prev_next_off;
				/* these two globals store the values of first_off and next_off in cse,
				 * when there is a blk split at index level. This is to permit rollback
				 * to intermediate states */
GBLDEF	sm_uc_ptr_t		min_mmseg;
GBLDEF	sm_uc_ptr_t		max_mmseg;
GBLDEF	mmseg			*mmseg_head;
GBLDEF	ua_list			*first_ua, *curr_ua;
GBLDEF	char			*update_array, *update_array_ptr;
GBLDEF	int			gv_fillfactor = 100,
				rc_set_fragment;       		/* Contains offset within data at which data fragment starts */
GBLDEF	uint4			update_array_size,
				cumul_update_array_size;	/* the current total size of the update array */
GBLDEF	kill_set		*kill_set_tail;
GBLDEF	boolean_t		pool_init;
GBLDEF	boolean_t		is_src_server;
GBLDEF	boolean_t		is_rcvr_server;
GBLDEF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLDEF	boolean_t		dse_running;
GBLDEF	jnlpool_addrs		jnlpool;
GBLDEF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLDEF	jnlpool_ctl_struct	temp_jnlpool_ctl_struct;
GBLDEF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl = &temp_jnlpool_ctl_struct;
GBLDEF	sm_uc_ptr_t		jnldata_base;
GBLDEF	int4			jnlpool_shmid = INVALID_SHMID;
GBLDEF	recvpool_addrs		recvpool;
GBLDEF	int			recvpool_shmid = INVALID_SHMID;
GBLDEF	int			gtmsource_srv_count;
GBLDEF	int			gtmrecv_srv_count;
/* The following _in_prog counters are needed to prevent deadlocks while doing jnl-qio (timer & non-timer). */
GBLDEF	volatile int4		db_fsync_in_prog;
GBLDEF	volatile int4		jnl_qio_in_prog;
#ifdef UNIX
GBLDEF	void			(*op_write_ptr)(mval *);
GBLDEF	void			(*op_wteol_ptr)(int4 n);
GBLDEF	gtmsiginfo_t		signal_info;
#ifndef MUTEX_MSEM_WAKE
GBLDEF	int			mutex_sock_fd = FD_INVALID;
GBLDEF	struct sockaddr_un	mutex_sock_address;
GBLDEF	struct sockaddr_un	mutex_wake_this_proc;
GBLDEF	int			mutex_wake_this_proc_len;
GBLDEF	int			mutex_wake_this_proc_prefix_len;
GBLDEF	fd_set			mutex_wait_on_descs;
#endif
#endif
GBLDEF	void			(*call_on_signal)();
GBLDEF	enum gtmImageTypes	image_type;	/* initialized at startup i.e. in dse.c, lke.c, gtm.c, mupip.c, gtmsecshr.c etc. */

#ifdef UNIX
GBLDEF	parmblk_struct 		*param_list; /* call-in parameters block (defined in unix/fgncalsp.h)*/
GBLDEF	unsigned int		invocation_mode = MUMPS_COMPILE; /* how mumps has been invoked */
GBLDEF	char			cli_err_str[MAX_CLI_ERR_STR] = "";   /* Parse Error message buffer */
GBLDEF	char			*cli_err_str_ptr;
GBLDEF	boolean_t		gtm_pipe_child;
#endif
GBLDEF	io_desc			*gtm_err_dev;
/* this array is indexed by file descriptor */
GBLDEF	boolean_t		*lseekIoInProgress_flags;
#if defined(UNIX)
/* Latch variable for Unix implementations. Used in SUN and HP */
GBLDEF	global_latch_t		defer_latch;
#endif
GBLDEF	int			num_additional_processors;
GBLDEF	int			gtm_errno = -1;		/* holds the errno (unix) in case of an rts_error */
GBLDEF	int4			error_condition;
GBLDEF	global_tlvl_info	*global_tlvl_info_head;
GBLDEF	buddy_list		*global_tlvl_info_list;
GBLDEF	boolean_t		job_try_again;
GBLDEF	volatile int4		gtmMallocDepth;		/* Recursion indicator */
GBLDEF	d_socket_struct		*socket_pool;
GBLDEF	boolean_t		mu_star_specified;
GBLDEF	backup_reg_list		*mu_repl_inst_reg_list;
#ifndef VMS
GBLDEF	volatile int		suspend_status = NO_SUSPEND;
#endif
GBLDEF	gv_namehead		*reset_gv_target = INVALID_GV_TARGET;
GBLDEF	VSIG_ATOMIC_T		util_interrupt;
GBLDEF	sgmnt_addrs		*kip_csa;
GBLDEF	boolean_t		need_kip_incr;
GBLDEF	int			merge_args;
GBLDEF	merge_glvn_ptr		mglvnp;
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
GBLDEF z_records	zbrk_recs;
#ifdef UNIX
GBLDEF	ipcs_mesg	db_ipcs;		/* For requesting gtmsecshr to update ipc fields */
GBLDEF	gd_region	*ftok_sem_reg;		/* Last region for which ftok semaphore is grabbed */
GBLDEF	int		gtm_non_blocked_write_retries; /* number of retries for non-blocked write to pipe */
#endif
#ifdef VMS
/* Following global variables store the state of an erroring sys$qio just before a GTMASSERT in the CHECK_CHANNEL_STATUS macro */
GBLDEF	uint4	check_channel_status;		/* stores the qio return status */
GBLDEF	uint4	check_channel_id;		/* stores the qio channel id */
#endif
GBLDEF	boolean_t		write_after_image;	/* true for after-image jnlrecord writing by recover/rollback */
GBLDEF	int			iott_write_error;
GBLDEF	int4			write_filter;
GBLDEF	boolean_t		need_no_standalone;
GBLDEF	int4	zdir_form = ZDIR_FORM_FULLPATH; /* $ZDIR shows full path including DEVICE and DIRECTORY */
GBLDEF	mval	dollar_zdir = DEFINE_MVAL_STRING(MV_STR, 0, 0, 0, NULL, 0, 0);
GBLDEF	int * volatile		var_on_cstack_ptr; /* volatile pointer to int; volatile so that nothing gets optimized out */
GBLDEF	hash_table_int4		cw_stagnate;
GBLDEF	boolean_t		cw_stagnate_reinitialized;

GBLDEF	uint4		pat_everything[] = { 0, 2, PATM_E, 1, 0, PAT_MAX_REPEAT, 0, PAT_MAX_REPEAT, 1 }; /* pattern = ".e" */
GBLDEF	mstr_len_t	sizeof_pat_everything = SIZEOF(pat_everything);
GBLDEF	uint4		*pattern_typemask;
GBLDEF	pattern		*pattern_list;
GBLDEF	pattern		*curr_pattern;
/* Unicode related data */
GBLDEF	boolean_t	gtm_utf8_mode;		/* Is GT.M running with Unicode Character Set; Set only after ICU initialization */
GBLDEF	boolean_t	is_gtm_chset_utf8;	/* Is gtm_chset environment variable set to UTF8 */
GBLDEF	boolean_t	utf8_patnumeric;	/* Should patcode N match non-ASCII numbers in pattern match ? */
GBLDEF	boolean_t	badchar_inhibit;	/* Suppress malformed UTF-8 characters by default */
GBLDEF  MSTR_DEF(dollar_zchset, 1, "M");
GBLDEF  MSTR_DEF(dollar_zpatnumeric, 1, "M");
/* Standard MUMPS pattern-match table.
 * This table holds the current pattern-matching attributes of each ASCII character.
 * Bits 0..23 of each entry correspond with the pattern-match characters, A..X.
 */
GBLDEF pattern mumps_pattern = {
	(void *) 0,		/* flink */
	(void *) 0,		/* typemask */
	(void *) 0,		/* pat YZ name array */
	(void *) 0,		/* pat YZ name-length array */
	-1,			/* number of YZ patcodes */
	1,			/* namlen */
	{'M', '\0'}		/* name */
};
/* mapbit is used by pattab.c and patstr.c. Note that patstr.c uses only entries until PATM_X */
GBLDEF	readonly uint4	mapbit[] =
{
	PATM_A, PATM_B, PATM_C, PATM_D, PATM_E, PATM_F, PATM_G, PATM_H,
	PATM_I, PATM_J, PATM_K, PATM_L, PATM_M, PATM_N, PATM_O, PATM_P,
	PATM_Q, PATM_R, PATM_S, PATM_T, PATM_U, PATM_V, PATM_W, PATM_X,
	PATM_YZ1, PATM_YZ2, PATM_YZ3, PATM_YZ4
};
LITDEF	uint4	typemask[PATENTS] =
{
	PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C,	/* hex 00-07 : ASCII characters */
	PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C,	/* hex 08-0F : ASCII characters */
	PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C,	/* hex 10-17 : ASCII characters */
	PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C,	/* hex 18-1F : ASCII characters */
	PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P,	/* hex 20-27 : ASCII characters */
	PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P,	/* hex 28-2F : ASCII characters */
	PATM_N, PATM_N, PATM_N, PATM_N, PATM_N, PATM_N, PATM_N, PATM_N,	/* hex 30-37 : ASCII characters */
	PATM_N, PATM_N, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P,	/* hex 38-3F : ASCII characters */
	PATM_P, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U,	/* hex 40-47 : ASCII characters */
	PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U,	/* hex 48-4F : ASCII characters */
	PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U,	/* hex 50-57 : ASCII characters */
	PATM_U, PATM_U, PATM_U, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P,	/* hex 58-5F : ASCII characters */
	PATM_P, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L,	/* hex 60-67 : ASCII characters */
	PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L,	/* hex 68-6F : ASCII characters */
	PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L,	/* hex 70-77 : ASCII characters */
	PATM_L, PATM_L, PATM_L, PATM_P, PATM_P, PATM_P, PATM_P, PATM_C,	/* hex 78-7F : ASCII characters */
	PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C,	/* hex 80-87 : non-ASCII characters */
	PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C,	/* hex 88-8F : non-ASCII characters */
	PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C,	/* hex 90-97 : non-ASCII characters */
	PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C, PATM_C,	/* hex 98-9F : non-ASCII characters */
	PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P,	/* hex A0-A7 : non-ASCII characters */
	PATM_P, PATM_P, PATM_L, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P,	/* hex A8-AF : non-ASCII characters */
	PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P,	/* hex B0-B7 : non-ASCII characters */
	PATM_P, PATM_P, PATM_L, PATM_P, PATM_P, PATM_P, PATM_P, PATM_P,	/* hex B8-BF : non-ASCII characters */
	PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U,	/* hex C0-C7 : non-ASCII characters */
	PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U,	/* hex C8-CF : non-ASCII characters */
	PATM_P, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U,	/* hex D0-D7 : non-ASCII characters */
	PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_U, PATM_P, PATM_L,	/* hex D8-DF : non-ASCII characters */
	PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L,	/* hex E0-E7 : non-ASCII characters */
	PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L,	/* hex E8-EF : non-ASCII characters */
	PATM_P, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L,	/* hex F0-F7 : non-ASCII characters */
	PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_L, PATM_P, PATM_C	/* hex F8-FF : non-ASCII characters */
};
GBLDEF	uint4		pat_allmaskbits;	/* universal set of valid pattern bit codes for currently active pattern table */
/* globals related to caching of pattern evaluation match result.
 * for a given <strptr, strlen, patptr, depth> tuple, we store the evaluation result.
 * in the above,
 *	<strptr, strlen>	uniquely identifies a substring of the input string.
 *	<patptr>		identifies the pattern atom that we are matching with
 *	<depth>			identifies the recursion depth of do_patalt() for this pattern atom ("repcnt" in code)
 * note that <depth> is a necessity in the above because the same alternation pattern atom can have different
 *	match or not-match status for the same input string depending on the repetition count usage of the pattern atom
 * after a series of thoughts on an efficient structure for storing pattern evaluation, finally arrived at a simple
 *	array of structures wherein for a given length (strlen) we have a fixed number of structures available.
 * we allocate an array of structures, say, 1024 structures.
 * this is a simple 1-1 mapping, wherein
 *	for length 0, the available structures are the first 32 structures of the array,
 *	for length 1, the available structures are the second 32 structures of the array.
 *	...
 *	for length 47, the available structures are the 47th 32 structures of the array.
 *	for length 48 and above, the available structures are all the remaining structures of the array.
 * whenever any new entry needs to be cached and there is no room among the available structures, we preempt the
 *	most unfrequently used cache entry (to do this we do keep a count of every entry's frequency of usage)
 * the assumption is that substrings of length > 48 (an arbitrary reasonable small number) won't be used
 *	so frequently so that they have lesser entries to fight for among themselves than lower values of length.
 * with the above caching in place, the program segment below took 15 seconds.
 * it was found that if the array size is increased to 16384 (as opposed to 1024 as above) and the available
 *	structures for each length increased proportionally (i.e. 16 times = 16*32 structures instead of 32 as above)
 *	the performance improved to the extent of taking 3 seconds.
 * but this raised an interesting question, that of "size" vs. "time" tradeoff.
 * with increasing array size, we get better "time" performance due to better caching.
 * but that has an overhead of increased "size" (memory) usage.
 * to arrive at a compromise, a dynamic algorithm emerged. the process will allocate a small array
 *	beginning at 1024 entries and grow to a max of 16384 entries as and when it deems the hit ratio is not good.
 * the array only grows, i.e. there is no downsizing algorithm at play.
 * the dynamic algorithm addresses to an extent both the "size" and "time" issues and finishes the below in 1 second.
 * #defines for the dynamic algorithm growth can be found in patcode.h
 */
GBLDEF	int4		curalt_depth = -1;				/* depth of alternation nesting */
GBLDEF	int4		do_patalt_calls[PTE_MAX_CURALT_DEPTH];		/* number of calls to do_patalt() */
GBLDEF	int4		do_patalt_hits[PTE_MAX_CURALT_DEPTH];		/* number of pte_csh hits in do_patalt() */
GBLDEF	int4		do_patalt_maxed_out[PTE_MAX_CURALT_DEPTH];	/* no. of pte_csh misses after maxing on allocation size */

GBLDEF	pte_csh		*pte_csh_array[PTE_MAX_CURALT_DEPTH];		/* pte_csh array (per curalt_depth) */
GBLDEF	int4		pte_csh_cur_size[PTE_MAX_CURALT_DEPTH];		/* current pte_csh size (per curalt_depth) */
GBLDEF	int4		pte_csh_alloc_size[PTE_MAX_CURALT_DEPTH];	/* current allocated pte_csh size (per curalt_depth) */
GBLDEF	int4		pte_csh_entries_per_len[PTE_MAX_CURALT_DEPTH];	/* current number of entries per len */
GBLDEF	int4		pte_csh_tail_count[PTE_MAX_CURALT_DEPTH];	/* count of non 1-1 corresponding pte_csh_array members */

GBLDEF	pte_csh		*cur_pte_csh_array;			/* copy of pte_csh_array corresponding to curalt_depth */
GBLDEF	int4		cur_pte_csh_size;			/* copy of pte_csh_cur_size corresponding to curalt_depth */
GBLDEF	int4		cur_pte_csh_entries_per_len;		/* copy of pte_csh_entries_per_len corresponding to curalt_depth */
GBLDEF	int4		cur_pte_csh_tail_count;			/* copy of pte_csh_tail_count corresponding to curalt_depth */

GBLDEF	readonly char	*before_image_lit[] = {"NOBEFORE_IMAGES", "BEFORE_IMAGES"};
GBLDEF	readonly char	*jnl_state_lit[]    = {"DISABLED", "OFF", "ON"};
GBLDEF	readonly char	*repl_state_lit[]   = {"OFF", "ON", "WAS_ON"};

GBLDEF	boolean_t	crit_sleep_expired;		/* mutex.mar: signals that a timer waiting for crit has expired */
GBLDEF	uint4		crit_deadlock_check_cycle;	/* compared to csa->crit_check_cycle to determine if a given region
							   in a transaction legitimately has crit or not */
GBLDEF	node_local_ptr_t	locknl;		/* if non-NULL, indicates node-local of interest to the LOCK_HIST macro */
GBLDEF	boolean_t	in_mutex_deadlock_check;	/* if TRUE, mutex_deadlock_check() is part of our current C-stack trace */
/* $ECODE and $STACK related variables.
 * error_frame and skip_error_ret should ideally be part of dollar_ecode structure. since sr_avms/opp_ret.m64 uses these
 * global variables and it was felt risky changing it to access a member of a structure, they are kept as separate globals */
GBLDEF	stack_frame		*error_frame;		/* ptr to frame where last error occurred or was rethrown */
GBLDEF	boolean_t		skip_error_ret;		/* set to TRUE by golevel(), used and reset by op_unwind() */
GBLDEF	dollar_ecode_type	dollar_ecode;		/* structure containing $ECODE related information */
GBLDEF	dollar_stack_type	dollar_stack;		/* structure containing $STACK related information */
GBLDEF	boolean_t		ztrap_explicit_null;	/* whether $ZTRAP was explicitly set to NULL in the current frame */
GBLDEF	int4			gtm_object_size;	/* Size of entire gtm object for compiler use */
GBLDEF	int4			linkage_size;		/* Size of linkage section during compile */
GBLDEF	uint4			lnkrel_cnt;		/* number of entries in linkage Psect to relocate */
GBLDEF  int4			sym_table_size;		/* size of the symbol table during compilation */
GBLDEF	boolean_t		stop_non_mandatory_expansion, non_mandatory_expansion; /* Used in stringpool managment */
GBLDEF	jnl_fence_control	jnl_fence_ctl;
GBLDEF	jnl_process_vector	*prc_vec;		/* for current process */
GBLDEF	jnl_process_vector	*originator_prc_vec;	/* for client/originator */
LITDEF	char	*jrt_label[JRT_RECTYPES] =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, update, fixed_size, is_replicated)	label,
#include "jnl_rec_table.h"	/* BYPASSOK */
#undef JNL_TABLE_ENTRY
};
LITDEF	int	jrt_update[JRT_RECTYPES] =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, update, fixed_size, is_replicated)	update,
#include "jnl_rec_table.h"	/* BYPASSOK */
#undef JNL_TABLE_ENTRY
};
LITDEF	boolean_t	jrt_fixed_size[JRT_RECTYPES] =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, update, fixed_size, is_replicated)	fixed_size,
#include "jnl_rec_table.h"	/* BYPASSOK */
#undef JNL_TABLE_ENTRY
};
LITDEF	boolean_t	jrt_is_replicated[JRT_RECTYPES] =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, update, fixed_size, is_replicated)	is_replicated,
#include "jnl_rec_table.h"	/* BYPASSOK */
#undef JNL_TABLE_ENTRY
};
LITDEF	char	*jnl_file_state_lit[JNL_FILE_STATES] =
{
	"JNL_FILE_UNREAD",
	"JNL_FILE_OPEN",
	"JNL_FILE_CLOSED",
	"JNL_FILE_EMPTY"
};
/* Change the initialization if struct_jrec_tcom in jnl.h changes */
GBLDEF	struct_jrec_tcom	tcom_record = {{JRT_TCOM, TCOM_RECLEN, 0, 0, 0, 0},
					0, 0, 0, 0, "", {TCOM_RECLEN, JNL_REC_SUFFIX_CODE}};
GBLDEF	jnl_gbls_t		jgbl;
GBLDEF	short 		crash_count;
GBLDEF	trans_num	start_tn;
GBLDEF	cw_set_element	cw_set[CDB_CW_SET_SIZE];
GBLDEF	unsigned char	cw_set_depth, cw_map_depth;
GBLDEF	unsigned int	t_tries;
GBLDEF	uint4		t_err;
GBLDEF	uint4		update_trans;	/* Bitmask indicating among other things whether this region was updated;
					 * See gdsfhead.h for UPDTRNS_* bitmasks
					 * Bit-0 is 1 if cw_set_depth is non-zero or if it is a duplicate set
					 * 	(cw_set_depth is zero in that case).
					 * Bit-1 is unused for non-TP.
					 * Bit-2 is 1 if transaction commit in this region is beyond point of rollback.
					 */

GBLDEF	boolean_t	is_uchar_wcs_code[] = 	/* uppercase failure codes that imply database cache related problem */
{	/* if any of the following failure codes are seen in the final retry, wc_blocked will be set to trigger cache recovery */
#define	CDB_SC_NUM_ENTRY(code, value)
#define CDB_SC_UCHAR_ENTRY(code, is_wcs_code, value)	is_wcs_code,
#define	CDB_SC_LCHAR_ENTRY(code, is_wcs_code, value)
#include "cdb_sc_table.h"	/* BYPASSOK */
#undef CDB_SC_NUM_ENTRY
#undef CDB_SC_UCHAR_ENTRY
#undef CDB_SC_LCHAR_ENTRY
};
GBLDEF	boolean_t	is_lchar_wcs_code[] = 	/* lowercase failure codes that imply database cache related problem */
{	/* if any of the following failure codes are seen in the final retry, wc_blocked will be set to trigger cache recovery */
#define	CDB_SC_NUM_ENTRY(code, value)
#define CDB_SC_UCHAR_ENTRY(code, is_wcs_code, value)
#define	CDB_SC_LCHAR_ENTRY(code, is_wcs_code, value)	is_wcs_code,
#include "cdb_sc_table.h"	/* BYPASSOK */
#undef CDB_SC_NUM_ENTRY
#undef CDB_SC_UCHAR_ENTRY
#undef CDB_SC_LCHAR_ENTRY
};
GBLDEF	boolean_t	gvdupsetnoop = TRUE;	/* if TRUE, duplicate SETs do not change GDS block (and therefore no PBLK journal
						 * records will be written) although the database transaction number will be
						 * incremented and logical SET journal records will be written. By default, this
						 * behavior is turned ON. GT.M has a way of turning it off with a VIEW command.
						 */
GBLDEF boolean_t	gtm_fullblockwrites;	/* Do full (not partial) database block writes T/F */
UNIX_ONLY(GBLDEF int4	gtm_shmflags;)		/* Extra flags for shmat */
#ifdef VMS
GBLDEF	uint4	gtm_memory_noaccess_defined;	/* count of the number of GTM_MEMORY_NOACCESS_ADDR logicals which are defined */
GBLDEF	uint4	gtm_memory_noaccess[GTM_MEMORY_NOACCESS_COUNT];	/* see VMS gtm_env_init_sp.c */
#endif
GBLDEF	volatile boolean_t	in_wcs_recover;	/* TRUE if in "wcs_recover", used by "bt_put" and "generic_exit_handler" */
GBLDEF	boolean_t	in_gvcst_incr;		/* set to TRUE by gvcst_incr, set to FALSE by gvcst_put
						 * distinguishes to gvcst_put, if the current db operation is a SET or $INCR */
GBLDEF	mval		*post_incr_mval;	/* mval pointing to the post-$INCR value */
GBLDEF	mval		increment_delta_mval;	/* mval holding the INTEGER increment value, set by gvcst_incr,
						 * used by gvcst_put/gvincr_recompute_upd_array which is invoked by t_end */
GBLDEF	boolean_t	is_dollar_incr;		/* valid only if gvcst_put is in the call-stack (i.e. t_err == ERR_GVPUTFAIL);
						 * is a copy of "in_gvcst_incr" just before it got reset to FALSE */
GBLDEF	int		indir_cache_mem_size;	/* Amount of memory currently in use by indirect cache */
GBLDEF	hash_table_objcode cache_table;
GBLDEF  int		cache_hits, cache_fails;
/* The alignment feature is disabled due to some issues in stringpool garbage collection.
 * TODO: When we sort out stringpool issues, change mstr_native_align to TRUE below */
GBLDEF	boolean_t	mstr_native_align;
GBLDEF boolean_t	save_mstr_native_align;
GBLDEF	mvar		*mvartab;
GBLDEF	mvax		*mvaxtab,*mvaxtab_end;
GBLDEF	mlabel		*mlabtab;
GBLDEF	mline		mline_root;
GBLDEF	mline		*mline_tail;
GBLDEF	short int	block_level;
GBLDEF	triple		t_orig;
GBLDEF	int		mvmax, mlmax, mlitmax;
static	char		routine_name_buff[SIZEOF(mident_fixed)], module_name_buff[SIZEOF(mident_fixed)];
static	char		int_module_name_buff[SIZEOF(mident_fixed)];
GBLDEF	MIDENT_DEF(routine_name, 0, &routine_name_buff[0]);
GBLDEF	MIDENT_DEF(module_name, 0, &module_name_buff[0]);
GBLDEF	MIDENT_DEF(int_module_name, 0, &int_module_name_buff[0]);
GBLDEF	char		rev_time_buf[REV_TIME_BUFF_LEN];
GBLDEF	unsigned short	source_name_len;
GBLDEF	short		object_name_len;
UNIX_ONLY(
	GBLDEF unsigned char	source_file_name[MAX_FBUFF + 1];
	GBLDEF unsigned char	object_file_name[MAX_FBUFF + 1];
	GBLDEF int		object_file_des;
)
VMS_ONLY(
	GBLDEF char		source_file_name[PATH_MAX];
	GBLDEF char		object_file_name[256];
	GBLDEF struct FAB	obj_fab;	/* file access block for the object file */
)
GBLDEF	int4		curr_addr, code_size;
GBLDEF	mident_fixed	zlink_mname;
GBLDEF	sm_uc_ptr_t	reformat_buffer;
GBLDEF	int		reformat_buffer_len;
GBLDEF	volatile int	reformat_buffer_in_use;	/* used only in DEBUG mode */
GBLDEF	boolean_t	mu_reorg_upgrd_dwngrd_in_prog;	/* TRUE if MUPIP REORG UPGRADE/DOWNGRADE is in progress */
GBLDEF	boolean_t	mu_reorg_nosafejnl;		/* TRUE if NOSAFEJNL explicitly specified */
GBLDEF	trans_num	mu_reorg_upgrd_dwngrd_blktn;	/* tn in blkhdr of current block processed by MUPIP REORG {UP,DOWN}GRADE */
GBLDEF	inctn_opcode_t	inctn_opcode = inctn_invalid_op;
GBLDEF	inctn_detail_t	inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLDEF	uint4		region_open_count;		/* Number of region "opens" we have executed */
GBLDEF	uint4		gtm_blkupgrade_flag = UPGRADE_IF_NEEDED;	/* by default upgrade only if necessary */
GBLDEF	boolean_t	disk_blk_read;
GBLDEF	boolean_t	gtm_dbfilext_syslog_disable;	/* by default, log every file extension message */
GBLDEF	int4		cws_reorg_remove_index;			/* see mu_swap_blk.c for comments on the need for these two */
GBLDEF	block_id	cws_reorg_remove_array[CWS_REORG_REMOVE_ARRAYSIZE];
GBLDEF	uint4		log_interval;
#ifdef UNIX
GBLDEF	uint4		gtm_principal_editing_defaults;	/* ext_cap flags if tt */
GBLDEF	boolean_t	in_repl_inst_edit;		/* used by an assert in repl_inst_read/repl_inst_write */
GBLDEF	boolean_t	in_repl_inst_create;		/* used by repl_inst_read/repl_inst_write */
GBLDEF	boolean_t	holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];	/* whether a particular replication semaphore is being held
								 * by the current process or not. */
GBLDEF	boolean_t	detail_specified;	/* Set to TRUE if -DETAIL is specified in MUPIP REPLIC -JNLPOOL or -EDITINST */
GBLDEF	boolean_t	in_mupip_ftok;		/* Used by an assert in repl_inst_read */
GBLDEF	uint4		section_offset;		/* Used by PRINT_OFFSET_PREFIX macro in repl_inst_dump.c */
GBLDEF	uint4		mutex_per_process_init_pid;	/* pid that invoked "mutex_per_process_init" */
GBLDEF	boolean_t	gtm_quiet_halt;		/* Suppress FORCEDHALT message */
#endif
#ifdef UNICODE_SUPPORTED
/* Unicode line terminators.  In addition to the following
 * codepoints, the sequence CR LF is considered a single
 * line terminator.
 */
LITDEF UChar32 u32_line_term[] =
{
	0x000A,			/* Line Feed */
	0x000D,			/* Carraige Return */
	0x0085,			/* Next Line - EBCDIC mostly */
	0x000C,			/* Form Feed */
	UTF_LINE_SEPARATOR,	/* Line Separator */
	UTF_PARA_SEPARATOR,	/* Paragraph Separator */
	0x0000
};
/* Given the first byte in a UTF-8 representation, the following array returns the total number of bytes in the encoding
 *	00-7F : 1 byte
 *	C2-DF : 2 bytes
 *	E0-EF : 3 bytes
 *	F0-F4 : 4 bytes
 *  All others: 1 byte
 * For details on the UTF-8 encoding see gtm_utf8.h
 */
LITDEF unsigned int utf8_bytelen[] =
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 00-1F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 20-3F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 40-5F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 60-7F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 80-9F */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* A0-BF */
	1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* C0-DF */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* E0-FF */
};
/* Given the first byte in a UTF-8 representation, the following array returns the number of bytes to follow
 *	00-7F :  0 byte
 *	C2-DF :  1 byte
 *	E0-EF :  2 bytes
 *	F0-F4 :  3 bytes
 *  All others: -1 bytes
 * For details on the UTF-8 encoding see gtm_utf8.h
 */
LITDEF signed int utf8_followlen[] =
{
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 00-1F */
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 20-3F */
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 40-5F */
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 60-7F */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 80-9F */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* A0-BF */
	-1,-1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* C0-DF */
	 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* E0-FF */
};
GBLDEF	gtm_wcswidth_fnptr_t	gtm_wcswidth_fnptr;	/* see comment in gtm_utf8.h about this typedef */
#endif
GBLDEF	uint4			gtm_max_sockets;	/* Maximum sockets per socket device supported by this process */
GBLDEF	d_socket_struct		*newdsocket;		/* Commonly used temp socket area */
GBLDEF	boolean_t		dse_all_dump;		/* TRUE if DSE ALL -DUMP is specified */
GBLDEF	int			socketus_interruptus;	/* How many times socket reads have been interrutped */
GBLDEF	int4			pending_errtriplecode;	/* if non-zero contains the error code to invoke ins_errtriple with */
GBLDEF	uint4	process_id;
GBLDEF	uint4	image_count;	/* not used in UNIX but defined to preserve VMS compatibility */
GBLDEF  size_t  totalRmalloc;                           /* Total storage currently (real) malloc'd (includes extent blocks) */
GBLDEF  size_t  totalAlloc;                             /* Total allocated (includes allocation overhead but not free space */
GBLDEF  size_t  totalUsed;                              /* Sum of user allocated portions (totalAlloc - overhead) */
GBLDEF	size_t	totalRallocGta;				/* Total storage currently (real) mmap alloc'd */
GBLDEF	size_t  totalAllocGta;                          /* Total mmap allocated (includes allocation overhead but not free space */
GBLDEF	size_t  totalUsedGta;                           /* Sum of "in-use" portions (totalAllocGta - overhead) */
GBLDEF	volatile char		*outOfMemoryMitigation;	/* Cache that we will freed to help cleanup if run out of memory */
GBLDEF	uint4			outOfMemoryMitigateSize;/* Size of above cache (in Kbytes) */
GBLDEF	int 			mcavail;
GBLDEF	mcalloc_hdr 		*mcavailptr, *mcavailbase;
GBLDEF	uint4			max_cache_memsize;	/* Maximum bytes used for indirect cache object code */
GBLDEF	uint4			max_cache_entries;	/* Maximum number of cached indirect compilations */
GBLDEF  void            (*cache_table_relobjs)(void);   /* Function pointer to call cache_table_rebuild() */
UNIX_ONLY(GBLDEF ch_ret_type (*ht_rhash_ch)());         /* Function pointer to hashtab_rehash_ch */
UNIX_ONLY(GBLDEF ch_ret_type (*jbxm_dump_ch)());        /* Function pointer to jobexam_dump_ch */
UNIX_ONLY(GBLDEF ch_ret_type (*stpgc_ch)());		/* Function pointer to stp_gcol_ch */
#ifdef VMS
GBLDEF	boolean_t		tp_has_kill_t_cse; /* cse->mode of kill_t_write or kill_t_create got created in this transaction */
#endif
GBLDEF	cache_rec_ptr_t	pin_fail_cr;			/* Pointer to the cache-record that we failed while pinning */
GBLDEF	cache_rec	pin_fail_cr_contents;		/* Contents of the cache-record that we failed while pinning */
GBLDEF	cache_rec_ptr_t	pin_fail_twin_cr;		/* Pointer to twin of the cache-record that we failed to pin */
GBLDEF	cache_rec	pin_fail_twin_cr_contents;	/* Contents of twin of the cache-record that we failed to pin */
GBLDEF	bt_rec_ptr_t	pin_fail_bt;			/* Pointer to bt of the cache-record that we failed to pin */
GBLDEF	bt_rec		pin_fail_bt_contents;		/* Contents of bt of the cache-record that we failed to pin */
GBLDEF	int4		pin_fail_in_crit;		/* Holder of crit at the time we failed to pin */
GBLDEF	int4		pin_fail_wc_in_free;		/* Number of write cache records in free queue when we failed to pin */
GBLDEF	int4		pin_fail_wcs_active_lvl;	/* Number of entries in active queue when we failed to pin */
GBLDEF	int4		pin_fail_ref_cnt;		/* Reference count when we failed to pin */
GBLDEF	int4		pin_fail_in_wtstart;		/* Count of processes in wcs_wtstart when we failed to pin */
GBLDEF	int4		pin_fail_phase2_commit_pidcnt;	/* Number of processes in phase2 commit when we failed to pin */
GBLDEF	zwr_hash_table	*zwrhtab;			/* How we track aliases during zwrites */
GBLDEF	uint4		zwrtacindx;			/* When creating $ZWRTACxxx vars for ZWRite, this holds xxx */
GBLDEF	uint4		tstartcycle;			/* lv_val cycle for tstart operations */
GBLDEF	uint4		lvtaskcycle;			/* lv_val cycle for misc lv_val related tasks */
GBLDEF	int4		SPGC_since_LVGC;			/* stringpool GCs since the last lv_val GC */
GBLDEF	int4		LVGC_interval = MIN_SPGC_PER_LVGC;	/* dead data GC is done every LVGC_interval stringpool GCs */
GBLDEF	lv_xnew_var	*xnewvar_anchor;		/* Anchor for unused lv_xnew_var blocks */
GBLDEF	lv_xnew_ref	*xnewref_anchor;		/* Anchor for unused lv_xnew_ref blocks */
GBLDEF	mval		*alias_retarg;			/* Points to an alias return arg created by a "QUIT *" recorded here so
							 * symtab-unwind logic can find it and modify it if necessary if a
							 * symtab popped during the return and the retarg points to an lv_val
							 * that is going to be destroyed.
							 */
#ifdef DEBUG_ALIAS
GBLDEF	boolean_t	lvmon_enabled;			/* Enable lv_val monitoring */
#endif
GBLDEF	block_id	gtm_tp_allocation_clue;		/* block# hint to start allocation for created blocks in TP */
#ifdef UNIX
GBLDEF	int4		gtm_zlib_cmp_level;		/* zlib compression level specified at process startup */
GBLDEF	int4		repl_zlib_cmp_level;		/* zlib compression level currently in use in replication pipe.
							 * This is a source-server specific variable and is non-zero only
							 * if compression is enabled and works in the receiver server as well.
							 */
GBLDEF	zlib_cmp_func_t		zlib_compress_fnptr;
GBLDEF	zlib_uncmp_func_t	zlib_uncompress_fnptr;
#endif
GBLDEF	mlk_stats_t	mlk_stats;			/* Process-private M-lock statistics */
#ifdef UNIX
/* Initialized blockalrm, block_ttinout and block_sigsent can be used by all threads */
GBLDEF	boolean_t	blocksig_initialized;		/* set to TRUE when blockalrm and block_sigsent are initialized */
GBLDEF	sigset_t	blockalrm;
UNIX_ONLY(GBLDEF	sigset_t	block_ttinout;)
GBLDEF	sigset_t	block_sigsent;	/* block all signals that can be sent externally
					  (SIGINT, SIGQUIT, SIGTERM, SIGTSTP, SIGCONT) */
GBLDEF  char            *gtm_core_file;
GBLDEF  char            *gtm_core_putenv;
#endif
#ifdef __MVS__
GBLDEF	char		*gtm_utf8_locale_object;
GBLDEF	boolean_t	gtm_tag_utf8_as_ascii = TRUE;
#endif
#ifdef GTM_CRYPT
LITDEF	char			gtmcrypt_repeat_msg[] = "Please look at prior messages related to encryption for more details";
GBLDEF	boolean_t		gtmcrypt_initialized;	/* Set to TRUE if gtmcrypt_init() completes successfully */
GBLDEF	char			dl_err[MAX_ERRSTR_LEN];
GBLDEF	gtmcrypt_init_t			gtmcrypt_init_fnptr;
GBLDEF  gtmcrypt_close_t        	gtmcrypt_close_fnptr;
GBLDEF  gtmcrypt_hash_gen_t		gtmcrypt_hash_gen_fnptr;
GBLDEF  gtmcrypt_encrypt_t		gtmcrypt_encrypt_fnptr;
GBLDEF  gtmcrypt_decrypt_t		gtmcrypt_decrypt_fnptr;
GBLDEF	gtmcrypt_getkey_by_hash_t	gtmcrypt_getkey_by_hash_fnptr;
GBLDEF	gtmcrypt_getkey_by_name_t	gtmcrypt_getkey_by_name_fnptr;
GBLDEF	gtmcrypt_strerror_t		gtmcrypt_strerror_fnptr;
#endif /* GTM_CRYPT */
#ifdef DEBUG
/* Following definitions are related to white_box testing */
GBLDEF	boolean_t	gtm_white_box_test_case_enabled;
GBLDEF	int		gtm_white_box_test_case_number;
GBLDEF	int		gtm_white_box_test_case_count;
GBLDEF	int 		gtm_wbox_input_test_case_count; /* VMS allows maximum 31 characters for external identifer */
GBLDEF	boolean_t	stringpool_unusable;		/* Set to TRUE by any function that does not expect any of its function
							 * callgraph to use/expand the stringpool. */
GBLDEF	boolean_t	stringpool_unexpandable;	/* Set to TRUE by any function for a small period when it has ensured
							 * enough space in the stringpool so it does not expect any more garbage
							 * collections or expansions.
							 */
GBLDEF	boolean_t	donot_INVOKE_MUMTSTART;		/* Set to TRUE whenever an implicit TSTART is done in gvcst_put/kill as
							 * part of an explicit + trigger update. In this situation, we dont expect
							 * MUM_TSTART macro to be invoked at all (see skip_INVOKE_RESTART below
							 * for description on why this is needed). So we keep this debug-only
							 * flag turned on throughout the gvcst_put/kill. An assert in
							 * mdb_condition_handler (invoked by INVOKE_RESTART macro when it does
							 * an rts_error) checks it never gets invoked while this is set.
							 */
#endif
GBLDEF	boolean_t	block_is_free;			/* Set to TRUE if the caller wants to let t_qread know that the block it is
							 * attempting to read is actually a FREE block
							 */
GBLDEF	int4		gv_keysize;
GBLDEF	gd_addr		*gd_header;
GBLDEF	gd_binding	*gd_map;
GBLDEF	gd_binding	*gd_map_top;
#ifdef GTM_TRIGGER
GBLDEF	int4		gtm_trigger_depth;		/* 0 if no trigger, 1 if inside trigger; 2 if inside nested trigger etc. */
GBLDEF	int4		tstart_trigger_depth;		/* gtm_trigger_depth at the time of the outermost "op_tstart"
							 * (i.e. explicit update). This should be used only if dollar_tlevel
							 * is non-zero as it is not otherwise maintained.
							 */
GBLDEF	uint4		trigger_name_cntr;		/* Counter from which trigger names are constructed */
GBLDEF	boolean_t 	*ztvalue_changed_ptr;		/* -> boolean in current gtm_trigger_parms signaling if ztvalue has
							      been updated */
GBLDEF	boolean_t	ztwormhole_used;		/* TRUE if $ztwormhole was used by trigger code */
GBLDEF	mstr		*dollar_ztname;
GBLDEF	mval		*dollar_ztdata,
			*dollar_ztoldval,
			*dollar_ztriggerop,
			*dollar_ztupdate,
			*dollar_ztvalue,
			dollar_ztwormhole = DEFINE_MVAL_STRING(MV_STR, 0, 0, 0, NULL, 0, 0),
			dollar_ztslate = DEFINE_MVAL_STRING(MV_STR, 0, 0, 0, NULL, 0, 0),
			gtm_trigger_etrap;  		/* Holds $ETRAP value for inside trigger */
GBLDEF	int		tprestart_state;		/* When triggers restart, multiple states possible. See tp_restart.h */
GBLDEF	boolean_t	skip_INVOKE_RESTART;		/* set to TRUE if caller of op_tcommit/t_retry does not want it to
							 * use the INVOKE_RESTART macro (which uses an rts_error to trigger
							 * the restart) instead return code. The reason we dont want to do
							 * rts_error is that this is an implicit tstart situation where we
							 * did not do opp_tstart.s so we dont want control to be transferred
							 * using the MUM_TSTART macro by mdb_condition_handler (which assumes
							 * opp_tstart.s invocation).
							 */
GBLDEF	boolean_t	goframes_unwound_trigger;	/* goframes() unwound a trigger base frame during its unwinds */
GBLDEF	symval		*trigr_symval_list;		/* List of availalable symvals for use in (nested) triggers */
GBLDEF	boolean_t	dollar_ztrigger_invoked;	/* $ZTRIGGER() was invoked on at least one region in this transaction */
# ifdef DEBUG
  GBLDEF gv_trigger_t		*gtm_trigdsc_last;	/* For debugging purposes - parms gtm_trigger called with */
  GBLDEF gtm_trigger_parms	*gtm_trigprm_last;
  GBLDEF ch_ret_type		(*ch_at_trigger_init)();	/* Condition handler in effect when gtm_trigger called */
# endif
GBLDEF	boolean_t	explicit_update_repl_state;	/* Initialized just before an explicit update invokes any triggers.
							 * Set to 1 if triggering update is to a replicated database.
							 * Set to 0 if triggering update is to a non-replicated database.
							 * Value stays untouched across nested trigger invocations.
							 */
#endif
GBLDEF	boolean_t	skip_dbtriggers;		/* Set to FALSE by default (i.e. triggers are invoked). Set to TRUE
							 * unconditionally by MUPIP LOAD as it always skips triggers. Also set
							 * to TRUE by journal recovery/update-process only when they encounter
							 * updates done by MUPIP LOAD so they too skip trigger processing. In the
							 * case of update process, this keeps primary/secondary in sync. In the
							 * case of journal recovery, this keeps the db and jnl in sync.
							 */
GBLDEF	boolean_t	expansion_failed;		/* used by string pool when trying to expand */
GBLDEF	boolean_t	retry_if_expansion_fails;	/* used by string pool when trying to expand */

GBLDEF	boolean_t	mupip_exit_status_displayed;	/* TRUE if mupip_exit has already displayed a message for non-zero status.
							 * Used by mur_close_files to ensure some message gets printed in case
							 * of abnormal exit status (in some cases mupip_exit might not have been
							 * invoked but we will still go through mur_close_files e.g. if exit is
							 * done directly without invoking mupip_exit).
							 */
GBLDEF	boolean_t	implicit_trollback;		/* Set to TRUE by OP_TROLLBACK macro before calling op_trollback. Set
							 * to FALSE by op_trollback. Used to indicate op_trollback as to
							 * whether it is being called from generated code (opp_trollback.s)
							 * or from C runtime code.
							 */
#ifdef DEBUG
GBLDEF	boolean_t	ok_to_UNWIND_in_exit_handling;	/* see gtm_exit_handler.c for comments */
GBLDEF	boolean_t	skip_block_chain_tail_check;
GBLDEF	boolean_t	in_mu_rndwn_file;		/* TRUE if we are in mu_rndwn_file (holding standalone access) */
#endif
GBLDEF	char		gvcst_search_clue;
#ifdef UNIX
/* The following are replication related global variables. Ideally if we had a repl_gbls_t structure (like jnl_gbls_t)
 * this would be a member in that. But since we dont have one and since we need to initialize this specificially to a
 * non-zero value (whereas usually everything else accepts a 0 default value), this is better kept as a separate global
 * variable instead of inside a global variable structure.
 */
GBLDEF	int4			strm_index = INVALID_SUPPL_STRM;
						/* # of the supplementary stream if one exists.
						 * If this process is an update process running on a supplementary
						 *   replication instance and the journal pool allows updates (that
						 *   is the instance was started as a root primary), the value of
						 *   this variable will be anywhere from 1 to 15 once the receiver
						 *   establishes connection with a non-supplementary source server.
						 * If this process is a mumps process running on a supplementary
						 *   replication instance and the journal pool allows updates (that
						 *   is the instance was started as a root primary), the value of
						 *   this variable will be 0 to indicate this is the local stream.
						 * Otherwise, this variable is set to -1 (INVALID_SUPPL_STRM)
						 * This variable is used by t_end/tp_tend to determine if strm_seqno
						 *   field in the journal record needs to be filled in or not.
						 *   It is filled in only if this variable is 0 to 15.
						 */
GBLDEF  repl_conn_info_t        *this_side, *remote_side;
/* Replication related global variables END */
GBLDEF	seq_num			gtmsource_save_read_jnl_seqno;
GBLDEF	gtmsource_state_t	gtmsource_state = GTMSOURCE_DUMMY_STATE;
#endif
GBLDEF	boolean_t	gv_play_duplicate_kills;	/* A TRUE value implies KILLs of non-existent nodes will continue to
							 * write jnl records and increment the db curr_tn even though they dont
							 * touch any GDS blocks in the db (i.e. treat it as a duplicate kill).
							 * Set to TRUE for the update process & journal recovery currently.
							 * Set to FALSE otherwise.
							 */
GBLDEF	boolean_t	donot_fflush_NULL;		/* Set to TRUE whenever we dont want gtm_putmsg to fflush(NULL). BYPASSOK
							 * As of Jan 2012, mu_rndwn_all is the only user of this functionality.
							 */
#ifdef UNIX
GBLDEF	boolean_t	jnlpool_init_needed;		/* TRUE if jnlpool_init should be done at database init time (eg., for
							 * anticipatory freeze supported configurations). The variable is set
							 * explicitly by interested commands (eg., MUPIP REORG).
							 */
GBLDEF	boolean_t	span_nodes_disallowed; 		/* Indicates whether spanning nodes are not allowed. For example,
							 * they are not allowed for GT.CM OMI and GNP. */
GBLDEF	boolean_t	argumentless_rundown;
GBLDEF	is_anticipatory_freeze_needed_t		is_anticipatory_freeze_needed_fnptr;
GBLDEF	set_anticipatory_freeze_t		set_anticipatory_freeze_fnptr;
GBLDEF	boolean_t				is_jnlpool_creator;
GBLDEF	char		gtm_dist[GTM_PATH_MAX];		/* Value of $gtm_dist env variable */
#endif
GBLDEF	boolean_t	in_jnl_file_autoswitch;	/* Set to TRUE for a short window inside jnl_file_extend when we are about to
						 * autoswitch; used by jnl_write. */
#ifdef GTM_PTHREAD
GBLDEF	pthread_t	gtm_main_thread_id;		/* ID of the main GT.M thread. */
GBLDEF	boolean_t	gtm_main_thread_id_set;		/* Indicates whether the thread ID is set. */
GBLDEF	boolean_t	gtm_jvm_process;		/* Indicates whether we are running with JVM or stand-alone. */
#endif
GBLDEF	size_t		gtm_max_storalloc;	/* Maximum that GTM allows to be allocated - used for testing */
#ifdef VMS
GBLDEF		sgmnt_addrs	*vms_mutex_check_csa;	/* On VMS, mutex_deadlock_check() is directly called from mutex.mar. In
							 * order to avoid passing csa parameter from the VMS assembly, we set this
							 * global from mutex_lock* callers.
							 */
#endif
GBLDEF	boolean_t	ipv4_only;		/* If TRUE, only use AF_INET.
						 * Reflects the value of the gtm_ipv4_only environment variable, so is process wide.
						 */
