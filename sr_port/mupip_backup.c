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

#include "mdef.h"

#if defined(UNIX)
# include "gtm_fcntl.h"
# include <sys/stat.h>
# include "gtm_unistd.h"
#elif defined(VMS)
# include <rms.h>
# include <iodef.h>
#else
# error Unsupported Platform
#endif
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_tempnam.h"
#include "gtm_time.h"

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
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "io.h"
#include "interlock.h"
#include "lockconst.h"
#include "sleep_cnt.h"
#if defined(UNIX)
#include "eintr_wrappers.h"
#endif
#include "gtm_file_stat.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "gt_timer.h"
#include "gvcst_init.h"
#include "is_proc_alive.h"
#include "is_file_identical.h"
#include "dbfilop.h"
#include "mupip_exit.h"
#include "mu_getlst.h"
#include "mu_outofband_setup.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "wcs_flu.h"
#include "trans_log_name.h"
#include "mupip_backup.h"

#if defined(UNIX)
# define PATH_DELIM		'/'
# define TEMPDIR_LOG_NAME	"$GTM_BAKTMPDIR"
#elif defined(VMS)
# define PATH_DELIM		']'
# define TEMPDIR_LOG_NAME	"GTM_BAKTMPDIR"
static  const   unsigned short  zero_fid[3];
#else
# error Unsupported Platform
#endif

#ifdef __MVS__
#define TMPDIR_ACCESS_MODE	(R_OK | W_OK | X_OK)
#else
#define TMPDIR_ACCESS_MODE	(R_OK | W_OK | X_OK | F_OK)
#endif


GBLREF 	bool		record;
GBLREF 	bool		error_mupip;
GBLREF 	bool		file_backed_up;
GBLREF 	bool		incremental;
GBLREF 	bool		online;
GBLREF 	uchar_ptr_t	mubbuf;
GBLREF 	int4		mubmaxblk;
GBLREF	tp_region	*grlist;
GBLREF 	tp_region 	*halt_ptr;
GBLREF 	bool		in_backup;
GBLREF 	bool		is_directory;
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
GBLREF	jnl_gbls_t	jgbl;

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

void mupip_backup(void)
{
	bool		journal;
	char		*tempdirname, *tempfilename, *ptr;
	uint4		level, blk, status, ret;
	unsigned short	s_len, length, ntries;
	int4		size, gds_ratio, buff_size, i, crit_counter;
	uint4		ustatus;
	size_t		backup_buf_size;
	trans_num	tn;
	backup_buff_ptr_t	bptr;
	backup_reg_list	*rptr, *clnup_ptr;
	static bool	once = TRUE;
	bool		inc_since_inc , inc_since_rec, result, newjnlfiles, gotit,
			newjnlfiles_specified, keep_prev_link, bkdbjnl_disable_specified, bkdbjnl_off_specified;
	unsigned char	since_buff[50];
	jnl_create_info jnl_info;
	file_control	*fc;
	char            tempdir_trans_buffer[MAX_TRANS_NAME_LEN],
			tempnam_prefix[MAX_FN_LEN], tempdir_full_buffer[MAX_FN_LEN + 1], jnl_file_name[JNL_NAME_SIZE];
	char		*jnl_str_ptr, rep_str[256], jnl_str[256], entry[256],
			full_jnl_fn[JNL_NAME_SIZE], prev_jnl_fn[JNL_NAME_SIZE];
	int		ccnt, index, comparison, num, jnl_fstat;
	mstr            tempdir_log, tempdir_trans, *file, tempdir_full, filestr;
	uint4		jnl_status, temp_file_name_len, tempdir_trans_len, trans_log_name_status;

#if defined(VMS)
	struct FAB	temp_fab;
	struct NAM	temp_nam;
	struct XABPRO	temp_xabpro;
	short		iosb[4];
	char		def_jnl_fn[MAX_FN_LEN];
	GDS_INFO	*gds_info;
	char		exp_file_name[MAX_FN_LEN];
	uint4		exp_file_name_len;
#elif defined(UNIX)
	struct stat     stat_buf;
	int		fstat_res;
#else
# error UNSUPPORTED PLATFORM
#endif
	boolean_t	jnl_options[jnl_end_of_list] = {FALSE, FALSE, FALSE}, save_no_prev_link;


	error_def(ERR_BACKUPCTRL);
	error_def(ERR_MUPCLIERR);
	error_def(ERR_FREEZECTRL);
	error_def(ERR_DBRDONLY);
	error_def(ERR_DBFILERR);
	error_def(ERR_MUNOACTION);
	error_def(ERR_MUNOFINISH);
	error_def(ERR_BACKUP2MANYKILL);
        error_def(ERR_MUSELFBKUP);
        error_def(ERR_JNLNOCREATE);
        error_def(ERR_JNLCREATE);
	error_def(ERR_PREVJNLLINKCUT);
	error_def(ERR_JNLSTATE);
	error_def(ERR_FILEEXISTS);
	error_def(ERR_JNLDISABLE);
	error_def(ERR_FILEPARSE);
	error_def(ERR_JNLFNF);
	error_def(ERR_NOTRNDMACC);

	/* ==================================== STEP 1. Initialization ======================================= */

	ret = SS_NORMAL;
	jnl_str_ptr = &jnl_str[0];
	halt_ptr = grlist = NULL;
	in_backup = TRUE;
	inc_since_inc = inc_since_rec = file_backed_up = error_mupip = FALSE;
	debug_mupip = (CLI_PRESENT == cli_present("DBG"));

	mu_outofband_setup();
	jnl_status = 0;
	if (once)
	{
		gvinit();
		once = FALSE;
	}

	/* ============================ STEP 2. Parse and construct grlist ================================== */

	if (incremental = (CLI_PRESENT == cli_present("INCREMENTAL") || CLI_PRESENT == cli_present("BYTESTREAM")))
	{
		int4 temp_tn;

		if (0 == cli_get_hex("TRANSACTION", &temp_tn))
		{
			temp_tn = 0;
			s_len = sizeof(since_buff);
			if (cli_get_str("SINCE", (char *)since_buff, &s_len))
			{
				lower_to_upper(since_buff, since_buff, s_len);
				if ((0 == memcmp(since_buff, "INCREMENTAL", s_len))
					|| (0 == memcmp(since_buff, "BYTESTREAM", s_len)))
					inc_since_inc = TRUE;
				else if (0 == memcmp(since_buff, "RECORD", s_len))
					inc_since_rec = TRUE;
			}
		} else if (temp_tn < 1)
		{
			util_out_print("The minimum allowable transaction number is one.", TRUE);
			mupip_exit(ERR_MUNOACTION);
		}
		tn = (trans_num)temp_tn;
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
	} else if (CLI_NEGATED == cli_present("NEWJNLFILES"))
	{
		keep_prev_link = FALSE; /* for safety */
		newjnlfiles_specified = TRUE;
		newjnlfiles = FALSE;
	}
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
		s_len = sizeof(jnl_str);
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
	mu_getlst("REG_NAME", sizeof(backup_reg_list));
	if (error_mupip)
	{
		mubclnup(NULL, need_to_free_space);
		util_out_print("!/MUPIP cannot start backup with above errors!/", TRUE);
		mupip_exit(ERR_MUNOACTION);
	}
	if (mu_ctrly_occurred || mu_ctrlc_occurred)
	{
		mubclnup(NULL, need_to_free_space);
		gtm_putmsg(VARLSTCNT(1) ERR_BACKUPCTRL);
		mupip_exit(ERR_MUNOFINISH);
	}
	for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
	{
		file = &(rptr->backup_file);
		file->addr[file->len] = '\0';
		if ((backup_to_file == rptr->backup_to))
		{	/* make sure that backup won't be overwriting the database file itself */
			if (TRUE == is_file_identical(file->addr, (char *)rptr->reg->dyn.addr->fname))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_MUSELFBKUP, 2, DB_LEN_STR(rptr->reg));
				error_mupip = TRUE;
			}
			UNIX_ONLY(
				if (CLI_PRESENT != cli_present("REPLACE"))
				{	/* make sure backup files do not already exist */
					if (FILE_PRESENT == (fstat_res = gtm_file_stat(file, NULL, NULL, FALSE, &ustatus)))
					{
						gtm_putmsg(VARLSTCNT(4) ERR_FILEEXISTS, 2, LEN_AND_STR((char *)file->addr));
						error_mupip = TRUE;
					} else if (FILE_STAT_ERROR == fstat_res)
						gtm_putmsg(VARLSTCNT(1) ustatus);
				}
			)
		} else if (!incremental)
		{ 	/* non-incremental backups to "exec" and "tp" are not supported*/
			gtm_putmsg(VARLSTCNT(1) ERR_NOTRNDMACC);
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
	size = ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);

	ESTABLISH(mu_freeze_ch);
	tempfilename = tempdir_full.addr = tempdir_full_buffer;
	if (TRUE == online)
	{
		tempdir_log.addr = TEMPDIR_LOG_NAME;
		tempdir_log.len = sizeof(TEMPDIR_LOG_NAME) - 1;
		trans_log_name_status = trans_log_name(&tempdir_log, &tempdir_trans, tempdir_trans_buffer);
		/* save the length of the "base" so we can (restore it and) re-use the string in tempdir_trans.addr */
		tempdir_trans_len = tempdir_trans.len;
	} else
		tempdir_trans_len = 0;

	for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
	{
		/* restore the original length since we are looping thru regions */
		tempdir_trans.len = tempdir_trans_len;
		file = &(rptr->backup_file);
		file->addr[file->len] = '\0';
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
				break;
		if ((dba_bg != rptr->reg->dyn.addr->acc_meth) && (dba_mm != rptr->reg->dyn.addr->acc_meth))
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
		gvcst_init(gv_cur_region);
		if (gv_cur_region->was_open)
		{
			gv_cur_region->open = FALSE;
			rptr->not_this_time = give_up_before_create_tempfile;
			continue;
		}
		TP_CHANGE_REG(gv_cur_region);
		if (gv_cur_region->read_only)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
			rptr->not_this_time = give_up_before_create_tempfile;
			continue;
		}
		rptr->backup_hdr = (sgmnt_data_ptr_t)malloc(size);
		if (TRUE == online)
		{
			/* determine the directory name and prefix for the temp file */
			memset(tempnam_prefix, 0, MAX_FN_LEN);
			memcpy(tempnam_prefix, gv_cur_region->rname, gv_cur_region->rname_len);
			SPRINTF(&tempnam_prefix[gv_cur_region->rname_len], "_%x", process_id);

			if ((SS_NORMAL == trans_log_name_status)
				&& (NULL != tempdir_trans.addr) && (0 != tempdir_trans.len))
				*(tempdir_trans.addr + tempdir_trans.len) = 0;
			else if (incremental && (backup_to_file != rptr->backup_to))
			{
				tempdir_trans.addr = tempdir_trans_buffer;
				tempdir_trans.len = sizeof(SCRATCH_DIR) - 1;
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
					memcpy(tempdir_trans_buffer, rptr->backup_file.addr,
						(tempdir_trans.len = ptr - rptr->backup_file.addr + 1));
					tempdir_trans_buffer[tempdir_trans.len] = '\0';
				} else
#if defined(UNIX)
				{
					tempdir_trans_buffer[0] = '.';
					tempdir_trans_buffer[1] = '\0';
					tempdir_trans.len = 1;
				}
			}

			/* verify the accessibility of the tempdir */
			if (FILE_STAT_ERROR == (fstat_res = gtm_file_stat(&tempdir_trans, NULL, &tempdir_full, FALSE, &ustatus)))
			{
				gtm_putmsg(VARLSTCNT(5) ERR_FILEPARSE, 2, tempdir_trans.len, tempdir_trans.addr, ustatus);
				mupip_exit(ustatus);
			}
			SPRINTF(tempfilename + tempdir_full.len,"/%s_XXXXXX",tempnam_prefix);
			MKSTEMP(tempfilename, rptr->backup_fd);
			if (-1 == rptr->backup_fd )
			{
				status = errno;
				if ((NULL != tempdir_full.addr) &&
					(0 != ACCESS(tempdir_full.addr, TMPDIR_ACCESS_MODE)))
				{
					status = errno;
					util_out_print("!/Do not have full access to directory for temporary files: !AD", TRUE,
							tempdir_trans.len, tempdir_trans.addr);
					gtm_putmsg(VARLSTCNT(1) status);
					util_out_print("!/MUPIP cannot start backup with above errors!/", TRUE);
					mubclnup(rptr, need_to_del_tempfile);
					mupip_exit(status);
				}
				util_out_print("!/Cannot create the temporary file in directory !AD for online backup", TRUE,
						tempdir_trans.len, tempdir_trans.addr);
				gtm_putmsg(VARLSTCNT(1) status);
				util_out_print("!/MUPIP cannot start backup with above errors!/", TRUE);
				mubclnup(rptr, need_to_del_tempfile);
				mupip_exit(status);

			}
			tempdir_full.len = strlen(tempdir_full.addr); /* update the length */
			if (debug_mupip)
				util_out_print("!/MUPIP INFO:   Temp file name: !AD", TRUE,tempdir_full.len, tempdir_full.addr);
			memcpy(&rptr->backup_tempfile[0], tempdir_full.addr, tempdir_full.len);
			rptr->backup_tempfile[tempdir_full.len] = 0;

			/* give temporary files the same set of permission as the database files */
			FSTAT_FILE(((unix_db_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fd, &stat_buf, fstat_res);
			if ((-1 == fstat_res)
				|| (-1 == fchmod(rptr->backup_fd, stat_buf.st_mode)))
			{
				status = errno;
				gtm_putmsg(VARLSTCNT(1) status);
				util_out_print("!/MUPIP cannot start backup with above errors!/", TRUE);
				mubclnup(rptr, need_to_del_tempfile);
				mupip_exit(status);
			}
#elif defined(VMS)
					assert(FALSE);
			}

			temp_xabpro = cc$rms_xabpro;
			temp_xabpro.xab$w_pro = ((vms_gds_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->xabpro->xab$w_pro
							& (~((XAB$M_NODEL << XAB$V_SYS) | (XAB$M_NODEL << XAB$V_OWN)));
			temp_nam = cc$rms_nam;
			temp_nam.nam$l_rsa = rptr->backup_tempfile;
			temp_nam.nam$b_rss = sizeof(rptr->backup_tempfile) - 1;	/* temp solution, note it is a byte value */

			temp_fab = cc$rms_fab;
			temp_fab.fab$l_nam = &temp_nam;
			temp_fab.fab$l_xab = &temp_xabpro;
		        temp_fab.fab$b_org = FAB$C_SEQ;
		        temp_fab.fab$l_fop = FAB$M_MXV | FAB$M_CBT | FAB$M_TEF | FAB$M_CIF;
		        temp_fab.fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_BIO | FAB$M_TRN;

			gtm_tempnam(tempdir_trans.addr, tempnam_prefix, tempfilename);
			temp_file_name_len = exp_file_name_len = strlen(tempfilename);
			memcpy(exp_file_name, tempfilename, temp_file_name_len);
			if (!get_full_path(tempfilename, temp_file_name_len, exp_file_name,
						&exp_file_name_len, sizeof(exp_file_name), &ustatus))
			{
				util_out_print("!/Unable to resolve concealed definition for file !AD ", TRUE,
						temp_file_name_len, tempfilename);
				gtm_putmsg(VARLSTCNT(1) ustatus);
				mubclnup(rptr, need_to_del_tempfile);
				mupip_exit(ERR_MUNOACTION);
			}
			if (debug_mupip)
				util_out_print("!/MUPIP INFO:   Temp file name: !AD", TRUE, exp_file_name_len, exp_file_name);

			temp_fab.fab$l_fna = exp_file_name;
			temp_fab.fab$b_fns = exp_file_name_len;

			ntries = 0;
			gotit = FALSE;
			while (TRUE != gotit)
			{
				switch (status = sys$create(&temp_fab))
	        		{
				        case RMS$_CREATED:
						gotit = TRUE;
						break;
					case RMS$_NORMAL:
				        case RMS$_SUPERSEDE:
				        case RMS$_FILEPURGED:
						sys$close(&temp_fab);
						ntries++;
						gtm_tempnam(tempdir_trans.addr, tempnam_prefix, tempfilename);
			                        temp_fab.fab$l_fna = tempfilename;
			                        temp_fab.fab$b_fns = strlen(tempfilename);
				                break;
				        default:
						error_mupip = TRUE;
        			}
				if (error_mupip || (ntries > MAX_TEMP_OPEN_TRY))
				{
					util_out_print("!/Cannot create the temporary file !AD for online backup.", TRUE,
							LEN_AND_STR(tempfilename));
					gtm_putmsg(VARLSTCNT(1) status);
					mubclnup(rptr, need_to_del_tempfile);
                                        mupip_exit(ERR_MUNOACTION);
				}
			}
			rptr->backup_tempfile[temp_nam.nam$b_rsl] = '\0';
			sys$close(&temp_fab);
#else
# error Unsupported Platform
#endif
		} else
		{
			while (FALSE == region_freeze(gv_cur_region, TRUE, FALSE))
			{
				hiber_start(1000);
				if ((TRUE == mu_ctrly_occurred) || (TRUE == mu_ctrlc_occurred))
				{
					mubclnup(rptr, need_to_del_tempfile);
					gtm_putmsg(VARLSTCNT(1) ERR_FREEZECTRL);
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
		gtm_putmsg(VARLSTCNT(1) ERR_BACKUPCTRL);
		mupip_exit(ERR_MUNOFINISH);
	}
	if (online)
	{
		for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
		{
			if (rptr->not_this_time <= keep_going)
			{
				crit_counter = 1;
				do
				{
					TP_CHANGE_REG(rptr->reg);
					grab_crit(gv_cur_region);
					if (0 == cs_addrs->hdr->kill_in_prog)
						break;
					rel_crit(gv_cur_region);
					wcs_sleep(crit_counter);
				} while (++crit_counter < MAX_CRIT_TRY);
				if (crit_counter >= MAX_CRIT_TRY)
				{
					mubclnup(rptr, need_to_rel_crit);
					mupip_exit(ERR_BACKUP2MANYKILL);
				}
			}
		}
	}

	/* ========================== STEP 4. Flush cache, calculate tn for incremental, and do backup =========== */

	if ((FALSE == mu_ctrly_occurred) && (FALSE == mu_ctrlc_occurred))
	{
		mup_bak_pause(); /* ? save some crit time? */
		for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
		{
			if (rptr->not_this_time > keep_going)
				continue;
			TP_CHANGE_REG(rptr->reg);
			wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH);
			if (incremental)
			{
				if (inc_since_inc)
					tn = cs_data->last_inc_backup;
				else if (inc_since_rec)
					tn = cs_data->last_rec_backup;
				else if (0 == tn)
					tn = cs_data->last_com_backup;
				rptr->tn = tn;
			}
			memcpy(rptr->backup_hdr, cs_data, size);
			if (online) /* save a copy of the fileheader, modify current fileheader and release crit */
			{
				bptr = cs_addrs->backup_buffer;
				if (BACKUP_NOT_IN_PROGRESS != cs_addrs->nl->nbb)
				{
					if (TRUE == is_proc_alive(bptr->backup_pid, bptr->backup_image_count))
					{
					    	/* someone else is doing the backup */
						util_out_print("!/Process !UL is backing up region !AD now.", TRUE,
								bptr->backup_pid, REG_LEN_STR(rptr->reg));
						rptr->not_this_time = give_up_after_create_tempfile;
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
						cre_jnl_file_intrpt_rename(((int)cs_data->jnl_file_len), cs_data->jnl_file_name);
					if (JNL_ALLOWED(cs_data))
					{
						memset(&jnl_info, 0, sizeof(jnl_info));
						jnl_info.prev_jnl = &prev_jnl_fn[0];
						set_jnl_info(gv_cur_region, &jnl_info);
						save_no_prev_link = jnl_info.no_prev_link =  (jnl_options[jnl_noprevjnlfile] ||
							!keep_prev_link || !JNL_ENABLED(cs_data)) ? TRUE : FALSE;
						JNL_SHORT_TIME(jgbl.gbl_jrec_time);	/* jnl_file_close & cre_jnl_file need it */
						VMS_ONLY(
							gds_info = FILE_INFO(gv_cur_region);
								/* Is it possible for gds_info to be uninitialized? */
							assert(jnl_info.fn_len == gds_info->fab->fab$b_fns);
							assert(0 == memcmp(jnl_info.fn, gds_info->fab->fab$l_fna, jnl_info.fn_len));
						)
						if (JNL_ENABLED(cs_data) &&
							UNIX_ONLY((0 != cs_addrs->nl->jnl_file.u.inode))
							VMS_ONLY((0 != memcmp(cs_addrs->nl->jnl_file.jnl_file_id.fid,
									zero_fid, sizeof(zero_fid)))))
						{	/* Note: following will again call wcs_flu() */
							if (SS_NORMAL != (status = set_jnl_file_close(SET_JNL_FILE_CLOSE_BACKUP)))
							{
								util_out_print("!/Journal file !AD not closed:",
									TRUE, jnl_info.jnl_len, jnl_info.jnl);
								gtm_putmsg(VARLSTCNT(1) status);
								rptr->not_this_time = give_up_after_create_tempfile;
								rel_crit(rptr->reg);
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
								gtm_putmsg(VARLSTCNT(5) ERR_JNLFNF, 2,
									filestr.len, filestr.addr, ustatus);
								rptr->not_this_time = give_up_after_create_tempfile;
								rel_crit(rptr->reg);
								continue;
							}
							jnl_info.no_rename = (FILE_NOT_FOUND == jnl_fstat);
						}
						wcs_flu(WCSFLU_FSYNC_DB | WCSFLU_FLUSH_HDR);	/* For VMS WCSFLU_FSYNC_DB
												 * is ignored */
						if (EXIT_NRM == cre_jnl_file(&jnl_info))
						{
							if (jnl_info.no_prev_link && (save_no_prev_link != jnl_info.no_prev_link))
								gtm_putmsg(VARLSTCNT(6) ERR_PREVJNLLINKCUT, 4,
									JNL_LEN_STR(cs_data), DB_LEN_STR(rptr->reg));
							memcpy(cs_data->jnl_file_name, jnl_info.jnl, jnl_info.jnl_len);
							cs_data->jnl_file_name[jnl_info.jnl_len] = '\0';
							cs_data->jnl_file_len = jnl_info.jnl_len;
							cs_data->jnl_buffer_size = jnl_info.buffer;
							cs_data->jnl_alq = jnl_info.alloc;
							cs_data->jnl_deq = jnl_info.extend;
							cs_data->jnl_before_image = jnl_info.before_images;
							cs_data->trans_hist.header_open_tn = jnl_info.tn;
							cs_data->jnl_state = jnl_open;
							gtm_putmsg(VARLSTCNT(10) ERR_JNLCREATE, 8, jnl_info.jnl_len, jnl_info.jnl,
								LEN_AND_LIT("region"), REG_LEN_STR(gv_cur_region),
								LEN_AND_STR(before_image_lit[(jnl_info.before_images ? 1 : 0)]));
							if (JNL_ENABLED(cs_data) &&
								(jnl_options[jnl_noprevjnlfile] || !keep_prev_link))
								gtm_putmsg(VARLSTCNT(6) ERR_PREVJNLLINKCUT,
									4, JNL_LEN_STR(cs_data), DB_LEN_STR(gv_cur_region));
							fc = gv_cur_region->dyn.addr->file_cntl;
							fc->op = FC_WRITE;
							fc->op_buff = (sm_uc_ptr_t)cs_data;
							status = dbfilop(fc);
							if (SS_NORMAL != status)
							{
								UNIX_ONLY(gtm_putmsg(VARLSTCNT(7) ERR_DBFILERR, 2,
										DB_LEN_STR(gv_cur_region), 0, status, 0);)
								VMS_ONLY(gtm_putmsg(VARLSTCNT(9) ERR_DBFILERR, 2,
										DB_LEN_STR(gv_cur_region), 0, status, 0,
										gds_info->fab->fab$l_stv, 0);)
								rptr->not_this_time = give_up_after_create_tempfile;
								rel_crit(rptr->reg);
								continue;
							}
						} else
						{
							gtm_putmsg(VARLSTCNT(4) ERR_JNLNOCREATE, 2, jnl_info.jnl_len, jnl_info.jnl);
							rptr->not_this_time = give_up_after_create_tempfile;
							rel_crit(rptr->reg);
							continue;
						}
					} else
						gtm_putmsg(VARLSTCNT(4) MAKE_MSG_WARNING(ERR_JNLDISABLE),
										2, DB_LEN_STR(gv_cur_region));
				}
				memset(bptr, 0, sizeof(backup_buff));
				bptr->size = BACKUP_BUFFER_SIZE - sizeof(backup_buff);
				memcpy(&(bptr->tempfilename[0]), &(rptr->backup_tempfile[0]),
						strlen(rptr->backup_tempfile));
				bptr->tempfilename[strlen(rptr->backup_tempfile)] = 0;
				bptr->backup_tn = cs_addrs->ti->curr_tn;
				bptr->backup_pid = process_id;
				bptr->inc_backup_tn = (incremental ? tn : 0);
				SET_LATCH_GLOBAL(&bptr->backup_ioinprog_latch, LOCK_AVAILABLE);
				VMS_ONLY(SET_LATCH(&bptr->backup_ioinprog_latch.latch_word, LOCK_AVAILABLE));
				VMS_ONLY(bptr->backup_image_count = image_count;)
				cs_addrs->nl->nbb = -1; /* start */
				rel_crit(rptr->reg);
			}
		}
		for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
		{
			if (rptr->not_this_time > keep_going)
				continue;
			/*
			 * For backed up database we want to change some file header fields.
			 */
			rptr->backup_hdr->freeze = 0;
			rptr->backup_hdr->image_count = 0;
			rptr->backup_hdr->kill_in_prog = 0;
			rptr->backup_hdr->owner_node = 0;
			rptr->backup_hdr->trans_hist.header_open_tn = rptr->backup_hdr->trans_hist.curr_tn;
			memset(rptr->backup_hdr->machine_name, 0, MAX_MCNAMELEN);
			rptr->backup_hdr->repl_state = repl_closed;
			rptr->backup_hdr->semid = INVALID_SEMID;
			rptr->backup_hdr->shmid = INVALID_SHMID;
			rptr->backup_hdr->sem_ctime.ctime = 0;
			rptr->backup_hdr->shm_ctime.ctime = 0;
			if (jnl_options[jnl_off] || bkdbjnl_off_specified)
				rptr->backup_hdr->jnl_state = jnl_closed;
			if (jnl_options[jnl_disable] || bkdbjnl_disable_specified)
			{
				rptr->backup_hdr->jnl_state = jnl_notallowed;
				rptr->backup_hdr->jnl_file_len = 0;
				rptr->backup_hdr->jnl_file_name[0] = '\0';
			}
			if (jnl_options[jnl_off] || bkdbjnl_off_specified ||
					jnl_options[jnl_disable] || bkdbjnl_disable_specified)
				gtm_putmsg(VARLSTCNT(8) ERR_JNLSTATE, 6, LEN_AND_LIT(FILE_STR),
					rptr->backup_file.len, rptr->backup_file.addr,
					LEN_AND_STR(jnl_state_lit[rptr->backup_hdr->jnl_state]));
		}
		ENABLE_AST
		mubbuf = (uchar_ptr_t)malloc(BACKUP_READ_SIZE);
		for (rptr = (backup_reg_list *)(grlist);  NULL != rptr;  rptr = rptr->fPtr)
		{
			gv_cur_region = rptr->reg;
			if (rptr->not_this_time > keep_going)
				continue;
			cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
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
		free(mubbuf);
	} else
	{
		mubclnup((backup_reg_list *)halt_ptr, need_to_rel_crit);
		gtm_putmsg(VARLSTCNT(1) ERR_BACKUPCTRL);
		mupip_exit(ERR_MUNOFINISH);
	}

	/* =============================== STEP 5. clean up  ============================================== */

	mubclnup((backup_reg_list *)halt_ptr, need_to_del_tempfile);

	REVERT;

	if (mu_ctrly_occurred || mu_ctrlc_occurred)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_BACKUPCTRL);
		ret = ERR_MUNOFINISH;
	} else if (TRUE == error_mupip)
		ret = ERR_MUNOFINISH;
	else
		util_out_print("!/!/BACKUP COMPLETED.!/", TRUE);
	gv_cur_region = NULL;
	mupip_exit(ret);
}
