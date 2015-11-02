/****************************************************************
 *								*
 *	Copyright 2003, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "min_max.h"
#include "cli.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "io.h"
#include "iosp.h"
#include "dpgbldir.h"
#ifdef UNIX
#include "ftok_sems.h"
#include "repl_instance.h"
#include "mu_rndwn_replpool.h"
#include "mu_rndwn_repl_instance.h"
#elif defined(VMS)
#include <descrip.h>
#include "gtm_inet.h"
#include "iosb_disk.h"	/* For mur_read_file.h */
#include "dpgbldir_sysops.h"
#include "gbldirnam.h"
#include "repl_sem.h"
#include "gtmrecv.h"
#endif
#include "mu_rndwn_file.h"
#include "read_db_files_from_gld.h"
#include "mur_db_files_from_jnllist.h"
#include "gtm_rename.h"
#include "gtmmsg.h"
#include "file_head_read.h"
#include "mupip_exit.h"
#include "mu_gv_cur_reg_init.h"
#include "dbfilop.h"
#include "mur_read_file.h"	/* for mur_fread_eof() prototype */
#include "tp_change_reg.h"
#include "gds_rundown.h"
#include "is_file_identical.h"
#include "repl_msg.h"
#include "gtmsource.h"

GBLREF  int		mur_regno;
GBLREF	jnl_ctl_list	*mur_jctl;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	mur_opt_struct	mur_options;
GBLREF 	mur_gbls_t	murgbl;
GBLREF	gd_region	*gv_cur_region;
GBLREF	jnlpool_addrs	jnlpool;

#define            		STAR_QUOTE "\"*\""
boolean_t mur_open_files()
{
	int                             jnl_total, jnlno, regno, max_reg_total;
	unsigned short			jnl_file_list_len; /* cli_get_str requires a short */
	char                            jnl_file_list[MAX_LINE];
	char				*cptr, *cptr_last, *ctop;
	jnl_ctl_list                    *jctl, *temp_jctl;
	reg_ctl_list			*rctl, *rctl_top, tmp_rctl;
	mval                            mv;
	gld_dbname_list			*gld_db_files, *curr;
	gd_addr                 	*temp_gd_header;
	boolean_t                       star_specified;
	redirect_list			*rl_ptr;
	unsigned int			full_len;
	replpool_identifier		replpool_id;
	sgmnt_data_ptr_t		csd;
	sgmnt_addrs			*csa;
	file_control			*fc;
	bool				reg_frz_status;
#ifdef VMS
	uint4				status;
	boolean_t			sgmnt_found;
	mstr				gbldir_mstr, *tran_name;
	gds_file_id			file_id;
	struct dsc$descriptor_s 	name_dsc;
	char            		res_name[MAX_NAME_LEN + 2];/* +1 for the terminating null and
						another +1 for the length stored in [0] by global_name() */
	error_def (ERR_MURPOOLRNDWNSUC);
	error_def (ERR_MURPOOLRNDWNFL);
	error_def (ERR_MUJPOOLRNDWNSUC);
	error_def (ERR_MUJPOOLRNDWNFL);
#endif

	error_def (ERR_MUPCLIERR);
	error_def (ERR_FILENOTFND);
	error_def (ERR_MUSTANDALONE);
	error_def (ERR_DBJNLNOTMATCH);
	error_def (ERR_JNLNMBKNOTPRCD);
	error_def (ERR_NOTALLJNLEN);
	error_def (ERR_NOTALLREPLON);
	error_def (ERR_JNLDBTNNOMATCH);
	error_def (ERR_NOPREVLINK);
	error_def (ERR_JNLTNOUTOFSEQ);
	error_def (ERR_STARFILE);
	error_def (ERR_NOSTARFILE);
	error_def (ERR_MUPCLIERR);
	error_def (ERR_DBFRZRESETSUC);
	error_def (ERR_FILEPARSE);
	error_def (ERR_JNLBADRECFMT);
	error_def (ERR_JNLSTATEOFF);
	error_def (ERR_REPLSTATEOFF);
	error_def (ERR_REPLINSTUNDEF);
	error_def (ERR_MUPJNLINTERRUPT);
	error_def (ERR_ROLLBKINTERRUPT);
	error_def (ERR_DBRDONLY);
	error_def (ERR_DBFRZRESETFL);
	error_def (ERR_DBFILOPERR);
	error_def (ERR_TEXT);

	jnl_file_list_len = MAX_LINE;
	if (FALSE == CLI_GET_STR_ALL("FILE", jnl_file_list, &jnl_file_list_len))
		mupip_exit(ERR_MUPCLIERR);
	if ((1 == jnl_file_list_len && '*' == jnl_file_list[0]) ||
		(STR_LIT_LEN(STAR_QUOTE) == jnl_file_list_len &&
		0 == memcmp(jnl_file_list, STAR_QUOTE, STR_LIT_LEN(STAR_QUOTE))))
	{
		star_specified = TRUE;
		if (NULL != mur_options.redirect)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_STARFILE, 2, LEN_AND_LIT("REDIRECT qualifier"));
			mupip_exit(ERR_MUPCLIERR);
		}
	} else
	{
		star_specified = FALSE;
		if (mur_options.rollback)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_NOSTARFILE, 2, LEN_AND_LIT("ROLLBACK qualifier"));
			mupip_exit(ERR_MUPCLIERR);
		}
	}
	/* We assume recovery will be done only on current global directory.
	 * That is, journal file names specified must be from current global directory.
	 */
	if (star_specified)
	{
		mv.mvtype = MV_STR;
		mv.str.len = 0;
		temp_gd_header = zgbldir(&mv);
		max_reg_total = temp_gd_header->n_regions;
		gld_db_files = read_db_files_from_gld(temp_gd_header);
	} else
		gld_db_files = mur_db_files_from_jnllist(jnl_file_list, jnl_file_list_len, &max_reg_total);
	if (NULL == gld_db_files)
		return FALSE;
	mur_ctl = (reg_ctl_list *)malloc(sizeof(reg_ctl_list) * max_reg_total);
	memset(mur_ctl, 0, sizeof(reg_ctl_list) * max_reg_total);
	curr = gld_db_files;
	murgbl.max_extr_record_length = DEFAULT_EXTR_BUFSIZE;
	murgbl.repl_standalone = FALSE;
	if (mur_options.rollback)
	{	/* Rundown the Jnlpool and Recvpool */
#if defined(UNIX)
		if (!repl_inst_get_name((char *)replpool_id.instfilename, &full_len, sizeof(replpool_id.instfilename)))
		{
			gtm_putmsg(VARLSTCNT(1) ERR_REPLINSTUNDEF);
			return FALSE;
		}
		if (!mu_rndwn_repl_instance(&replpool_id, FALSE))
			return FALSE;	/* mu_rndwn_repl_instance will have printed appropriate message in case of error */
		assert(NULL == jnlpool.repl_inst_filehdr);
		murgbl.repl_standalone = mu_replpool_grab_sem(FALSE);
		assert(NULL != jnlpool.repl_inst_filehdr);
#elif defined(VMS)
		gbldir_mstr.addr = DEF_GDR;
		gbldir_mstr.len = sizeof(DEF_GDR) - 1;
		tran_name = get_name(&gbldir_mstr);
		memcpy(replpool_id.gtmgbldir, tran_name->addr, tran_name->len);
		full_len = tran_name->len;
		if (!get_full_path(replpool_id.gtmgbldir, tran_name->len,
				replpool_id.gtmgbldir, &full_len, MAX_TRANS_NAME_LEN, &status))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_FILENOTFND, 2, tran_name->len, tran_name->addr);
			return FALSE;
		}
		else
		{
			tran_name->len = full_len;	/* since on vax, mstr.len is a 'short' */
			set_gdid_from_file((gd_id_ptr_t)&file_id, replpool_id.gtmgbldir, tran_name->len);
			global_name("GT$P", &file_id, res_name); /* P - Stands for Journal Pool */
			res_name[res_name[0] + 1] = '\0';
			STRCPY(replpool_id.repl_pool_key, &res_name[1]);
			replpool_id.pool_type = JNLPOOL_SEGMENT;
			sgmnt_found = FALSE;
			if (mu_rndwn_replpool(&replpool_id, FALSE, &sgmnt_found) && sgmnt_found)
				gtm_putmsg(VARLSTCNT(6) ERR_MUJPOOLRNDWNSUC, 4, res_name[0], &res_name[1],
						tran_name->len, replpool_id.gtmgbldir);
			else if (sgmnt_found)
			{
				gtm_putmsg(VARLSTCNT(6) ERR_MUJPOOLRNDWNFL, 4, res_name[0], &res_name[1],
						tran_name->len, replpool_id.gtmgbldir);
				return FALSE;
			}
			global_name("GT$R", &file_id, res_name); /* R - Stands for Recv Pool */
			res_name[res_name[0] + 1] = '\0';
			STRCPY(replpool_id.repl_pool_key, &res_name[1]);
			replpool_id.pool_type = RECVPOOL_SEGMENT;
			sgmnt_found = FALSE;
			if (mu_rndwn_replpool(&replpool_id, FALSE, &sgmnt_found) && sgmnt_found)
				gtm_putmsg(VARLSTCNT(6) ERR_MURPOOLRNDWNSUC, 4, res_name[0], &res_name[1],
						tran_name->len, replpool_id.gtmgbldir);
			else if (sgmnt_found)
			{
				gtm_putmsg(VARLSTCNT(6) ERR_MURPOOLRNDWNFL, 4, res_name[0], &res_name[1],
					tran_name->len, replpool_id.gtmgbldir);
				return FALSE;
			}
		}
#endif
	}
	for (murgbl.reg_full_total = 0, rctl = mur_ctl, rctl_top = mur_ctl + max_reg_total;
										rctl < rctl_top; rctl++, curr = curr->next)
	{	/* Do region specific initialization */
		rctl->gd = curr->gd;	/* region structure is already set with proper values */
		rctl->standalone = FALSE;
		rctl->csa = NULL;
		rctl->csd = NULL;
		rctl->jctl = rctl->jctl_head = rctl->jctl_alt_head = rctl->jctl_turn_around = rctl->jctl_apply_pblk = NULL;
		murgbl.reg_full_total++;	/* mur_close_files() expects rctl->csa and rctl->jctl to be initialized.
						 * so consider this rctl only after those have been initialized. */
		init_hashtab_mname(&rctl->gvntab, 0);	/* for mur_forward() */
		rctl->db_ctl = (file_control *)malloc(sizeof(file_control));
		memset(rctl->db_ctl, 0, sizeof(file_control));
		/* For redirect we just need to change the name of database. recovery will redirect to new database file */
		if (mur_options.redirect)
		{
			for (rl_ptr = mur_options.redirect;  rl_ptr != NULL;  rl_ptr = rl_ptr->next)
			{
				if (curr->gd->dyn.addr->fname_len == rl_ptr->org_name_len &&
				    0 == memcmp(curr->gd->dyn.addr->fname, rl_ptr->org_name, rl_ptr->org_name_len))
				{
					curr->gd->dyn.addr->fname_len = rl_ptr->new_name_len;
					memcpy(curr->gd->dyn.addr->fname, rl_ptr->new_name, curr->gd->dyn.addr->fname_len);
					curr->gd->dyn.addr->fname[curr->gd->dyn.addr->fname_len] = 0;
				}
			}
		}
        	if (!mupfndfil(curr->gd, NULL))
		{
			if (mur_options.update || star_specified)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_FILENOTFND, 2, DB_LEN_STR(curr->gd));
				return FALSE;
			}
			/* else for 1) show 2) extract 3) verify action qualifier ,
			 * we do not need database to be present unless star (*) specified.
			 * For star we need to get journal file name from database file header,
			 * so database must be present in the system.
                	 * NOTE: csa and csd fields are NULL, if database is not present */
		} else /* database present */
		{
			if (mur_options.update)
			{	/* recover and rollback qualifiers always require standalone access */
				gv_cur_region = rctl->gd;	/* mu_rndwn_file() assumes gv_cur_region is set in VMS */
				if (!STANDALONE(rctl->gd))	/* STANDALONE macro calls mu_rndwn_file() */
				{
					gtm_putmsg(VARLSTCNT(4) ERR_MUSTANDALONE, 2, DB_LEN_STR(rctl->gd));
					return FALSE;
				}
				rctl->standalone = TRUE;
			}
			if (mur_options.update || mur_options.extr[GOOD_TN])
			{	/* NOTE: Only for collation info extract needs database access */
        			gvcst_init(rctl->gd);
				csa = rctl->csa = &FILE_INFO(rctl->gd)->s_addrs;
				csd = rctl->csd = rctl->csa->hdr;
				if (mur_options.update)
				{
					assert(!csa->nl->donotflush_dbjnl);
					csa->nl->donotflush_dbjnl = TRUE; /* indicate gds_rundown/mu_rndwn_file to not wcs_flu()
									   * this shared memory until recover/rlbk cleanly exits */
				}
				if (rctl->gd->read_only && mur_options.update)
				{
					gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(rctl->gd));
					return FALSE;
				}
				assert(!JNL_ENABLED(csd) || 0 == csd->jnl_file_name[csd->jnl_file_len]);
				rctl->db_ctl->file_type = rctl->gd->dyn.addr->file_cntl->file_type;
				rctl->db_ctl->file_info = rctl->gd->dyn.addr->file_cntl->file_info;
				rctl->recov_interrupted = csd->recov_interrupted;
				if (mur_options.update && rctl->recov_interrupted)
				{	/* interrupted recovery might have changed current csd's jnl_state/repl_state.
					 * restore the state of csd before the start of interrupted recovery.
					 */
					if (mur_options.forward)
					{	/* error out. need fresh backup of database for forward recovery */
						gtm_putmsg(VARLSTCNT(4) ERR_MUPJNLINTERRUPT, 2, DB_LEN_STR(rctl->gd));
						return FALSE;
					}
					/* In case rollback with non-zero resync_seqno got interrupted, we would have
					 * written intrpt_recov_resync_seqno. If so, cannot use RECOVER now.
					 * In all other cases, intrpt_recov_resolve_time would have been written.
					 */
					if (!mur_options.rollback && csd->intrpt_recov_resync_seqno)
					{
						gtm_putmsg(VARLSTCNT(4) ERR_ROLLBKINTERRUPT, 2, DB_LEN_STR(rctl->gd));
						return FALSE;
					}
					csd->jnl_state = csd->intrpt_recov_jnl_state;
					csd->repl_state = csd->intrpt_recov_repl_state;
				}
				/* Save current states */
				rctl->jnl_state = csd->jnl_state;
				rctl->repl_state = csd->repl_state;
				rctl->before_image = csd->jnl_before_image;
				if (mur_options.update)
				{
					if (!mur_options.rollback)
					{
						if (!mur_options.forward && !JNL_ENABLED(csd))
						{
							if (!star_specified)
							{
								gtm_putmsg(VARLSTCNT(4) ERR_JNLSTATEOFF, 2, DB_LEN_STR(rctl->gd));
								return FALSE;
							}
							continue;
						}
					} else
					{
						if (!REPL_ALLOWED(csd))
						{
							if (JNL_ENABLED(csd))
							{
								gtm_putmsg(VARLSTCNT(4) ERR_REPLSTATEOFF, 2, DB_LEN_STR(rctl->gd));
								return FALSE;
							}
							continue;
						}
					}
					if (csd->freeze)
					{	/* region_freeze should release freeze here */
						reg_frz_status = region_freeze(curr->gd, FALSE, TRUE);
						assert (0 == rctl->csa->hdr->freeze);
						assert(reg_frz_status);
						if (!reg_frz_status)
						{
							gtm_putmsg(VARLSTCNT(4) ERR_DBFRZRESETFL, 2, DB_LEN_STR(curr->gd));
							return FALSE;
						}
						gtm_putmsg(VARLSTCNT(4) ERR_DBFRZRESETSUC, 2, DB_LEN_STR(curr->gd));
					}
					/* save current jnl/repl state before changing in case recovery is interrupted */
					csd->intrpt_recov_jnl_state = csd->jnl_state;
					csd->intrpt_recov_repl_state = csd->repl_state;
					csd->recov_interrupted = TRUE;
					/* Temporarily change current state. mur_close_files() will restore them as appropriate */
					if (mur_options.forward && JNL_ENABLED(csd))
						csd->jnl_state = jnl_closed;
					csd->repl_state = repl_closed;
					csd->file_corrupt = TRUE;
					/* flush the changed csd to disk */
					fc = rctl->gd->dyn.addr->file_cntl;
					fc->op = FC_WRITE;
					fc->op_buff = (sm_uc_ptr_t)csd;
					fc->op_len = ROUND_UP(SIZEOF_FILE_HDR(csd), DISK_BLOCK_SIZE);
					fc->op_pos = 1;
					dbfilop(fc);
				}
				rctl->db_tn = csd->trans_hist.curr_tn;
				/* Some routines use csa */
				csa->jnl_state= csd->jnl_state;
				csa->repl_state = csd->repl_state;
				csa->jnl_before_image = csd->jnl_before_image;
			} else
			{	/* NOTE: csa field is NULL, if we do not open database */
				csd = rctl->csd = (sgmnt_data_ptr_t)malloc(SGMNT_HDR_LEN);
				assert(0 == curr->gd->dyn.addr->fname[curr->gd->dyn.addr->fname_len]);
				/* 1) show 2) extract 3) verify action does not need standalone access.
				 * In this case csa is NULL */
				if (!file_head_read((char *)curr->gd->dyn.addr->fname, rctl->csd, SGMNT_HDR_LEN))
				{
					gtm_putmsg(VARLSTCNT(4) ERR_DBFILOPERR, 2, REG_LEN_STR(rctl->gd));
					return FALSE;
				}
				rctl->jnl_state = csd->jnl_state;
				rctl->repl_state = csd->repl_state;
				rctl->before_image = csd->jnl_before_image;
			}
			/* For star_specified we open journal files here.
			 * For star_specified we cannot do anything if journaling is disabled */
			if (star_specified && JNL_ALLOWED(csd))
			{	/* User implicitly specified current generation journal files.
                		 * Only one journal per region is specified. So open them now.
				 */
        			mur_jctl = jctl = (jnl_ctl_list *)malloc(sizeof(jnl_ctl_list));
				memset(jctl, 0, sizeof(jnl_ctl_list));
                        	rctl->jctl_head = rctl->jctl = jctl;
                		jctl->jnl_fn_len = csd->jnl_file_len;
				memcpy(jctl->jnl_fn, csd->jnl_file_name, csd->jnl_file_len);
				jctl->jnl_fn[jctl->jnl_fn_len] = 0;
        			/* If system crashed during rename, following will fix it .
                        	 * Following function is directly related to cre_jnl_file_common */
                		cre_jnl_file_intrpt_rename(jctl->jnl_fn_len, jctl->jnl_fn);
                        	if (!mur_fopen(jctl))
                                	return FALSE;
				/* note mur_fread_eof must be done after setting mur_ctl, mur_regno and mur_jctl */
				mur_regno = (rctl - &mur_ctl[0]);
				if (SS_NORMAL != (jctl->status = mur_fread_eof(jctl)))
				{
					gtm_putmsg(VARLSTCNT(9) ERR_JNLBADRECFMT, 3, jctl->jnl_fn_len, jctl->jnl_fn,
						jctl->rec_offset, ERR_TEXT, 2, LEN_AND_LIT("mur_fread_eof failed"));
					return FALSE;
				}
				if (!is_file_identical((char *)jctl->jfh->data_file_name, (char *)rctl->gd->dyn.addr->fname))
				{
					gtm_putmsg(VARLSTCNT(8) ERR_DBJNLNOTMATCH, 6, DB_LEN_STR(rctl->gd),
						jctl->jnl_fn_len, jctl->jnl_fn,
						jctl->jfh->data_file_name_length, jctl->jfh->data_file_name);
					return FALSE;
				}
			}
        	}  /* mupfndfil */
	} /* End for */
	/* At this point mur_ctl[] has been created from the current global directory database file names
	 * or from the journal file header's database names.
	 * For star_specified == TRUE implicitly only current generation journal files are specified and already opened
	 * For star_specified == FALSE user can specify multiple generations. We need to sort them */
	if (!star_specified)
	{
		jnlno = 0;
		cptr = jnl_file_list;
		ctop = &jnl_file_list[jnl_file_list_len];
		while (cptr < ctop)
		{
			mur_jctl = jctl = (jnl_ctl_list *)malloc(sizeof(jnl_ctl_list));
			memset(jctl, 0, sizeof(jnl_ctl_list));
			cptr_last = cptr;
			while (0 != *cptr && ',' != *cptr && '"' != *cptr &&  ' ' != *cptr)
				++cptr;
			if (!get_full_path(cptr_last, cptr - cptr_last,
						(char *)jctl->jnl_fn, &jctl->jnl_fn_len, MAX_FN_LEN, &jctl->status2))
			{
				gtm_putmsg(VARLSTCNT(5) ERR_FILEPARSE, 2, cptr_last, cptr - cptr_last, jctl->status2);
				return FALSE;
			}
			cptr++;	/* skip separator */
			/* Note cre_jnl_file_intrpt_rename was already called in mur_db_files_from_jnllist */
			if (!mur_fopen(jctl))
				return FALSE;
			for (mur_regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_full_total;
					rctl < rctl_top; rctl++, mur_regno++)
			{
				if (rctl->gd->dyn.addr->fname_len == jctl->jfh->data_file_name_length &&
						(0 == memcmp(jctl->jfh->data_file_name, rctl->gd->dyn.addr->fname,
								rctl->gd->dyn.addr->fname_len)))
					break;
			}
			if (rctl == rctl_top)
			{
				for (rl_ptr = mur_options.redirect;  (NULL != rl_ptr);  rl_ptr = rl_ptr->next)
				{
					if ((jctl->jfh->data_file_name_length == rl_ptr->org_name_len)
							&& (0 == memcmp(jctl->jfh->data_file_name,
									rl_ptr->org_name, rl_ptr->org_name_len)))
						break;
				}
				if (NULL != rl_ptr)
				{
					for (mur_regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_full_total;
							rctl < rctl_top; rctl++, mur_regno++)
					{
						if (rctl->gd->dyn.addr->fname_len == rl_ptr->new_name_len &&
								(0 == memcmp(rctl->gd->dyn.addr->fname, rl_ptr->new_name,
										rl_ptr->new_name_len)))
							break;
					}
				}
				if (rctl == rctl_top)
					GTMASSERT;/* db list was created from journal file header. So it is not possible */
			}
			/* note mur_fread_eof must be done after setting mur_ctl, mur_regno and mur_jctl */
			if (SS_NORMAL != (jctl->status = mur_fread_eof(jctl)))
			{
				gtm_putmsg(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, jctl->jnl_fn_len, jctl->jnl_fn, jctl->rec_offset);
				return FALSE;
			}
			/* Now, we have found the region for this jctl */
			csd = rctl->csd;
			if (!mur_options.forward)
			{
				rctl->jctl = rctl->jctl_head = jctl;
				if (mur_options.update && !is_file_identical((char *)csd->jnl_file_name, (char *)jctl->jnl_fn))
				{
					gtm_putmsg(VARLSTCNT(8) ERR_JNLNMBKNOTPRCD, 6, jctl->jnl_fn_len, jctl->jnl_fn,
							JNL_LEN_STR(csd), DB_LEN_STR(rctl->gd));
					return FALSE;
				}
			} else
			{	/* rctl->jctl_head will have the lowest bov_tn of the journals of the region
				 * Then next_gen items will be non-decreasing order of bov_tn */
				if (NULL == rctl->jctl_head)
				{
					assert(NULL == rctl->jctl);
					rctl->jctl = rctl->jctl_head = jctl;
				} else
				{
					temp_jctl = jctl;
					jctl = rctl->jctl_head;
					if (temp_jctl->jfh->bov_tn < jctl->jfh->bov_tn ||
						(temp_jctl->jfh->bov_tn == jctl->jfh->bov_tn &&
						temp_jctl->jfh->eov_tn < jctl->jfh->eov_tn))
					{
						assert(NULL == jctl->prev_gen);
						temp_jctl->prev_gen = NULL;
						temp_jctl->next_gen = jctl;
						jctl->prev_gen = temp_jctl;
						rctl->jctl_head = temp_jctl;
					} else
					{
						while (NULL != jctl->next_gen &&
							((jctl->next_gen->jfh->bov_tn < temp_jctl->jfh->bov_tn) ||
							(jctl->next_gen->jfh->bov_tn == temp_jctl->jfh->bov_tn &&
							jctl->next_gen->jfh->eov_tn < temp_jctl->jfh->eov_tn)))
							jctl = jctl->next_gen ;
						temp_jctl->next_gen = jctl->next_gen;
						temp_jctl->prev_gen = jctl;
						if (NULL != jctl->next_gen)
							jctl->next_gen->prev_gen = temp_jctl;
						jctl->next_gen = temp_jctl;
					}
			       }
			}   /* mur_options.forward */
		} /* for jnlno */
	}
	/* If not all regions of a global directory are processed, we shrink mur_ctl array and conserve space.
	 * It is specially needed for later code */
	murgbl.reg_total = murgbl.reg_full_total;
	for (regno = 0; regno < murgbl.reg_total; regno++)
	{
		if (NULL == mur_ctl[regno].jctl)
		{
			for (--murgbl.reg_total; murgbl.reg_total > regno; murgbl.reg_total--)
			{
				if (NULL != mur_ctl[murgbl.reg_total].jctl)
				{
					tmp_rctl = mur_ctl[regno];
					mur_ctl[regno] = mur_ctl[murgbl.reg_total];
					mur_ctl[murgbl.reg_total] = tmp_rctl;
					break;
				}
			}
		}
	}
	assert(murgbl.reg_full_total == max_reg_total);
	if (!mur_options.rollback && murgbl.reg_total < murgbl.reg_full_total)
		gtm_putmsg(VARLSTCNT (1) ERR_NOTALLJNLEN);
	else if (mur_options.rollback && murgbl.reg_total < murgbl.reg_full_total)
		gtm_putmsg(VARLSTCNT (1) ERR_NOTALLREPLON);
	if (0 == murgbl.reg_total)
		return FALSE;
	/* From this point consider only regions with journals to be processed (murgbl.reg_total)
	 * However mur_close_files will close all regions opened (murgbl.reg_full_total) */
	if (mur_options.forward)
	{
		for (mur_regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++, mur_regno++)
		{
			jctl = rctl->jctl_head;
			if (mur_options.update)
			{
				csd = rctl->csd;
				if (mur_options.chain)
				{	/* User might have not specified journal file starting tn matching database curr_tn.
					 * So try to open previous generation journal files and add to linked list */
					while (jctl->jfh->bov_tn > csd->trans_hist.curr_tn)
					{
						if (0 == jctl->jfh->prev_jnl_file_name_length)
						{
							gtm_putmsg(VARLSTCNT(8) ERR_JNLDBTNNOMATCH, 6,
								jctl->jnl_fn_len, jctl->jnl_fn, &jctl->jfh->bov_tn,
								DB_LEN_STR(rctl->gd), &csd->trans_hist.curr_tn);
							gtm_putmsg(VARLSTCNT(4) ERR_NOPREVLINK, 2,
									jctl->jnl_fn_len, jctl->jnl_fn);
							return FALSE;
						} else
						{
							rctl->jctl = mur_jctl = jctl; /* To satisfy a good assert */
							if (!mur_insert_prev())
								return FALSE;
							jctl = mur_jctl;
						}
					}
				}
				if (jctl->jfh->bov_tn != csd->trans_hist.curr_tn && !mur_options.notncheck)
				{
					gtm_putmsg(VARLSTCNT(8) ERR_JNLDBTNNOMATCH, 6, jctl->jnl_fn_len, jctl->jnl_fn,
						&jctl->jfh->bov_tn, DB_LEN_STR(rctl->gd), &csd->trans_hist.curr_tn);
					return FALSE;
				}
			} /* if mur_options.update */
			while (NULL != jctl->next_gen) /* Check for continuity */
			{
				if (!mur_options.notncheck && (jctl->next_gen->jfh->bov_tn != jctl->jfh->eov_tn))
				{
					gtm_putmsg(VARLSTCNT(8) ERR_JNLTNOUTOFSEQ, 6,
						&jctl->jfh->eov_tn, jctl->jnl_fn_len, jctl->jnl_fn,
						&jctl->next_gen->jfh->bov_tn, jctl->next_gen->jnl_fn_len, jctl->next_gen->jnl_fn);
					return FALSE;
				}
				jctl = jctl->next_gen;
			}
			rctl->jctl = jctl;      /* latest in linked list */
		} /* end for */
	}
	return TRUE;
}
