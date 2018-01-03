/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include <sys/shm.h>
#include "gtm_permissions.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_tempnam.h"
#include "gtm_time.h"

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdsbgtr.h"
#include "filestruct.h"
#include "ast.h"
#include "cli.h"
#include "iosp.h"
#include "error.h"
#include "mupipbckup.h"
#include "stp_parms.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "io.h"
#include "interlock.h"
#include "lockconst.h"
#include "sleep_cnt.h"
#include "gtmio.h"		/* for OPENFILE macro */
#include "repl_sp.h"		/* for F_CLOSE macro */
#include "gtm_ipc.h"
#include "repl_instance.h"
#include "mu_gv_cur_reg_init.h"
#include "ftok_sems.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "do_shmat.h"		/* for do_shmat() prototype */
#include "mutex.h"
#include "gtm_file_stat.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "gt_timer.h"
#include "is_proc_alive.h"
#include "is_file_identical.h"
#include "dbfilop.h"
#include "mupip_exit.h"
#include "mu_getlst.h"
#include "mu_outofband_setup.h"
#include "gtmmsg.h"
#include "wcs_backoff.h"
#include "wcs_sleep.h"
#include "wcs_flu.h"
#include "trans_log_name.h"
#include "shmpool.h"
#include "mupip_backup.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "add_inter.h"
#include "gtm_logicals.h"
#include "gtm_c_stack_trace.h"
#include "have_crit.h"
#include "repl_sem.h"
#include "gtm_sem.h"
#include "anticipatory_freeze.h"

#define PATH_DELIM		'/'

#define TMPDIR_ACCESS_MODE	(R_OK | W_OK | X_OK)

GBLDEF  boolean_t	backup_started;
GBLDEF  boolean_t	backup_interrupted;

GBLREF 	bool		record;
GBLREF 	bool		error_mupip;
GBLREF 	bool		file_backed_up;
GBLREF 	bool		incremental;
GBLREF 	bool		online;
GBLREF 	int4		mubmaxblk;
GBLREF	tp_region	*grlist;
GBLREF 	tp_region 	*halt_ptr;
GBLREF 	bool		in_backup;
GBLREF 	bool		mu_ctrly_occurred;
GBLREF 	bool		mu_ctrlc_occurred;
GBLREF 	bool		mubtomag;
GBLREF 	gd_region	*gv_cur_region;
GBLREF 	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data_ptr_t cs_data;
GBLREF 	mstr		directory;
GBLREF	uint4		process_id;
GBLREF	uint4		image_count;
GBLREF 	boolean_t 	debug_mupip;
GBLREF	char		*before_image_lit[];
GBLREF	char		*jnl_state_lit[];
GBLREF	char		*repl_state_lit[];
GBLREF	jnl_gbls_t	jgbl;
GBLREF	void            (*call_on_signal)();
GBLREF	gd_addr		*gd_header;
#ifdef DEBUG
GBLREF  int		process_exiting;		/* Process is on it's way out */
#endif

GBLREF	boolean_t		jnlpool_init_needed;
GBLREF	backup_reg_list		*mu_repl_inst_reg_list;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	jnlpool_addrs_ptr_t	jnlpool_head;
GBLREF	uint4			mutex_per_process_init_pid;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	int			pool_init;

LITREF char             gtm_release_name[];
LITREF int4             gtm_release_name_len;

error_def(ERR_BACKUPCTRL);
error_def(ERR_BACKUPKILLIP);
error_def(ERR_BKUPRUNNING);
error_def(ERR_NOMORESEMCNT);
error_def(ERR_DBCCERR);
error_def(ERR_DBFILERR);
error_def(ERR_DBRDONLY);
error_def(ERR_DBROLLEDBACK);
error_def(ERR_ERRCALL);
error_def(ERR_FILEEXISTS);
error_def(ERR_FILEPARSE);
error_def(ERR_FREEZECTRL);
error_def(ERR_JNLCREATE);
error_def(ERR_JNLDISABLE);
error_def(ERR_JNLFILOPN);
error_def(ERR_JNLFNF);
error_def(ERR_JNLNOCREATE);
error_def(ERR_JNLPOOLSETUP);
error_def(ERR_JNLSTATE);
error_def(ERR_KILLABANDONED);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUNOSTRMBKUP);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUSELFBKUP);
error_def(ERR_NOTRNDMACC);
error_def(ERR_OFRZACTIVE);
error_def(ERR_PERMGENFAIL);
error_def(ERR_PREVJNLLINKCUT);
error_def(ERR_REPLINSTUNDEF);
error_def(ERR_REPLJNLCNFLCT);
error_def(ERR_REPLPOOLINST);
error_def(ERR_REPLREQROLLBACK);
error_def(ERR_REPLSTATE);
error_def(ERR_REPLSTATEERR);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_FILENAMETOOLONG);
ZOS_ONLY(error_def(ERR_BADTAG);)

static char	* const jnl_parms[] =
{
	"DISABLE",
	"NOPREVJNLFILE",
	"OFF"
};
enum
{
	jnl_disable,
	jnl_noprevjnlfile,
	jnl_off,
	jnl_end_of_list
};

void mupip_backup_call_on_signal(void)
{	/* Called if mupip backup is terminated by a signal. Performs cleanup of temporary files and shutdown backup. */
	assert(NULL == call_on_signal);	/* Should have been reset by signal handling caller before invoking this function.
					 * This will ensure we do not recurse via call_on_signal if there is another error.
					 */
	assert(process_exiting);	/* should have been set by caller */
	if (backup_started)
	{	/* Cleanup that which we have messed */
		backup_interrupted = TRUE;
		mubclnup(NULL, need_to_del_tempfile);
	}
}

/* When we have crit, check if this region is actively journaled and if gbl_jrec_time needs to be
 * adjusted (to ensure time ordering of journal records within this region's journal file).
 * This needs to be done BEFORE writing any journal records for this region. The value of
 * jgbl.gbl_jrec_time at the end of this loop will be used to write journal records for ALL
 * regions so all regions will have same eov/bov timestamps.
 */
#define UPDATE_GBL_JREC_TIME												\
{															\
	if (JNL_ENABLED(cs_data))											\
	{														\
		jpc = cs_addrs->jnl;											\
		jbp = jpc->jnl_buff;											\
		ADJUST_GBL_JREC_TIME(jgbl, jbp);									\
	}														\
}

void mupip_backup(void)
{
	bool			journal;
	char			*tempfilename, *ptr;
	uint4			status, ret, kip_count;
	unsigned short		s_len, length;
	int4			size, crit_counter, save_errno, rv, orig_buff_size;
	uint4			ustatus, reg_count;
	trans_num		tn;
	shmpool_buff_hdr_ptr_t	sbufh_p;
	shmpool_blk_hdr_ptr_t	sblkh_p, next_sblkh_p;
	backup_reg_list		*rptr, *rrptr, *nocritrptr;
	boolean_t		inc_since_inc , inc_since_rec, result, newjnlfiles, tn_specified,
				replication_on, newjnlfiles_specified, keep_prev_link, bkdbjnl_disable_specified,
				bkdbjnl_off_specified;
	unsigned char		since_buff[50];
	jnl_create_info		jnl_info;
	file_control		*fc;
	char			tempdir_trans_buffer[MAX_TRANS_NAME_LEN],
				tempnam_prefix[MAX_FN_LEN], tempdir_full_buffer[MAX_FN_LEN + 1];
	char			*jnl_str_ptr, jnl_str[256], entry[256], prev_jnl_fn[JNL_NAME_SIZE];
	int			index, jnl_fstat;
	mstr			tempdir_log, tempdir_trans, *file, *rfile, *replinstfile, tempdir_full, filestr;
	uint4			jnl_status, temp_file_name_len, tempdir_trans_len, trans_log_name_status;
	boolean_t		jnl_options[jnl_end_of_list] = {FALSE, FALSE, FALSE}, save_no_prev_link;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	jnl_tm_t		save_gbl_jrec_time;
	gd_region		*r_save, *reg;
	int			sync_io_status;
	boolean_t		sync_io, sync_io_specified;
	boolean_t		dummy_ftok_counter_halted;
	void			*repl_inst_available;
	struct stat		stat_buf;
	int			fstat_res, fclose_res, tmpfd;
	gd_segment		*seg;
	char			*errptr, scndry_msg[OUT_BUFF_SIZE];
	unsigned int		full_len;
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	repl_inst_hdr		repl_instance, *save_inst_hdr;
	unsigned char		*cmdptr;
	unsigned char 		command[(GTM_PATH_MAX) * 2 + 5]; /*2 files in the cmd, 5 == SIZEOF("cp") + 2 (space) + 1 (NULL)*/
	struct shmid_ds		shm_buf;
	struct semid_ds		semstat;
	union semun		semarg;
	int4			shm_id, sem_id;
	replpool_identifier	replpool_id;
	DEBUG_ONLY(jnlpool_addrs_ptr_t	jnlpool_save);
	sm_uc_ptr_t		start_addr;
	int			user_id;
	int			group_id;
	int			perm;
	uint4			*kip_pids_arr_ptr;
	seq_num			jnl_seqno;
	char			time_str[CTIME_BEFORE_NL + 2];	/* for GET_CUR_TIME macro */
	struct perm_diag_data	pdd;
	ZOS_ONLY(int		realfiletag;)

	/* ==================================== STEP 1. Initialization ======================================= */
	backup_started = backup_interrupted = FALSE;
	ret = SS_NORMAL;
	jnl_str_ptr = &jnl_str[0];
	halt_ptr = grlist = NULL;
	in_backup = TRUE;
	inc_since_inc = inc_since_rec = file_backed_up = error_mupip = FALSE;
	debug_mupip = (CLI_PRESENT == cli_present("DBG"));
	call_on_signal = mupip_backup_call_on_signal;
	mu_outofband_setup();
	jnl_status = 0;
	if (NULL == gd_header)
		gvinit();
	/* ============================ STEP 2. Parse and construct grlist ================================== */
	tn_specified = FALSE;
	if (incremental = (CLI_PRESENT == cli_present("INCREMENTAL") || CLI_PRESENT == cli_present("BYTESTREAM")))
	{
		trans_num temp_tn;
		if (0 == cli_get_hex64("TRANSACTION", &temp_tn))
		{
			temp_tn = 0;
			s_len = SIZEOF(since_buff);
			if (cli_get_str("SINCE", (char *)since_buff, &s_len))
			{
				lower_to_upper(since_buff, since_buff, s_len);
				if ((0 == memcmp(since_buff, "INCREMENTAL", s_len))
					|| (0 == memcmp(since_buff, "BYTESTREAM", s_len)))
					inc_since_inc = TRUE;
				else if (0 == memcmp(since_buff, "RECORD", s_len))
					inc_since_rec = TRUE;
			}
		} else
		{
			tn_specified = TRUE;
			if (temp_tn < 1)
			{
				util_out_print("The minimum allowable transaction number is one.", TRUE);
				mupip_exit(ERR_MUNOACTION);
			}
		}
		tn = temp_tn;
	}
	online = (TRUE != cli_negated("ONLINE"));
	record = (CLI_PRESENT == cli_present("RECORD"));
	newjnlfiles_specified = FALSE;
	newjnlfiles = TRUE;	/* by default */
	keep_prev_link = TRUE;
	if (CLI_PRESENT == cli_present("NEWJNLFILES"))
	{
		newjnlfiles_specified = newjnlfiles = TRUE;
		if (CLI_NEGATED == cli_present("NEWJNLFILES.PREVLINK"))
			keep_prev_link = FALSE;
		sync_io_status = cli_present("NEWJNLFILES.SYNC_IO");
		sync_io_specified = TRUE;
		if (CLI_PRESENT == sync_io_status)
			sync_io = TRUE;
		else if (CLI_NEGATED == sync_io_status)
			sync_io = FALSE;
		else
			sync_io_specified = FALSE;
	} else if (CLI_NEGATED == cli_present("NEWJNLFILES"))
	{
		keep_prev_link = FALSE; /* for safety */
		newjnlfiles_specified = TRUE;
		newjnlfiles = FALSE;
	}
	replication_on = FALSE;
	if (CLI_PRESENT == cli_present("REPLICATION.ON")) /* REPLICATION.OFF is disabled at the CLI layer */
		replication_on = TRUE;
	bkdbjnl_disable_specified = FALSE;
	bkdbjnl_off_specified = FALSE;
	if (CLI_PRESENT == cli_present("BKUPDBJNL"))
	{
		if (CLI_PRESENT == cli_present("BKUPDBJNL.DISABLE"))
			bkdbjnl_disable_specified = TRUE;
		if (CLI_PRESENT == cli_present("BKUPDBJNL.OFF"))
			bkdbjnl_off_specified = TRUE;
	}
	journal = (CLI_PRESENT == cli_present("JOURNAL"));
	if (TRUE == cli_negated("JOURNAL"))
		jnl_options[jnl_disable] = TRUE;
	else if (journal)
	{
		s_len = SIZEOF(jnl_str);
		UNSUPPORTED_PLATFORM_CHECK;
		if (!CLI_GET_STR_ALL("JOURNAL", jnl_str_ptr, &s_len))
			mupip_exit(ERR_MUPCLIERR);
		while (*jnl_str_ptr)
		{
			if (!cli_get_str_ele(jnl_str_ptr, entry, &length, TRUE))
				mupip_exit(ERR_MUPCLIERR);
			for (index = 0;  index < jnl_end_of_list;  ++index)
				if (0 == strncmp(jnl_parms[index], entry, length))
				{
					jnl_options[index] = TRUE;
					break;
				}
			if (jnl_end_of_list == index)
			{
				util_out_print("Qualifier JOURNAL: Unrecognized option: !AD", TRUE, length, entry);
				mupip_exit(ERR_MUPCLIERR);
			}
			jnl_str_ptr += length;
			assert(',' == *jnl_str_ptr || !(*jnl_str_ptr));	/* either comma separator or end of option list */
			if (',' == *jnl_str_ptr)
				jnl_str_ptr++;  /* skip separator */
		}
		if (jnl_options[jnl_disable] && jnl_options[jnl_off])
		{
			util_out_print("Qualifier JOURNAL: DISABLE may not be specified with any other options", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		if (jnl_options[jnl_noprevjnlfile] && !newjnlfiles)
		{
			util_out_print("Qualifier JOURNAL: NOPREVJNLFILE may not be specified with NONEWJNLFILES qualifier", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	}
	mu_getlst("REG_NAME", SIZEOF(backup_reg_list));
	if (error_mupip)
	{
		mubclnup(NULL, need_to_free_space);
		util_out_print("!/MUPIP cannot start backup with above errors!/", TRUE);
		mupip_exit(ERR_MUNOACTION);
	}
	if (mu_ctrly_occurred || mu_ctrlc_occurred)
	{
		mubclnup(NULL, need_to_free_space);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_BACKUPCTRL);
		mupip_exit(ERR_MUNOFINISH);
	}
	assert((NULL != grlist) || (NULL != mu_repl_inst_reg_list));
	if (NULL != mu_repl_inst_reg_list)
	{	/* Check that backup destination file for replication instance is different from the backup
		 * destination file for other regions that were specified.
		 */
		replinstfile = &(mu_repl_inst_reg_list->backup_file);
		replinstfile->addr[replinstfile->len] = '\0';
		if (CLI_PRESENT != cli_present("REPLACE"))
		{	/* make sure backup files do not already exist */
			if (FILE_PRESENT == (fstat_res = gtm_file_stat(replinstfile, NULL, NULL, FALSE, &ustatus)))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_FILEEXISTS, 2,
						LEN_AND_STR((char *)replinstfile->addr));
				error_mupip = TRUE;
			} else if (FILE_STAT_ERROR == fstat_res)
			{	/* stat doesn't usually return with an error. Assert so we can analyze */
				assert(FALSE);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("stat"), CALLFROM, ustatus);
				error_mupip = TRUE;
			}
		}
		for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
		{
			file = &(rptr->backup_file);
			file->addr[file->len] = '\0';
			if (!STRCMP(replinstfile->addr, file->addr))
			{
				util_out_print("Cannot backup replication instance file and database region !AD "
					"to the same destination file !AZ", TRUE, REG_LEN_STR(rptr->reg),
					replinstfile->addr);
				error_mupip = TRUE;
			}
		}
	}
	for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
	{
		file = &(rptr->backup_file);
		file->addr[file->len] = '\0';
		if ((backup_to_file == rptr->backup_to))
		{	/* make sure that backup won't be overwriting the database file itself */
			if (TRUE == is_file_identical(file->addr, (char *)rptr->reg->dyn.addr->fname))
			{
				gtm_putmsg_csa(CSA_ARG(REG2CSA(rptr->reg)) VARLSTCNT(4) ERR_MUSELFBKUP, 2, DB_LEN_STR(rptr->reg));
				error_mupip = TRUE;
			}
			if (CLI_PRESENT != cli_present("REPLACE"))
			{	/* make sure backup files do not already exist */
				if (FILE_PRESENT == (fstat_res = gtm_file_stat(file, NULL, NULL, FALSE, &ustatus)))
				{
					gtm_putmsg_csa(CSA_ARG(REG2CSA(rptr->reg)) VARLSTCNT(4) ERR_FILEEXISTS, 2,
							LEN_AND_STR((char *)file->addr));
					error_mupip = TRUE;
				} else if (FILE_STAT_ERROR == fstat_res)
				{	/* stat doesn't usually return with an error. Assert so we can analyze */
					assert(FALSE);
					gtm_putmsg_csa(CSA_ARG(REG2CSA(rptr->reg)) VARLSTCNT(8) ERR_SYSCALL, 5,
							LEN_AND_LIT("stat"), CALLFROM, ustatus);
					error_mupip = TRUE;
				}
			}
			for (rrptr = (backup_reg_list *)(grlist);  rrptr != rptr;  rrptr = rrptr->fPtr)
			{
				rfile = &(rrptr->backup_file);
				assert('\0' == rfile->addr[rfile->len]);
				if (!STRCMP(file->addr, rfile->addr))
				{
					util_out_print("Cannot backup database regions !AD and !AD to the same "
						"destination file !AZ",
						TRUE, REG_LEN_STR(rrptr->reg), REG_LEN_STR(rptr->reg), file->addr);
					error_mupip = TRUE;
				}
			}
		} else if (!incremental)
		{ 	/* non-incremental backups to "exec" and "tcp" are not supported*/
			gtm_putmsg_csa(CSA_ARG(REG2CSA(rptr->reg)) VARLSTCNT(1) ERR_NOTRNDMACC);
			error_mupip = TRUE;
		}
	}
	if (TRUE == error_mupip)
	{
		mubclnup(NULL, need_to_free_space);
		util_out_print("!/MUPIP cannot start backup with above errors!/", TRUE);
		mupip_exit(ERR_MUNOACTION);
	}
	/* =========================== STEP 3. Verify the regions and grab_crit()/freeze them ============ */
	mubmaxblk = 0;
	halt_ptr = grlist;
	size = ROUND_UP(SIZEOF_FILE_HDR_MAX, DISK_BLOCK_SIZE);
	ESTABLISH(mu_freeze_ch);
	tempfilename = tempdir_full.addr = tempdir_full_buffer;
	orig_buff_size = SIZEOF(tempdir_full_buffer);
	tempdir_full.len = orig_buff_size;
	if (TRUE == online)
	{
		tempdir_log.addr = GTM_BAK_TEMPDIR_LOG_NAME;
		tempdir_log.len = STR_LIT_LEN(GTM_BAK_TEMPDIR_LOG_NAME);
		trans_log_name_status =
			TRANS_LOG_NAME(&tempdir_log, &tempdir_trans, tempdir_trans_buffer, SIZEOF(tempdir_trans_buffer),
					do_sendmsg_on_log2long);
		if ((SS_NORMAL != trans_log_name_status)
		    || (NULL == tempdir_trans.addr) || (0 == tempdir_trans.len))
		{	/* GTM_BAK_TEMPDIR_LOG_NAME not found, attempt GTM_BAK_TEMPDIR_LOG_NAME_UC instead */
			tempdir_log.addr = GTM_BAK_TEMPDIR_LOG_NAME_UC;
			tempdir_log.len = STR_LIT_LEN(GTM_BAK_TEMPDIR_LOG_NAME_UC);
			trans_log_name_status =
				TRANS_LOG_NAME(&tempdir_log, &tempdir_trans, tempdir_trans_buffer, SIZEOF(tempdir_trans_buffer),
					       do_sendmsg_on_log2long);
		}
		/* save the length of the "base" so we can (restore it and) re-use the string in tempdir_trans.addr */
		tempdir_trans_len = tempdir_trans.len;
	} else
		tempdir_trans_len = 0;
	jnlpool_init_needed = TRUE;
	for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
	{	/* restore the original length since we are looping thru regions */
		tempdir_trans.len = tempdir_trans_len;
		file = &(rptr->backup_file);
		file->addr[file->len] = '\0';
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
			break;
		if (!IS_REG_BG_OR_MM(rptr->reg))
		{
			util_out_print("Region !AD is not a BG or MM databases", TRUE, REG_LEN_STR(rptr->reg));
			rptr->not_this_time = give_up_before_create_tempfile;
			continue;
		}
		if (reg_cmcheck(rptr->reg))
		{
			util_out_print("!/Can't BACKUP region !AD across network", TRUE, REG_LEN_STR(rptr->reg));
			rptr->not_this_time = give_up_before_create_tempfile;
			continue;
		}
		gv_cur_region = rptr->reg;
		gvcst_init(gv_cur_region, NULL);
		if (gv_cur_region->was_open)
		{
			gv_cur_region->open = FALSE;
			rptr->not_this_time = give_up_before_create_tempfile;
			continue;
		}
		TP_CHANGE_REG(gv_cur_region);
		if (gv_cur_region->read_only)
		{
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
			rptr->not_this_time = give_up_before_create_tempfile;
			continue;
		}
		/* Used to have MAX_RMS_RECORDSIZE here (instead of 32 * 1024) but this def does not exist, though we
		 * are making the same restriction due to lack of testing more than anything else so the hard-coded
		 * value will do for now. SE 5/2005
		 */
		if (incremental && ((32 * 1024) - SIZEOF(shmpool_blk_hdr)) < cs_data->blk_size)
		{
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) MAKE_MSG_TYPE(ERR_MUNOSTRMBKUP, ERROR), 3,
					DB_LEN_STR(gv_cur_region), 32 * 1024 - DISK_BLOCK_SIZE);
			rptr->not_this_time = give_up_before_create_tempfile;
			continue;
		}
		rptr->backup_hdr = (sgmnt_data_ptr_t)malloc(size);
		if (TRUE == online)
		{	/* determine the directory name and prefix for the temp file */
			memset(tempnam_prefix, 0, MAX_FN_LEN);
			memcpy(tempnam_prefix, gv_cur_region->rname, gv_cur_region->rname_len);
			SPRINTF(&tempnam_prefix[gv_cur_region->rname_len], "_%x", process_id);
			if ((SS_NORMAL == trans_log_name_status)
					&& (NULL != tempdir_trans.addr) && (0 != tempdir_trans.len))
				*(tempdir_trans.addr + tempdir_trans.len) = 0;
			else if (incremental && (backup_to_file != rptr->backup_to))
			{
				tempdir_trans.addr = tempdir_trans_buffer;
				tempdir_trans.len = SIZEOF(SCRATCH_DIR) - 1;
				memcpy(tempdir_trans_buffer, SCRATCH_DIR, tempdir_trans.len);
				tempdir_trans_buffer[tempdir_trans.len]='\0';
			} else
			{
				ptr = rptr->backup_file.addr + rptr->backup_file.len - 1;
				tempdir_trans.addr = tempdir_trans_buffer;
				while ((PATH_DELIM != *ptr) && (ptr > rptr->backup_file.addr))
					ptr--;
				if (ptr > rptr->backup_file.addr)
				{
					tempdir_trans.len = INTCAST(ptr - rptr->backup_file.addr + 1);
					if (SIZEOF(tempdir_trans_buffer) >= tempdir_trans.len)
					{
						memcpy(tempdir_trans_buffer, rptr->backup_file.addr, tempdir_trans.len);
						tempdir_trans_buffer[tempdir_trans.len] = '\0';
					}
					else
					{
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_FILEPARSE, 2,
						  rptr->backup_file.len, rptr->backup_file.addr);
                                		mubclnup(rptr, need_to_del_tempfile);
                                		mupip_exit(ERR_FILENAMETOOLONG);
					}
				} else
				{
					tempdir_trans_buffer[0] = '.';
					tempdir_trans_buffer[1] = '\0';
					tempdir_trans.len = 1;
				}
			}
			 /* Reset length , since gtm_file_stat expects the full buffer length as input */
                        tempdir_full.len = orig_buff_size;
			/* verify the accessibility of the tempdir */
			if (FILE_STAT_ERROR == (fstat_res = gtm_file_stat(&tempdir_trans, NULL, &tempdir_full, FALSE, &ustatus)))
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_FILEPARSE, 2, tempdir_trans.len,
						tempdir_trans.addr, ustatus);
				mubclnup(rptr, need_to_del_tempfile);
				mupip_exit(ustatus);
			}
			/*creating temp filename here , check if we have the required space*/
			if (orig_buff_size <= (tempdir_full.len + strlen(tempnam_prefix) + SIZEOF(uint4)))
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_FILEPARSE, 2, tempdir_trans.len,
						tempdir_trans.addr);
				mubclnup(rptr, need_to_del_tempfile);
				mupip_exit(ERR_FILENAMETOOLONG);
			}
			SPRINTF(tempfilename + tempdir_full.len,"/%s_XXXXXX",tempnam_prefix);
			MKSTEMP(tempfilename, rptr->backup_fd);
			if (FD_INVALID == rptr->backup_fd)
			{
				status = errno;
				if ((NULL != tempdir_full.addr) &&
					(0 != ACCESS(tempdir_full.addr, TMPDIR_ACCESS_MODE)))
				{
					status = errno;
					util_out_print("!/Do not have full access to directory for temporary files: !AD", TRUE,
						tempdir_trans.len, tempdir_trans.addr);
				} else
					util_out_print("!/Cannot create the temporary file in directory !AD for online backup",
						TRUE, tempdir_trans.len, tempdir_trans.addr);
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) status);
				SET_ERROR_CONDITION(status);	/* sets "error_condition" & "severity" */
				util_out_print("!/MUPIP cannot start backup with above errors!/", TRUE);
				mubclnup(rptr, need_to_del_tempfile);
				mupip_exit(status);
			}
#			ifdef __MVS__
			if (-1 == gtm_zos_set_tag(rptr->backup_fd, TAG_BINARY, TAG_NOTTEXT, TAG_FORCE, &realfiletag))
				TAG_POLICY_GTM_PUTMSG(tempfilename, realfiletag, TAG_BINARY, errno);
#			endif
			/* Temporary file for backup was created above using "mkstemp" which on AIX opens the file without
			 * large file support enabled. Work around that by closing the file descriptor returned and reopening
			 * the file with the "open" system call (which gets correctly translated to "open64"). We need to do
			 * this because the temporary file can get > 2GB. Since it is not clear if mkstemp on other Unix platforms
			 * will open the file for large file support, we use this solution for other Unix flavors as well.
			 */
			tmpfd = rptr->backup_fd;
			OPENFILE(tempfilename, O_RDWR, rptr->backup_fd);
			if (FD_INVALID == rptr->backup_fd)
			{
				status = errno;
				util_out_print("!/Error re-opening temporary file created by mkstemp()!/", TRUE);
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) status);
				SET_ERROR_CONDITION(status);	/* sets "error_condition" & "severity" */
				util_out_print("!/MUPIP cannot start backup with above errors!/", TRUE);
				mubclnup(rptr, need_to_del_tempfile);
				mupip_exit(status);
			}
			/* Now that the temporary file has been opened successfully, close the fd returned by mkstemp */
			F_CLOSE(tmpfd, fclose_res);	/* resets "tmpfd" to FD_INVALID */
			tempdir_full.len = STRLEN(tempdir_full.addr); /* update the length */
#			ifdef __MVS__
			if (-1 == gtm_zos_tag_to_policy(rptr->backup_fd, TAG_BINARY, &realfiletag))
				TAG_POLICY_GTM_PUTMSG(tempfilename, realfiletag, TAG_BINARY, errno);
#			endif
			if (debug_mupip)
				util_out_print("!/MUPIP INFO:   Temp file name: !AD", TRUE,tempdir_full.len, tempdir_full.addr);
			memcpy(&rptr->backup_tempfile[0], tempdir_full.addr, tempdir_full.len);
			rptr->backup_tempfile[tempdir_full.len] = 0;
			/* give temporary files the group and permissions as other shared resources - like journal files */
			FSTAT_FILE(((unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fd, &stat_buf, fstat_res);
			if (-1 != fstat_res)
				if (!gtm_permissions(&stat_buf, &user_id, &group_id, &perm, PERM_FILE, &pdd))
				{
					send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6+PERMGENDIAG_ARG_COUNT)
						ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("backup file"),
						RTS_ERROR_STRING(
							((unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fn),
						PERMGENDIAG_ARGS(pdd));
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6+PERMGENDIAG_ARG_COUNT)
						ERR_PERMGENFAIL, 4, RTS_ERROR_STRING("backup file"),
						RTS_ERROR_STRING(
							((unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fn),
						PERMGENDIAG_ARGS(pdd));
					mubclnup(rptr, need_to_del_tempfile);
					mupip_exit(EPERM);
				}
			/* Setup new group and permissions if indicated by the security rules.  Use
			 * 0770 anded with current mode for the new mode if masked permission selected.
			 */
			if ((-1 == fstat_res) || (-1 == FCHMOD(rptr->backup_fd, perm))
				|| (((INVALID_UID != user_id) || (INVALID_GID != group_id))
					&& (-1 == fchown(rptr->backup_fd, user_id, group_id))))
			{
				status = errno;
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) status);
				SET_ERROR_CONDITION(status);	/* sets "error_condition" & "severity" */
				util_out_print("!/MUPIP cannot start backup with above errors!/", TRUE);
				mubclnup(rptr, need_to_del_tempfile);
				mupip_exit(status);
			}
		} else
		{
			while (REG_ALREADY_FROZEN == region_freeze(gv_cur_region, TRUE, FALSE, FALSE, FALSE, FALSE))
			{
				hiber_start(1000);
				if ((TRUE == mu_ctrly_occurred) || (TRUE == mu_ctrlc_occurred))
				{
					mubclnup(rptr, need_to_del_tempfile);
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_FREEZECTRL);
					mupip_exit(ERR_MUNOFINISH);
				}
			}
		}
		if (cs_addrs->hdr->blk_size > mubmaxblk)
			mubmaxblk = cs_addrs->hdr->blk_size + 4;
		halt_ptr = (tp_region *)(rptr->fPtr);
	}
	if ((TRUE == mu_ctrly_occurred) || (TRUE == mu_ctrlc_occurred))
	{
		mubclnup(rptr, need_to_del_tempfile);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_BACKUPCTRL);
		mupip_exit(ERR_MUNOFINISH);
	}
	udi = NULL;
	if (NULL != mu_repl_inst_reg_list)
	{	/* Replication instance file needs to be backed up as well. But, before doing so, we need to get the ftok and
		 * the access control semaphore on the instance file and the journal pool. By doing so, we shut off other concurrent
		 * writers to the instance file.
		 * Note: It is important that this is done BEFORE grabbing crit on any of the regions as otherwise we might be
		 * creating a possible chance of deadlock due to out-of-order access. To illustrate, lets say we went ahead and got
		 * crit on all regions and are about to get the access control semaphore. During this time, online rollback comes
		 * along and acquires the access control semaphore on the journal pool and proceeds to get the crit on all regions.
		 * Now, backup holds crit and needs access control semaphore whereas rollback holds the access control semaphore
		 * and needs crit. Classic deadlock.
		 */
		/* Attach to jnlpool if backup of replication instance file is needed and "jnlpool_init" did not happen as part of
		 * "gvcst_init" (e.g. if CUSTOM_ERRORS_AVAILABLE is FALSE).
		 */
		repl_inst_available = REPL_INST_AVAILABLE(NULL);
		if (!pool_init && repl_inst_available)
			jnlpool_init(GTMRELAXED, (boolean_t)FALSE, (boolean_t *)NULL, NULL);
		if (!pool_init)
		{	/* pool_init is only set if the source server has setup shared memory */
			assert(NULL != jnlpool);
			if (!repl_inst_available)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REPLINSTUNDEF);
			if (!jnlpool)
			{	/* jnlpool_init should have provided but be safe */
				jnlpool = malloc(SIZEOF(jnlpool_addrs));
				memset((uchar_ptr_t)jnlpool, 0, SIZEOF(jnlpool_addrs));
				assert(NULL == jnlpool_head);
			}
			if (NULL == jnlpool->jnlpool_dummy_reg)
			{
				r_save = gv_cur_region;
				mu_gv_cur_reg_init();
				jnlpool->jnlpool_dummy_reg = reg = gv_cur_region;
				gv_cur_region = r_save;
				ASSERT_IN_RANGE(MIN_RN_LEN, SIZEOF(JNLPOOL_DUMMY_REG_NAME) - 1, MAX_RN_LEN);
				MEMCPY_LIT(reg->rname, JNLPOOL_DUMMY_REG_NAME);
				reg->rname_len = STR_LIT_LEN(JNLPOOL_DUMMY_REG_NAME);
				reg->rname[reg->rname_len] = '\0';
				udi = FILE_INFO(reg);
				seg = reg->dyn.addr;
				memcpy((char *)seg->fname, replpool_id.instfilename, full_len);
				udi->fn = (char *)seg->fname;
				seg->fname_len = full_len;
				seg->fname[full_len] = '\0';
				udi->ftok_semid = INVALID_SEMID;
			} else
			{	/* Possible if jnlpool_init did mu_gv_cur_reg_init but returned prematurely due to NOJNLPOOL */
				assert(!jnlpool->jnlpool_dummy_reg->open);
				reg = jnlpool->jnlpool_dummy_reg;
				/* Since mu_gv_cur_reg_init is already done, ensure that the reg->rname is correct */
				assert(0 == MEMCMP_LIT(reg->rname, JNLPOOL_DUMMY_REG_NAME));
				assert(reg->rname_len == STR_LIT_LEN(JNLPOOL_DUMMY_REG_NAME));
				assert('\0' == reg->rname[reg->rname_len]);
				udi = FILE_INFO(reg);
			}
			DEBUG_ONLY(jnlpool_save = jnlpool);
			do
			{
				if (!ftok_sem_get(jnlpool->jnlpool_dummy_reg, TRUE, REPLPOOL_ID, FALSE, &dummy_ftok_counter_halted))
				{
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JNLPOOLSETUP);
					error_mupip = TRUE;
					goto repl_inst_bkup_done1;
				}
				save_errno = errno;
				repl_inst_read(udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
				/* If we notice no jnlpool semid open then it means the journal pool has not been established
				 * by the source server. In that case, there is no point trying to attach to a non-existent
				 * jnlpool. Break out and proceed with the backup while still holding the ftok lock.
				 */
				sem_id = repl_instance.jnlpool_semid;
				if (INVALID_SEMID == sem_id)
					break;
				/* A source server sneaked in between when we did the "jnlpool_init" above and set up a
				 * journal pool. Attach to it after releasing the ftok lock. We expect this situation to be
				 * rare that we keep retrying.
				 */
				ftok_sem_release(jnlpool->jnlpool_dummy_reg, udi->counter_ftok_incremented, TRUE);
				jnlpool_init(GTMRELAXED, (boolean_t)FALSE, (boolean_t *)NULL, NULL);
					/* "jnlpool_init" will set "pool_init" if successful */
				if (pool_init)
					break;
				assert(jnlpool_save == jnlpool);	/* should be reusing incomplete structure */
			} while (TRUE);
		}
		if (pool_init)
		{
			udi = FILE_INFO(jnlpool->jnlpool_dummy_reg);
			assert(INVALID_SEMID != udi->ftok_semid);
			if (!ftok_sem_lock(jnlpool->jnlpool_dummy_reg, FALSE))
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JNLPOOLSETUP);
				error_mupip = TRUE;
				goto repl_inst_bkup_done1;
			}
			/* (Re-)read the instance file header now that we have the ftok lock */
			repl_inst_read(udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
			if (jnlpool->jnlpool_ctl->ftok_counter_halted)
				udi->counter_ftok_incremented = FALSE;	/* so we do not inadvertently delete the ftok semaphore */
		}
		assert(NULL != jnlpool->jnlpool_dummy_reg);
		assert(!pool_init || ((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl)));
		shm_id = repl_instance.jnlpool_shmid;
		sem_id = repl_instance.jnlpool_semid;
		udi = FILE_INFO(jnlpool->jnlpool_dummy_reg);
		if (INVALID_SEMID != sem_id)
		{
			assert(pool_init);
			assert(repl_instance.crash);
			semarg.buf = &semstat;
			if (-1 == semctl(sem_id, DB_CONTROL_SEM, IPC_STAT, semarg))
			{
				save_errno = errno;
				SNPRINTF(scndry_msg, OUT_BUFF_SIZE, "Error with semctl on Journal Pool SEMID (%d)", sem_id);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_REPLREQROLLBACK, 2, full_len, udi->fn,
						ERR_TEXT, 2, LEN_AND_STR(scndry_msg), save_errno);
				error_mupip = TRUE;
				goto repl_inst_bkup_done1;
			} else if (semarg.buf->sem_ctime != repl_instance.jnlpool_semid_ctime)
			{
				SNPRINTF(scndry_msg, OUT_BUFF_SIZE, "Creation time for Journal Pool SEMID (%d) is %d; Expected %d",
						sem_id, semarg.buf->sem_ctime, repl_instance.jnlpool_semid_ctime);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_REPLREQROLLBACK, 2, full_len, udi->fn,
						ERR_TEXT, 2, LEN_AND_STR(scndry_msg));
				error_mupip = TRUE;
				goto repl_inst_bkup_done1;
			}
			set_sem_set_src(sem_id); /* repl_sem.c has some functions which needs semid in static variable */
			status = grab_sem(SOURCE, JNL_POOL_ACCESS_SEM);
			if (SS_NORMAL != status)
			{
				save_errno = errno;
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
						RTS_ERROR_LITERAL("Error with journal pool access semaphore"),
						save_errno);
				error_mupip = TRUE;
				goto repl_inst_bkup_done1;
			}
			udi->grabbed_access_sem = TRUE;
			udi->counter_acc_incremented = TRUE;
		}
		/* At this point, we either hold the access control lock on the journal pool OR the journal pool
		 * semaphore doesn't exist. In either case, we can proceed with the "cp" of the instance file
		 */
		assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM] || (INVALID_SEMID == sem_id));
		if (repl_instance.file_corrupt)
		{
			SNPRINTF(scndry_msg, OUT_BUFF_SIZE, "Instance file header has file_corrupt field set to TRUE");
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_REPLREQROLLBACK, 2, full_len, udi->fn,
					ERR_TEXT, 2, LEN_AND_STR(scndry_msg));
			error_mupip = TRUE;
		}
	}
repl_inst_bkup_done1:
	kip_count = 0;
	SET_GBL_JREC_TIME;	/* routines that write jnl records (e.g. wcs_flu) require this to be initialized */
	DEBUG_ONLY(reg_count = 0);
	for (rptr = (backup_reg_list *)(grlist); NULL != rptr; rptr = rptr->fPtr)
	{
		if (rptr->not_this_time <= keep_going)
		{
			TP_CHANGE_REG(rptr->reg);
			assert(!cs_addrs->hold_onto_crit); /* DSE/Rollback can never reach here */
			if (online)
			{
				grab_crit(gv_cur_region);
				DEBUG_ONLY(reg_count++);
				INCR_INHIBIT_KILLS(cs_addrs->nl);
				if (cs_data->kill_in_prog)
					kip_count++;
			} else if (JNL_ENABLED(cs_data))
			{	/* For the non-online case, we want to get crit on ALL regions to ensure we write the
				 * same timestamp in journal records across ALL regions (in wcs_flu below). We will
				 * release crit as soon as the wcs_flu across all regions is done.
				 */
				grab_crit(gv_cur_region);
				DEBUG_ONLY(reg_count++);
			}
			if (FROZEN_CHILLED(cs_addrs))
			{	/* While a FREEZE -ONLINE was in place, all processes exited, leaving the
				 * shared memory up. Either autorelease, if enabled, or error out.
				 */
				DO_CHILLED_AUTORELEASE(cs_addrs, cs_data);
				if (FROZEN_CHILLED(cs_addrs))
				{
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_OFRZACTIVE, 2, DB_LEN_STR(gv_cur_region));
					rptr->not_this_time = give_up_before_create_tempfile;
					continue;
				}
			}
			/* We will be releasing crit if KIP is set, so don't update jgbl.gbl_jrec_time now */
			if (!kip_count)
			{
				UPDATE_GBL_JREC_TIME;
			}
		}
	}
	/* If we have KILLs in progress on any of the regions, wait a maximum of 1 minute(s) for those to finish. */
	for (crit_counter = 1; online && kip_count; )
	{	/* The purpose of this loop is to wait for kip to clear on all regions, waiting for a maximum of about a minute.
		 * This is complicated by the fact that in spite of setting INCR_INHIBIT_KILLS above, a process in crit for a
		 * "fourth" retry may ignote that, and set the kip flag while we are waiting
		 */
		DEBUG_ONLY(nocritrptr = NULL;)
		for (rptr = (backup_reg_list *)(grlist); NULL != rptr; rptr = rptr->fPtr)
		{	/* Release all the crits before going into the wait loop */
			if (rptr->not_this_time > keep_going)
				continue;
			TP_CHANGE_REG(rptr->reg);
			if (cs_addrs->now_crit)
			{	/* once we encountered a region in the list that we did not hold crit on, we should also
				 * not hold crit on all later regions in the list. assert that.
				 */
				assert(NULL == nocritrptr);
				rel_crit(gv_cur_region);
			}
			DEBUG_ONLY(
			else
				nocritrptr = rptr;
			)
		}
		/* Wait for a maximum of 1 minute on all the regions for KIP to reset */
		for (rptr = (backup_reg_list *)(grlist); NULL != rptr; rptr = rptr->fPtr)
		{	/* multiple regions might have kip, but waiting on any waits on all, so just do this for 1 fixed time */
			if (rptr->not_this_time > keep_going)
				continue;
			TP_CHANGE_REG(rptr->reg);
			if (debug_mupip)
			{
				GET_CUR_TIME(time_str);
				util_out_print("!/MUPIP INFO: !AD : Start kill-in-prog wait for database !AD", TRUE,
					CTIME_BEFORE_NL, time_str, DB_LEN_STR(gv_cur_region));
			}
			kip_pids_arr_ptr = cs_addrs->nl->kip_pid_array;
			while (cs_data->kill_in_prog && (MAX_CRIT_TRY > crit_counter++))
			{
				GET_C_STACK_FOR_KIP(kip_pids_arr_ptr, crit_counter, MAX_CRIT_TRY, 1, MAX_KIP_PID_SLOTS);
				wcs_sleep(crit_counter);
			}
			if (debug_mupip)
			{
				GET_CUR_TIME(time_str);
				util_out_print("!/MUPIP INFO: !AD : Done with kill-in-prog wait on !AD", TRUE,
					CTIME_BEFORE_NL, time_str, DB_LEN_STR(gv_cur_region));
			}
			if (MAX_CRIT_TRY < crit_counter)
				break;
		}
		/* Since we have waited a while for KIP to get reset, get current time again to make it more accurate */
		SET_GBL_JREC_TIME;
		for (kip_count = 0, rptr = (backup_reg_list *)(grlist); NULL != rptr; rptr = rptr->fPtr)
		{
			if (rptr->not_this_time > keep_going)
				continue;
			TP_CHANGE_REG(rptr->reg);
			grab_crit(gv_cur_region);
			/* Now that we have crit, check if this region is actively journaled and if gbl_jrec_time needs to be
			 * adjusted (to ensure time ordering of journal records within this region's journal file).
			 * This needs to be done BEFORE writing any journal records for this region. The value of
			 * jgbl.gbl_jrec_time at the end of this loop will be used to write journal records for ALL
			 * regions so all regions will have same eov/bov timestamps.
			 */
			UPDATE_GBL_JREC_TIME;
			if (cs_data->kill_in_prog)
			{
				if (MAX_CRIT_TRY < crit_counter)
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_BACKUPKILLIP,
						       2, DB_LEN_STR(gv_cur_region));
				else	/* lets wait some more */
				{
					kip_count++;
					break;
				}
			}
		}
	}
	assert(reg_count == have_crit(CRIT_HAVE_ANY_REG | CRIT_ALL_REGIONS));
	DEBUG_ONLY(save_gbl_jrec_time = jgbl.gbl_jrec_time;)
	/* ========================== STEP 4. Flush cache, calculate tn for incremental, and do backup =========== */
	if ((FALSE == mu_ctrly_occurred) && (FALSE == mu_ctrlc_occurred))
	{
		mup_bak_pause(); /* ? save some crit time? */
		backup_started = TRUE;
		DEBUG_ONLY(jnl_seqno = 0;)
		if (NULL != mu_repl_inst_reg_list)
		{
			if (error_mupip)
			{	/* Some error happened while getting the ftok or the access control. Release locks, ignore instance
				 * file and continue with the backup of database files.
				 */
				goto repl_inst_bkup_done2;
			}
			assert(udi == FILE_INFO(jnlpool->jnlpool_dummy_reg));
			seg = jnlpool->jnlpool_dummy_reg->dyn.addr; /* re-initialize (for pool_init == TRUE case) */
			cmdptr = &command[0];
			assert(SIZEOF(command) >= (5 + seg->fname_len + mu_repl_inst_reg_list->backup_file.len));
			memcpy(cmdptr, "cp ", 3);
			cmdptr += 3;
			memcpy(cmdptr, seg->fname, seg->fname_len);
			cmdptr[seg->fname_len] = ' ';
			cmdptr += seg->fname_len + 1;
			memcpy(cmdptr, mu_repl_inst_reg_list->backup_file.addr, mu_repl_inst_reg_list->backup_file.len);
			cmdptr += mu_repl_inst_reg_list->backup_file.len;
			*cmdptr = '\0';
			/* We read the instance file header from disk before getting crit on the databases. It is possible
			 * further updates happened to the live instance file (e.g. history record additions etc.) since
			 * then that the copy of the file header we read from disk does not yet have. Note down the copy we
			 * read from disk and after the system "cp" is done and we do a "grab_lock", check if the live
			 * file header is identical to the read file header and if so no further action is needed. Otherwise
			 * we need to note down the live file header as the new one and redo the system "cp" (outside of a
			 * "grab_lock" window as this is an external utility and database access would stall). This is because
			 * the file header we read should match the file contents. History records are the only ones which change
			 * the size of the instance file right now and so as long as there is no change in this field before
			 * and after the "cp", no need to redo the "cp".
			 */
			do
			{
				rv = SYSTEM(((char *)command));
				if (0 != rv)
				{
					if (-1 == rv)
					{
						save_errno = errno;
						errptr = (char *)STRERROR(save_errno);
						util_out_print("system : !AZ", TRUE, errptr);
					}
					util_out_print("Error doing !AD", TRUE, cmdptr - command, command);
					error_mupip = TRUE;
					goto repl_inst_bkup_done2;
				}
				assert(!pool_init || (INVALID_SHMID != shm_id) && (INVALID_SEMID != sem_id));
				assert(pool_init || (INVALID_SHMID == shm_id) && (INVALID_SEMID == sem_id));
				if (pool_init)
				{	/* The journal pool exists. Note down the journal seqno from there and copy that onto
					 * the backed up instance file header. Also, clean up other fields in the backed up
					 * instance file header.
					 */
					csa = &udi->s_addrs;
					assert(csa->critical
						== (mutex_struct_ptr_t)((sm_uc_ptr_t)jnlpool->jnlpool_ctl + JNLPOOL_CTL_SIZE));
					grab_lock(jnlpool->jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
					jnl_seqno = jnlpool->jnlpool_ctl->jnl_seqno;
					if (repl_instance.num_histinfo != jnlpool->repl_inst_filehdr->num_histinfo)
					{
						repl_instance = *jnlpool->repl_inst_filehdr;
						rel_lock(jnlpool->jnlpool_dummy_reg);
						continue;	/* redo the "cp" */
					}
					save_inst_hdr = jnlpool->repl_inst_filehdr;
					jnlpool->repl_inst_filehdr = &repl_instance;
					assert(0 != jnl_seqno);
					/* All the cleanup we want is exactly done by the "repl_inst_histinfo_truncate" function.
					 * But we don't want to clean the instance file. We want to instead clean the backed up
					 * instance file. To that effect, temporarily change "udi->fn" to reflect the backed up
					 * file so all the changes get flushed there. Restore it after the function call.
					 */
					udi->fn = (char *)mu_repl_inst_reg_list->backup_file.addr;
					/* Before invoking "repl_inst_histinfo_truncate", set "strm_seqno[]" appropriately in the
					 * instance file header. The truncate function call expects it to be set before invocation.
					 */
					COPY_JCTL_STRMSEQNO_TO_INSTHDR_IF_NEEDED;
					repl_inst_histinfo_truncate(jnl_seqno);	/* Flush updated file header to
										 * backed up instance file */
					rel_lock(jnlpool->jnlpool_dummy_reg);
					udi->fn = (char *)seg->fname; /* Restore */
					/* Now that instance file truncate is done, restore jnlpool->repl_inst_filehdr to its
					 * original value
					 */
					assert(NULL != save_inst_hdr);
					jnlpool->repl_inst_filehdr = save_inst_hdr;
				} else
				{	/* We are guaranteed that NO one is actively accessing the instance file. So, no need to
					 * truncate the backed up instance file.
					 */
					jnl_seqno = repl_instance.jnl_seqno;
				}
				break;
			} while (TRUE);
			assert(0 != jnl_seqno || !pool_init);
repl_inst_bkup_done2:
			if (udi)
			{
				if (udi->grabbed_access_sem)
				{
					assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM] && (INVALID_SEMID != sem_id));
					assertpro(SS_NORMAL == rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM));
				}
				if (udi->grabbed_ftok_sem)
					ftok_sem_release(jnlpool->jnlpool_dummy_reg, udi->counter_ftok_incremented, TRUE);
			}
			if (!error_mupip)
			{
				util_out_print("Replication Instance file !AD backed up in file !AD", TRUE,
					LEN_AND_STR(udi->fn),
					mu_repl_inst_reg_list->backup_file.len, mu_repl_inst_reg_list->backup_file.addr);
				util_out_print("Journal Seqnos up to 0x!16@XQ are backed up.", TRUE, &jnl_seqno);
				util_out_print("", TRUE);
			} else
				util_out_print("Error backing up replication instance file !AD. Moving on to other backups.",
					TRUE, LEN_AND_STR(udi->fn));
		}
		jgbl.dont_reset_gbl_jrec_time = TRUE;
		for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
		{
			if (rptr->not_this_time > keep_going)
				continue;
			TP_CHANGE_REG(rptr->reg);
			wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_MSYNC_DB);
			if (incremental)
			{
				if (inc_since_inc)
				{
					rptr->tn = cs_data->last_inc_backup;
					rptr->last_blk_at_last_bkup = cs_data->last_inc_bkup_last_blk;
				} else if (inc_since_rec)
				{
					rptr->tn = cs_data->last_rec_backup;
					rptr->last_blk_at_last_bkup = cs_data->last_rec_bkup_last_blk;
				} else if (0 == tn)
				{
					rptr->tn = cs_data->last_com_backup;
					rptr->last_blk_at_last_bkup = cs_data->last_com_bkup_last_blk;
				} else
				{	/* /TRANS was specified as arg so use it and all bitmaps are
					   subject to being backed up (since this is not a contiguous backup
					   to the last one)
					*/
					rptr->tn = tn;
					rptr->last_blk_at_last_bkup = 0;
				}
			}
			assert((0 == jnl_seqno) || !REPL_ENABLED(cs_data) || (cs_data->reg_seqno <= jnl_seqno));
			memcpy(rptr->backup_hdr, cs_data, SIZEOF_FILE_HDR(cs_data));
			if (online) /* save a copy of the fileheader, modify current fileheader and release crit */
			{
				if (0 != cs_addrs->hdr->abandoned_kills)
				{
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_KILLABANDONED, 4, DB_LEN_STR(rptr->reg),
						LEN_AND_LIT("backup database could have incorrectly marked busy integrity errors"));
				}
				sbufh_p = cs_addrs->shmpool_buffer;
				memcpy(&sbufh_p->shadow_file_header, cs_data, SGMNT_HDR_LEN);
				if (BACKUP_NOT_IN_PROGRESS != cs_addrs->nl->nbb)
				{
					if (TRUE == is_proc_alive(sbufh_p->backup_pid, sbufh_p->backup_image_count))
					{
					    	/* someone else is doing the backup */
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_BKUPRUNNING, 3,
								sbufh_p->backup_pid, REG_LEN_STR(rptr->reg));
						rptr->not_this_time = give_up_after_create_tempfile;
						/* Decerement counter so that inhibited KILLs can now proceed */
						DECR_INHIBIT_KILLS(cs_addrs->nl);
						rel_crit(rptr->reg);
						continue;
					}
				}
				if (!newjnlfiles_specified)
					newjnlfiles = (JNL_ENABLED(cs_data)) ? TRUE : FALSE;
				/* switch the journal file, if journaled */
				if (newjnlfiles)
				{
					if (cs_data->jnl_file_len)
						cre_jnl_file_intrpt_rename(cs_addrs);
					if (JNL_ALLOWED(cs_data))
					{
						memset(&jnl_info, 0, SIZEOF(jnl_info));
						jnl_info.prev_jnl = &prev_jnl_fn[0];
						set_jnl_info(gv_cur_region, &jnl_info);
						save_no_prev_link = jnl_info.no_prev_link =  (jnl_options[jnl_noprevjnlfile] ||
							!keep_prev_link || !JNL_ENABLED(cs_data)) ? TRUE : FALSE;
						if (JNL_ENABLED(cs_data) && (0 != cs_addrs->nl->jnl_file.u.inode))
						{	/* Note: following will again call wcs_flu() */
							if (SS_NORMAL != (status = set_jnl_file_close()))
							{
								util_out_print("!/Journal file !AD not closed:",
									TRUE, jnl_info.jnl_len, jnl_info.jnl);
								gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) status);
								rptr->not_this_time = give_up_after_create_tempfile;
								DECR_INHIBIT_KILLS(cs_addrs->nl);
								rel_crit(rptr->reg);
								error_mupip = TRUE;
								continue;
							}
							jnl_info.no_rename = FALSE;
						} else
						{
							filestr.addr = (char *)jnl_info.jnl;
							filestr.len = jnl_info.jnl_len;
							if (FILE_STAT_ERROR == (jnl_fstat =
									gtm_file_stat(&filestr, NULL, NULL, FALSE, &ustatus)))
							{
								gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_JNLFNF, 2,
									filestr.len, filestr.addr, ustatus);
								rptr->not_this_time = give_up_after_create_tempfile;
								DECR_INHIBIT_KILLS(cs_addrs->nl);
								rel_crit(rptr->reg);
								error_mupip = TRUE;
								continue;
							}
							jnl_set_cur_prior(gv_cur_region, cs_addrs, cs_data);
							jnl_info.no_rename = (FILE_NOT_FOUND == jnl_fstat);
						}
						wcs_flu(WCSFLU_FSYNC_DB | WCSFLU_FLUSH_HDR | WCSFLU_MSYNC_DB);
						if (!JNL_ENABLED(cs_data) && (NULL != cs_addrs->nl))
						{ /* Cleanup the jnl file info in shared memory before switching
						     journal file. This case occurs if mupip backup -newjnl is
						     run after jnl_file_lost() closes journaling on a region */
							NULLIFY_JNL_FILE_ID(cs_addrs);
						}
						if (replication_on)
						{
							if (REPL_WAS_ENABLED(cs_data))
							{
								jnl_info.jnl_state = jnl_open;
								jnl_info.repl_state = repl_open;
								jnl_info.no_prev_link = TRUE;
								gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_REPLSTATE, 6,
									LEN_AND_LIT(FILE_STR), DB_LEN_STR(gv_cur_region),
									LEN_AND_STR(repl_state_lit[repl_open]));
								gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_JNLSTATE, 6,
									LEN_AND_LIT(FILE_STR), DB_LEN_STR(gv_cur_region),
									LEN_AND_STR(jnl_state_lit[jnl_open]));
							} else if (!REPL_ALLOWED(cs_data))
							{
								gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_REPLSTATEERR, 2,
									DB_LEN_STR(gv_cur_region), ERR_TEXT, 2,
									LEN_AND_LIT("Standalone access required"));
								rptr->not_this_time = give_up_after_create_tempfile;
								DECR_INHIBIT_KILLS(cs_addrs->nl);
								rel_crit(rptr->reg);
								error_mupip = TRUE;
								continue;
							}
						} else if (REPL_WAS_ENABLED(cs_data))
						{ /* Do not switch journal file when replication was turned
						     OFF by jnl_file_lost() */
							assert(cs_data->jnl_state == jnl_closed);
							gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(10) ERR_REPLJNLCNFLCT, 8,
									LEN_AND_STR(jnl_state_lit[jnl_open]),
									DB_LEN_STR(gv_cur_region),
									LEN_AND_STR(repl_state_lit[repl_closed]),
									LEN_AND_STR(jnl_state_lit[jnl_open]));
							rptr->not_this_time = give_up_after_create_tempfile;
							DECR_INHIBIT_KILLS(cs_addrs->nl);
							rel_crit(rptr->reg);
							error_mupip = TRUE;
							continue;
						} else
						{ /* While switching journal file, perhaps we never intended
						   * to turn journaling ON from OFF (either with replication
						   * or with M journaling). However, to keep the existing
						   * behavior, we will turn journaling ON */
							jnl_info.jnl_state = jnl_open;
						}
						if (EXIT_NRM == cre_jnl_file(&jnl_info))
						{
							if (jnl_info.no_prev_link && (save_no_prev_link != jnl_info.no_prev_link))
								gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_PREVJNLLINKCUT, 4,
									JNL_LEN_STR(cs_data), DB_LEN_STR(rptr->reg));
							memcpy(cs_data->jnl_file_name, jnl_info.jnl, jnl_info.jnl_len);
							cs_data->jnl_file_name[jnl_info.jnl_len] = '\0';
							cs_data->jnl_file_len = jnl_info.jnl_len;
							cs_data->jnl_buffer_size = jnl_info.buffer;
							cs_data->jnl_alq = jnl_info.alloc;
							cs_data->jnl_deq = jnl_info.extend;
							cs_data->jnl_before_image = jnl_info.before_images;
							cs_data->jnl_state = jnl_info.jnl_state;
							cs_data->repl_state = jnl_info.repl_state;
							if (newjnlfiles_specified && sync_io_specified)
								cs_data->jnl_sync_io = sync_io;
							cs_data->jnl_checksum = jnl_info.checksum;
							cs_data->jnl_eovtn = cs_data->trans_hist.curr_tn;
							gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(10) ERR_JNLCREATE, 8,
								jnl_info.jnl_len, jnl_info.jnl, LEN_AND_LIT("region"),
								REG_LEN_STR(gv_cur_region),
								LEN_AND_STR(before_image_lit[(jnl_info.before_images ? 1 : 0)]));
							if (JNL_ENABLED(cs_data) &&
								(jnl_options[jnl_noprevjnlfile] || !keep_prev_link))
								gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_PREVJNLLINKCUT,
									4, JNL_LEN_STR(cs_data), DB_LEN_STR(gv_cur_region));
							fc = gv_cur_region->dyn.addr->file_cntl;
							fc->op = FC_WRITE;
							/* Note: cs_data points to shared memory and is already aligned
							 * appropriately even if db was opened using O_DIRECT.
							 */
							fc->op_buff = (sm_uc_ptr_t)cs_data;
							fc->op_len = SGMNT_HDR_LEN;
							fc->op_pos = 1;
							status = dbfilop(fc);
							if (SS_NORMAL != status)
							{
								gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(7)
										ERR_DBFILERR, 2,
										DB_LEN_STR(gv_cur_region), 0, status, 0);
								rptr->not_this_time = give_up_after_create_tempfile;
								DECR_INHIBIT_KILLS(cs_addrs->nl);
								rel_crit(rptr->reg);
								error_mupip = TRUE;
								continue;
							}
						} else
							gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLNOCREATE, 2,
									jnl_info.jnl_len, jnl_info.jnl);
					} else
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) MAKE_MSG_WARNING(ERR_JNLDISABLE),
										2, DB_LEN_STR(gv_cur_region));
				} else if (replication_on && !REPL_ENABLED(cs_data))
				{
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8)
						ERR_REPLSTATEERR, 2, DB_LEN_STR(gv_cur_region),
						ERR_TEXT, 2,
						LEN_AND_LIT("Cannot turn replication ON without also switching journal file"));
					rptr->not_this_time = give_up_after_create_tempfile;
					DECR_INHIBIT_KILLS(cs_addrs->nl);
					rel_crit(rptr->reg);
					error_mupip = TRUE;
					continue;
				}
				if (FALSE == shmpool_lock_hdr(gv_cur_region))
				{
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(9) ERR_DBCCERR, 2, REG_LEN_STR(gv_cur_region),
						   ERR_ERRCALL, 3, CALLFROM);
					rptr->not_this_time = give_up_after_create_tempfile;
					DECR_INHIBIT_KILLS(cs_addrs->nl);
					rel_crit(rptr->reg);
					continue;
				}
				memcpy(&(sbufh_p->tempfilename[0]), &(rptr->backup_tempfile[0]),
						strlen(rptr->backup_tempfile));
				sbufh_p->tempfilename[strlen(rptr->backup_tempfile)] = 0;
				sbufh_p->backup_tn = cs_addrs->ti->curr_tn;
				sbufh_p->backup_pid = process_id;
				sbufh_p->inc_backup_tn = (incremental ? rptr->tn : 0);
				sbufh_p->dskaddr = 0;
				sbufh_p->backup_errno = 0;
				sbufh_p->failed = 0;
#				ifdef DEBUG
				/* Once the process register its pid as backup_pid, sleep for the 1 seconds, so that test
				 * tests hits the scenario where BKUPRUNNING message is printed. Note that sleep of 1 sec
				 * is fine because it is the box boxes which are failing to hit the concurrency scenario.
				 */
				if (gtm_white_box_test_case_enabled && (WBTEST_CONCBKUP_RUNNING == gtm_white_box_test_case_number))
					LONG_SLEEP(1);
#				endif
				/* Make sure that the backup queue does not have any remnants on it. Note that we do not
				   depend on the queue count here as it is imperative that, in the event that the count
				   and queue get out of sync, that there ARE NO BLOCKS on this queue when we start or
				   the backup will have unknown potentially very stale blocks resulting in at best bad
				   data progressing to severe integrity errors.
				*/
				for (sblkh_p = SBLKP_REL2ABS(&sbufh_p->que_backup, fl);
				     sblkh_p != (shmpool_blk_hdr_ptr_t)&sbufh_p->que_backup;
				     sblkh_p = next_sblkh_p)
				{	/* Loop through the queued backup blocks */
					VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
					VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_backup);
					next_sblkh_p = SBLKP_REL2ABS(sblkh_p, fl);	/* Get next offset before remove entry */
					shmpool_blk_free(rptr->reg, sblkh_p);
				}
				/* Unlock everything and let the backup proceed */
				shmpool_unlock_hdr(gv_cur_region);
				/* Signal to GT.M processes to start backing up before-images of any GDS block that gets changed
				 * starting now. To do that set "nbb" to -1 as that is smaller than every valid block number in db
				 * (starting from 0 upto a max. of 2^28-1 which is lesser than 2^31-1=BACKUP_NOT_IN_PROGRESS)
				 */
				cs_addrs->nl->nbb = -1; /* start */
				/* Decerement counter so that inhibited KILLs can now proceed */
				DECR_INHIBIT_KILLS(cs_addrs->nl);
				rel_crit(rptr->reg);
			} else
				rel_crit(rptr->reg);
		}
		jgbl.dont_reset_gbl_jrec_time = FALSE;
		/* Ensure jgbl.gbl_jrec_time did not get reset by any of the jnl writing functions */
		assert(save_gbl_jrec_time == jgbl.gbl_jrec_time);
		for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
		{
			if (rptr->not_this_time > keep_going)
				continue;
			/*
			 * For backed up database we want to change some file header fields.
			 */
			rptr->backup_hdr->freeze = FALSE;
			rptr->backup_hdr->image_count = 0;
			if (rptr->backup_hdr->kill_in_prog)
				rptr->backup_hdr->abandoned_kills = TRUE;
			rptr->backup_hdr->kill_in_prog = FALSE;
			memset(rptr->backup_hdr->machine_name, 0, MAX_MCNAMELEN);
			rptr->backup_hdr->repl_state = repl_closed;
			rptr->backup_hdr->semid = INVALID_SEMID;
			rptr->backup_hdr->shmid = INVALID_SHMID;
			rptr->backup_hdr->gt_sem_ctime.ctime = 0;
			rptr->backup_hdr->gt_shm_ctime.ctime = 0;
			if ((jnl_options[jnl_off] || bkdbjnl_off_specified) && (jnl_notallowed != gv_cur_region->jnl_state))
				rptr->backup_hdr->jnl_state = jnl_closed;
			if (jnl_options[jnl_disable] || bkdbjnl_disable_specified)
			{
				rptr->backup_hdr->jnl_state = jnl_notallowed;
				rptr->backup_hdr->jnl_file_len = 0;
				rptr->backup_hdr->jnl_file_name[0] = '\0';
			}
			if (jnl_options[jnl_off] || bkdbjnl_off_specified ||
					jnl_options[jnl_disable] || bkdbjnl_disable_specified)
				gtm_putmsg_csa(CSA_ARG(REG2CSA(rptr->reg)) VARLSTCNT(8) ERR_JNLSTATE, 6, LEN_AND_LIT(FILE_STR),
					rptr->backup_file.len, rptr->backup_file.addr,
					LEN_AND_STR(jnl_state_lit[rptr->backup_hdr->jnl_state]));
		}
		ENABLE_AST
		for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
		{
			if (rptr->not_this_time > keep_going)
				continue;
			gv_cur_region = rptr->reg;
			TP_CHANGE_REG(gv_cur_region);	/* sets cs_addrs and cs_data which mubinccpy/mubfilcpy rely on */
			if ((cs_addrs->onln_rlbk_cycle != cs_addrs->nl->onln_rlbk_cycle)
				|| (0 != cs_addrs->nl->onln_rlbk_pid))
			{	/* A concurrent online rollback happened since we did the gvcst_init or one is going on right now.
				 * In either case, the BACKUP is unreliable. Cleanup and exit
				 */
				error_mupip = TRUE;
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DBROLLEDBACK);
				break;
			}
			result = (incremental ? mubinccpy(rptr) : mubfilcpy(rptr));
			if (FALSE == result)
			{
				if (file_backed_up)
					util_out_print("Files backed up before above error are OK.", TRUE);
				error_mupip = TRUE;
				break;
			}
			if (mu_ctrly_occurred || mu_ctrlc_occurred)
				break;
		}
	} else
	{
		mubclnup((backup_reg_list *)halt_ptr, need_to_rel_crit);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_BACKUPCTRL);
		mupip_exit(ERR_MUNOFINISH);
	}
	/* =============================== STEP 5. clean up  ============================================== */
	mubclnup((backup_reg_list *)halt_ptr, need_to_del_tempfile);
	REVERT;
	if (mu_ctrly_occurred || mu_ctrlc_occurred)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_BACKUPCTRL);
		ret = ERR_MUNOFINISH;
	} else if (TRUE == error_mupip)
		ret = ERR_MUNOFINISH;
	else
		util_out_print("!/!/BACKUP COMPLETED.!/", TRUE);
	gv_cur_region = NULL;
	mupip_exit(ret);
}
