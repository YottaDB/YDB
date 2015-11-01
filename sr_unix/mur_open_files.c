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

#include "mdef.h"

#include <netinet/in.h>
#include "gtm_inet.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "cli.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "hashdef.h"
#include "jnl.h"
#include "muprec.h"
#include "io.h"
#include "iosp.h"
#include "copy.h"
#include "gtmio.h"
#include "gdskill.h"
#include "collseq.h"
#include "gdscc.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "parse_file.h"
#include "lockconst.h"
#include "aswp.h"
#include "eintr_wrappers.h"
#include "io_params.h"
#include "rename_file_if_exists.h"
#include "gbldirnam.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "util.h"
#include "send_msg.h"
#include "gvcst_init.h"
#include "mu_rndwn_file.h"
#include "mu_rndwn_replpool.h"
#include "ipcrmid.h"
#include "dpgbldir.h"
#include "dpgbldir_sysops.h"
#include "gtmmsg.h"
#include "ftok_sems.h"
#include "repl_instance.h"
#include "mu_rndwn_repl_instance.h"

#define	TMP_BUF_LEN	50

error_def(ERR_JNLREADEOF);
error_def(ERR_MUSTANDALONE);
error_def(ERR_DBPRIVERR);
error_def(ERR_STARTRECOVERY);
error_def(ERR_RENAMEFAIL);

GBLDEF	char		*mur_extract_buff;
GBLDEF	int		mur_extract_bsize;

GBLREF	boolean_t	mupip_jnl_recover;
GBLREF	int		(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF	int4		gv_keysize;
GBLREF	mur_opt_struct	mur_options;
GBLREF  seq_num		resync_jnl_seqno;
GBLREF	seq_num		consist_jnl_seqno;
GBLREF	seq_num		max_reg_seqno;
GBLREF	int4		n_regions;
GBLREF	char		*log_rollback;
GBLREF	seq_num		seq_num_zero;
GBLREF	seq_num		max_resync_seqno;
GBLDEF	uint4		dbrdwr_status;
GBLREF	jnl_proc_time	min_jnl_rec_time;

static	sgmnt_data_ptr_t	csd;

void mur_open_files_error(ctl_list *curr, int fd)
{
	int status;
	if (curr->rab)
	{
		mur_close(curr->rab);
		curr->rab = NULL;
	}
	if (NULL != curr->gd  &&  0 != curr->gd->dyn.addr->fname_len)
	{
		if (curr->db_ctl != NULL  &&  curr->db_ctl->file_info != NULL)
		{
			free(curr->db_ctl->file_info);
			curr->db_ctl->file_info = NULL;
		}
		curr->gd->dyn.addr->fname_len = 0;
	}
	if (NO_FD_OPEN < fd)
	{
		CLOSEFILE(fd, status);
		if (-1 == status)
			gtm_putmsg(VARLSTCNT(1) errno);
	}
	if (NULL != curr->gd)
	{
		if (curr->gd->dyn.addr != NULL)
			free(curr->gd->dyn.addr);
		free(curr->gd);
		curr->gd = NULL;
	}
	if (mur_options.update)
		free(csd);
}

/*
 *	This routine performs four basic functions:
 *
 *	  1  It opens all of the journal files specified on the command line,
 *	     and initializes the relevant data structures;
 *
 *	  2  If the /EXTRACT qualifier was specified, it opens the associated
 *	     file;
 *
 *	  3  It converts any delta times specified in command qualifiers to the
 *	     equivalent absolute times, based on the latest timestamp among
 *	     the last data records of all of the journal files, and validates
 *	     the results.
 *
 *	  4  It sorts the list of journal files, so that any files that are
 *	     logically contiguous are both adjacent within the list as well as
 *	     in the proper sequence;  and
 *
 *	It returns TRUE if all of the above functions were successful.
 */

bool	mur_open_files(ctl_list **jnl_files)
{
	ctl_list		*curr, *prev;
	jnl_file_header		*header;
	jnl_proc_time		max_time = 0;
	jnl_record		*rec;
	redirect_list		*rl_ptr;
	sgmnt_addrs		*csa;
	fi_type			*extr_file_info;
	fi_type			*losttrans_file_info;
	char			*c, *c1, *ctop, fn[MAX_FN_LEN], rename_fn[MAX_FN_LEN];
	bool 			standalone = FALSE, crash_occurred = FALSE;
	int			db_fd, fn_len, rename_len;
	int4			info_status;
	uint4			status;
	unsigned int		full_len;
	unsigned char		*ptr, qwstring[100];
	unsigned char		*ptr1, qwstring1[100];
	mval			op_val, op_pars;
	replpool_identifier	replpool_id;
	static readonly unsigned char		open_params_list[8]=
	{
		(unsigned char)iop_newversion,
		(unsigned char)iop_noreadonly,
		(unsigned char)iop_nowrap,
		(unsigned char)iop_stream,
		(unsigned char)iop_recordsize, (unsigned char)0x07F,(unsigned char)0x07F,
		(unsigned char)iop_eol
	};

	error_def(ERR_TEXT);

	if (mur_options.update)
	{
		mupip_jnl_recover = TRUE;
		csd = (sgmnt_data_ptr_t)malloc(ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE));
	}
	/* Open all of the journal files */
	for (curr = *jnl_files;  curr != NULL;  curr = curr->next)
	{
		n_regions++;
		/* Open journal file stand-alone */
		curr->rab = mur_rab_create(MINIMUM_BUFFER_SIZE);
		if ((status = mur_fopen(curr->rab, curr->jnl_fn, curr->jnl_fn_len)) != SS_NORMAL)
		{
			gtm_putmsg(VARLSTCNT(1) status);
			mur_open_files_error(curr, NO_FD_OPEN);
			return FALSE;
		}
		header = mur_get_file_header(curr->rab);
		if (!mur_options.forward  &&  !header->before_images)
		{
			util_out_print("Journal file !AD does not contain before-images;  cannot do backward recovery", TRUE,
					curr->jnl_fn_len, curr->jnl_fn);
			mur_open_files_error(curr, NO_FD_OPEN);
			return FALSE;
		}
		if (mur_options.rollback  &&  !header->before_images)
		{
			util_out_print("Journal file !AD does not contain before-images; cannot do rollback recovery FOR NOW",
					TRUE, curr->jnl_fn_len, curr->jnl_fn);
			mur_open_files_error(curr, NO_FD_OPEN);
			return FALSE;
		}
		if (mur_options.rollback  &&  repl_closed == header->repl_state)
		{
			util_out_print("Journal file !AD is NOT REPLICATION-ENABLED. Cannot do rollback", TRUE,
					curr->jnl_fn_len, curr->jnl_fn);
			mur_open_files_error(curr, NO_FD_OPEN);
			return FALSE;
		}
		if (mur_options.losttrans_file_info != NULL)
		{
			if (header->max_record_length > mur_extract_bsize)
				mur_extract_bsize = header->max_record_length;
			if (mur_extract_bsize == 0)
				mur_extract_bsize = 10240;
		}
		if (mur_options.extr_file_info != NULL)
		{
			if (header->max_record_length > mur_extract_bsize)
				mur_extract_bsize = header->max_record_length;
		}
		if (mur_extract_bsize == 0)
			mur_extract_bsize = 10240;

		if (mur_options.update)
		{
		        fn_len = 0;
		        if (mur_options.redirect)
			{
				for (rl_ptr = mur_options.redirect;  rl_ptr != NULL;  rl_ptr = rl_ptr->next)
				{
					if (header->data_file_name_length == rl_ptr->org_name_len  &&
					    memcmp(header->data_file_name, rl_ptr->org_name, rl_ptr->org_name_len) == 0)
					{
						fn_len = rl_ptr->new_name_len;
						memcpy(fn, rl_ptr->new_name, fn_len);
					}
				}
			}
			if (fn_len == 0)
			{
				fn_len = header->data_file_name_length;
				memcpy(fn, header->data_file_name, fn_len);
			}
			fn[fn_len] = '\0';

			/* See if the database file is the same as one previously opened here */
			for (prev = *jnl_files;  prev != curr;  prev = prev->next)
			{
				if (prev->gd->dyn.addr->fname_len == fn_len  &&
					memcmp(prev->gd->dyn.addr->fname, fn, fn_len) == 0)
				{
					/* It is;  just reuse the same info */
					curr->gd = prev->gd;
					curr->db_ctl = prev->db_ctl;
					curr->db_tn = prev->db_tn;
					curr->turn_around_tn = prev->turn_around_tn;
					curr->jnl_tn = prev->jnl_tn;
					curr->jnl_state = prev->jnl_state;
					curr->repl_state = prev->repl_state;
					curr->tab_ptr = prev->tab_ptr;
					if ((status = mur_fread_eof(curr->rab, curr->jnl_fn, curr->jnl_fn_len)) != SS_NORMAL)
					{
						gtm_putmsg(VARLSTCNT(1) status);
						mur_open_files_error(curr, NO_FD_OPEN);
						return FALSE;
					}
					break;
				}
			}
			if (prev != curr)
				continue;

			/* Open the database, turn journalling off, and then close it */

			curr->tab_ptr = (void *)malloc(sizeof(htab_desc));
			ht_init((htab_desc *)curr->tab_ptr, 0);

			curr->gd = (gd_region *)malloc(sizeof(gd_region));
			memset(curr->gd, 0, sizeof(gd_region));

			curr->gd->dyn.addr = (gd_segment *)malloc(sizeof(gd_segment));
			memset(curr->gd->dyn.addr, 0, sizeof(gd_segment));
			curr->gd->dyn.addr->acc_meth = dba_bg;

			curr->gd->dyn.addr->file_cntl = (file_control *)malloc(sizeof(file_control));
			memset(curr->gd->dyn.addr->file_cntl, 0, sizeof(file_control));

			curr->gd->dyn.addr->file_cntl->file_info = (void *)malloc(sizeof(unix_db_info));
			memset(curr->gd->dyn.addr->file_cntl->file_info, 0, sizeof(unix_db_info));

			curr->gd->dyn.addr->fname_len = fn_len;
			memcpy(curr->gd->dyn.addr->fname, fn, fn_len);

			standalone = mu_rndwn_file(curr->gd, TRUE);
			if (FALSE == standalone)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_MUSTANDALONE, 2, fn_len, fn);
				db_ipcs_reset(curr->gd, TRUE);
				mur_open_files_error(curr, NO_FD_OPEN);
				return FALSE;
			}
		}
		/* Rundown the Jnlpool and Recvpool for rollback */
		if (mur_options.rollback &&
			repl_inst_get_name((char *)replpool_id.instname, &full_len, sizeof(replpool_id.instname)) &&
				!mu_rndwn_repl_instance(&replpool_id))
					return FALSE;
		if ((status = mur_fread_eof(curr->rab, curr->jnl_fn, curr->jnl_fn_len)) != SS_NORMAL)
		{
			gtm_putmsg(VARLSTCNT(1) status);
			mur_open_files_error(curr, NO_FD_OPEN);
			return FALSE;
		}
		if (mur_options.since_time <= 0  ||  mur_options.before  &&  mur_options.before_time < 0)
		{
			for (status = mur_get_last(curr->rab); status == SS_NORMAL; status = mur_previous(curr->rab, 0))
			{
				if (curr->rab->pvt->jfh->crash)
					crash_occurred = TRUE;
				rec = (jnl_record *)curr->rab->recbuff;
				switch (REF_CHAR(&rec->jrec_type))
				{
				default:
					continue;
				case JRT_EOF:
					if (JNL_S_TIME(rec, jrec_eof) > max_time)
						max_time = JNL_S_TIME(rec, jrec_eof);
					break;
				case JRT_KILL:
				case JRT_FKILL:
				case JRT_GKILL:
				case JRT_TKILL:
				case JRT_UKILL:
				case JRT_SET:
				case JRT_FSET:
				case JRT_GSET:
				case JRT_TSET:
				case JRT_USET:
				case JRT_TCOM:
				case JRT_ZTCOM:
				case JRT_PBLK:
                                case JRT_EPOCH:
				case JRT_ZKILL:
				case JRT_FZKILL:
				case JRT_GZKILL:
				case JRT_TZKILL:
				case JRT_UZKILL:
				case JRT_INCTN:
				case JRT_AIMG:
					/* jrec_kill is used here as a "generic" data record type */
					if (rec->val.jrec_kill.short_time > max_time)
						max_time = rec->val.jrec_kill.short_time;
					break;
				}
				break;
			}
			if (status != SS_NORMAL  &&  status != ERR_JNLREADEOF)
				mur_jnl_read_error(curr, status, FALSE);
		}
		if (mur_options.update)
		{
			curr->db_ctl = (file_control *)malloc(sizeof(file_control));
			memset(curr->db_ctl, 0, sizeof(file_control));
                        send_msg(VARLSTCNT(6) ERR_STARTRECOVERY, 4, fn_len, fn, curr->jnl_fn_len, curr->jnl_fn);
			OPENFILE(fn, O_RDWR, db_fd);
			if (-1 == db_fd)
			{
				DEBUG_ONLY(dbrdwr_status = ERR_DBPRIVERR);
				util_out_print("Error opening database file !AZ - status:  ", TRUE, fn);
				mur_output_status(errno);
				db_ipcs_reset(curr->gd, TRUE);
				mur_open_files_error(curr, db_fd);
				return FALSE;
			}
			LSEEKREAD(db_fd, 0, csd, sizeof(sgmnt_data), status);
			if (0 != status)
			{
				if (-1 != status)
				{
					util_out_print("Error reading database file !AZ - status:  ", TRUE, fn);
					mur_output_status(status);
				} else
					util_out_print("Premature end-of-file for database file !AZ", TRUE, fn);
				db_ipcs_reset(curr->gd, TRUE);
				mur_open_files_error(curr, db_fd);
				return FALSE;
			}
			if (!mur_options.forward && csd->jnl_state == jnl_notallowed)
			{
				util_out_print("Journalling is not enabled for database file !AZ ", TRUE, fn);
				db_ipcs_reset(curr->gd, TRUE);
				mur_open_files_error(curr, db_fd);
				return FALSE;
			}
			csd->file_corrupt = TRUE; /* Indicating that recovery is initiated as database was corrupt most probably */
			curr->jnl_state = csd->jnl_state;
			curr->repl_state = csd->repl_state;

			if (log_rollback)
			{
				ptr = i2ascl(qwstring, csd->reg_seqno);
				ptr1 = i2asclx(qwstring1, csd->reg_seqno);
				util_out_print("MUR-I-DEBUG : Region !AZ --> RegionSeqno = !AD [0x!AD]", TRUE,
								fn, ptr-qwstring, qwstring, ptr1 - qwstring1, qwstring1);
			}
			if (mur_options.rollback)
			{
				if (QWLT(max_reg_seqno, csd->reg_seqno))
					QWASSIGN(max_reg_seqno, csd->reg_seqno);
				if (QWLT(max_resync_seqno, csd->resync_seqno))
					QWASSIGN(max_resync_seqno, csd->resync_seqno);
			}
			csd->jnl_state = jnl_notallowed;
			csd->repl_state = repl_closed;
			memset(csd->machine_name, 0, MAX_MCNAMELEN);

			curr->db_tn = csd->trans_hist.curr_tn;
			curr->turn_around_tn = csd->trans_hist.curr_tn;
			curr->jnl_tn = (trans_num)-1;
			LSEEKWRITE(db_fd, 0, csd, sizeof(sgmnt_data), status);
			if (0 != status)
			{
				util_out_print("Error opening database file !AZ - status:  ", TRUE, fn);
				mur_output_status(status);
				db_ipcs_reset(curr->gd, TRUE);
				mur_open_files_error(curr, db_fd);
				return FALSE;
			}
			CLOSEFILE(db_fd, status);
			if (-1 == status)
			{
				status = errno;
				util_out_print("Error closing database file !AZ - status : ", TRUE, fn);
				mur_output_status(status);
				db_ipcs_reset(curr->gd, TRUE);
			}
			gvcst_init(curr->gd);		/* Should never happen as only open one */
			if (curr->gd->was_open)		/* at a time, but handle for safety */
			{
				util_out_print("Error opening database file !AZ", TRUE, fn);
				mur_open_files_error(curr, NO_FD_OPEN);
				return FALSE;
			}
			if (!mur_options.forward)
				curr->db_ctl->file_info = curr->gd->dyn.addr->file_cntl->file_info;
			csa = &FILE_INFO(curr->gd)->s_addrs;
			if (((csa->hdr->max_key_size + MAX_NUM_SUBSC_LEN + 4) & (-4)) > gv_keysize)
				gv_keysize = (csa->hdr->max_key_size + MAX_NUM_SUBSC_LEN + 4) & (-4);
			csa->now_crit = FALSE;
		}
	}
	if (log_rollback)
	{
		ptr = i2ascl(qwstring, max_reg_seqno);
		ptr1 = i2asclx(qwstring1, max_reg_seqno);
		util_out_print("MUR-I-DEBUG : MaxRegionSeqno = !AD [0x!AD] ", TRUE,
					ptr-qwstring, qwstring, ptr1 - qwstring1, qwstring1);
	}
	if (mur_options.update)
		free(csd);
	if ((losttrans_file_info = (fi_type *)mur_options.losttrans_file_info) != NULL)
	{
		if (losttrans_file_info->fn_len == 0)
		{
			/* -LOSTTRANS was specified without a filename;  use the
			   same name as that of the first (or only) journal file */
			curr = *jnl_files;
			c = curr->jnl_fn;
			ctop = c + curr->jnl_fn_len;
			c1 = fn;
			while (c < ctop  &&  *c != '.')
				*c1++ = *c++;
			strcpy(c1, ".ltr");
			fn_len = strlen(fn);
			losttrans_file_info->fn = (char *)malloc(fn_len + 1);
			strcpy(losttrans_file_info->fn, fn);
			losttrans_file_info->fn_len = fn_len;
		}
		op_pars.mvtype = MV_STR;
		op_pars.str.len = sizeof(open_params_list);
		op_pars.str.addr = (char *)open_params_list;
		op_val.mvtype = MV_STR;
		op_val.str.len = losttrans_file_info->fn_len;
		op_val.str.addr = (char *)losttrans_file_info->fn;
		if (RENAME_FAILED == rename_file_if_exists(losttrans_file_info->fn, losttrans_file_info->fn_len,
                        &info_status, rename_fn, &rename_len))
                        gtm_putmsg(VARLSTCNT(7) ERR_RENAMEFAIL, 5, losttrans_file_info->fn_len,
                                losttrans_file_info->fn, rename_fn, rename_len, info_status);
		if((status = (*op_open_ptr)(&op_val, &op_pars, 0, 0)) == 0)
		{
			gtm_putmsg(VARLSTCNT(1)errno);
			util_out_print("Error opening Losttrans file !AD ", TRUE,
					losttrans_file_info->fn_len, losttrans_file_info->fn);
			return FALSE;
		}
		CHMOD(losttrans_file_info->fn, 0666);
		mur_extract_buff = (char *)malloc(mur_extract_bsize + 256);
	}

	/* Open the extraction file, if specified */
	if ((extr_file_info = (fi_type *)mur_options.extr_file_info) != NULL)
	{
		if (extr_file_info->fn_len == 0)
		{
			/* -EXTRACT was specified without a filename;  use the
			   same name as that of the first (or only) journal file */
			curr = *jnl_files;
			c = curr->jnl_fn;
			ctop = c + curr->jnl_fn_len;
			c1 = fn;
			while (c < ctop  &&  *c != '.')
				*c1++ = *c++;
			strcpy(c1, ".mjf");
			fn_len = strlen(fn);
			extr_file_info->fn = (char *)malloc(fn_len + 1);
			strcpy(extr_file_info->fn, fn);
			extr_file_info->fn_len = fn_len;
		}
		if (RENAME_FAILED == rename_file_if_exists(extr_file_info->fn, extr_file_info->fn_len,
                        &info_status, rename_fn, &rename_len))
                        gtm_putmsg(VARLSTCNT(7) ERR_RENAMEFAIL, 5, extr_file_info->fn_len,  extr_file_info->fn,
                                rename_len, rename_fn, info_status);
		op_pars.mvtype = MV_STR;
		op_pars.str.len = sizeof(open_params_list);
		op_pars.str.addr = (char *)open_params_list;
		op_val.mvtype = MV_STR;
		op_val.str.len = extr_file_info->fn_len;
		op_val.str.addr = (char *)extr_file_info->fn;
		if((status = (*op_open_ptr)(&op_val, &op_pars, 0, 0)) == 0)
		{
			gtm_putmsg(VARLSTCNT(1)errno);
			util_out_print("Error opening Extract file !AD ", TRUE, extr_file_info->fn_len, extr_file_info->fn);
			return FALSE;
		}
		mur_extract_buff = (char *)malloc(mur_extract_bsize + 256);
	}

	/* Convert any delta times specified in the command line, and validate that all times are consistent. */
	if (mur_options.since_time <= 0)
		mur_options.since_time += max_time; /* add delta time */
	if (mur_options.before)
	{
		if (mur_options.before_time <= 0)
			mur_options.before_time += max_time; /* add delta time */
		if (mur_options.before_time < mur_options.since_time)
		{
			util_out_print("-BEFORE time precedes -SINCE or -AFTER time", TRUE);
			return FALSE;
		}
	}
	if (mur_options.lookback_time <= 0)
	{	/* lookback_time is delta */
		mur_options.lookback_time += mur_options.since_time;
	} else
	{	/* lookback_time is absolute */
		if (mur_options.lookback_time > mur_options.since_time)
		{
			util_out_print("-LOOKBACK_LIMIT time is later than -SINCE time", TRUE);
			return FALSE;
		}
	}
	if (!mur_options.lookback_time_specified)
		if (crash_occurred)
			min_jnl_rec_time = mur_options.since_time - DEFAULT_EPOCH_INTERVAL;
		else
			min_jnl_rec_time = mur_options.since_time;
	else
		min_jnl_rec_time = mur_options.lookback_time;
	return mur_sort_and_checktn(jnl_files);
}
