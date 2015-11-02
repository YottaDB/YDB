/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "stp_parms.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "error.h"
#include "mupint.h"
#include "cli.h"
#include "iosp.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "mu_int_maps.h"
#include "util.h"
#include "mupipbckup.h"
#include "dbfilop.h"
#include "targ_alloc.h"
#include "mupip_exit.h"
#ifdef UNIX
#include "ipcrmid.h"
#include "ftok_sems.h"
#endif
#include "mu_getlst.h"
#include "mu_outofband_setup.h"
#include "mupip_integ.h"
#include "gtmmsg.h"
#include "collseq.h"

#define DUMMY_GLOBAL_VARIABLE   "%D%DUMMY_VARIABLE"
#define DUMMY_GLOBAL_VARIABLE_LEN sizeof(DUMMY_GLOBAL_VARIABLE)
#define MAX_UTIL_LEN 80
#define APPROX_ALL_ERRORS 1000000
#define DEFAULT_ERR_LIMIT 10
#define DEFAULT_ADJACENCY 10
#define PERCENT_FACTOR 100
#define PERCENT_DECIMAL_SCALE 100000
#define PERCENT_SCALE_FACTOR 1000
#define LEAVE_BLOCKS_ALONE 0x0FFFFFFFFUL

#define TEXT1 " is incorrect, should be "
#define TEXT2 "!/Largest transaction number found in database was "
#define TEXT3 "Current transaction number is                    "


/* The QWPERCENTCALC calculates the percent used compatibly with the code fragment below.
 * The size is no longer int_size but is now a qw_num, so the QW macros are now used to
 * do the calculation.
 *
 * leftpt  = (int_size * PERCENT_FACTOR) / (int_blks * blk_size);
 * rightpt = (int_size * PERCENT_DECIMAL_SCALE) / (int_blks * blk_size) - (leftpt * PERCENT_SCALE_FACTOR);
 */

#define QWPERCENTCALC(lpt, rpt, qwint_size, int_blks, blk_size) 	\
{									\
	qw_num	tmp;							\
	size_t	rem; 						\
									\
	if (int_blks)							\
	{								\
		QWMULBYDW(tmp, (qwint_size), PERCENT_DECIMAL_SCALE);	\
		QWDIVIDEBYDW(tmp, (int_blks), tmp, rem);		\
		QWDIVIDEBYDW(tmp, (blk_size), tmp, rem);		\
		QWDIVIDEBYDW(tmp, PERCENT_SCALE_FACTOR, tmp, rpt);	\
		DWASSIGNQW((lpt), tmp);					\
	} else								\
	{								\
		(lpt) = 0;						\
		(rpt) = 0;						\
	}								\
}

GBLDEF unsigned char		mu_int_root_level;
GBLDEF int4			mu_int_adj[MAX_BT_DEPTH + 1];
GBLDEF uint4			mu_int_errknt;
GBLDEF uint4			mu_int_blks[MAX_BT_DEPTH + 1];
GBLDEF uint4			mu_int_offset[MAX_BT_DEPTH + 1];
GBLDEF uint4			mu_int_recs[MAX_BT_DEPTH + 1];
GBLDEF qw_num			mu_int_size[MAX_BT_DEPTH + 1];
GBLDEF int			disp_map_errors;
GBLDEF int			disp_maxkey_errors;
GBLDEF int			disp_trans_errors;
GBLDEF int			maxkey_errors = 0;
GBLDEF int			muint_adj;
GBLDEF int			mu_int_plen;
GBLDEF int			mu_map_errs;
GBLDEF int			trans_errors = 0;
GBLDEF boolean_t		block = FALSE;
GBLDEF boolean_t		muint_fast = FALSE;
GBLDEF boolean_t		master_dir;
GBLDEF boolean_t		muint_key = FALSE;
GBLDEF boolean_t		muint_subsc = FALSE;
GBLDEF boolean_t		mu_int_err_ranges;
GBLDEF boolean_t		tn_reset_specified;	/* use this to avoid recomputing cli_present("TN_RESET") in the loop */
GBLDEF boolean_t		tn_reset_this_reg;
GBLDEF block_id			mu_int_adj_prev[MAX_BT_DEPTH + 1];
GBLDEF block_id			mu_int_path[MAX_BT_DEPTH + 1];
GBLDEF global_list		*trees_tail;
GBLDEF global_list		*trees;
GBLDEF sgmnt_data		mu_int_data;
GBLDEF trans_num		largest_tn;
GBLDEF int4			mu_int_blks_to_upgrd;

GBLREF bool			mu_ctrly_occurred;
GBLREF bool			mu_ctrlc_occurred;
GBLREF bool			error_mupip;
GBLREF short			crash_count;
GBLREF gd_region		*gv_cur_region;
GBLREF gv_namehead		*gv_target;
GBLREF gv_key			*gv_altkey;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF tp_region		*grlist;
GBLREF bool			region;

void mupip_integ(void)
{
	boolean_t		full;
	boolean_t		update_filehdr, update_header_tn;
	char			*temp, util_buff[MAX_UTIL_LEN];
	unsigned char		dummy;
	unsigned char		key_buff[2048];
	short			iosb[4];
	unsigned short		keylen;
	unsigned int		blocks_free = LEAVE_BLOCKS_ALONE;
	int			idx, leftpt, rightpt, total_errors, util_len;
	uint4			cli_status;
	block_id		mu_index_adj, mu_data_adj;
	uint4			prev_errknt, mu_dir_blks, mu_dir_recs, mu_data_blks, mu_data_recs, mu_index_blks, mu_index_recs;
	qw_num			mu_dir_size, mu_index_size, mu_data_size;
	block_id		dir_root;
	tp_region		*rptr;
	file_control		*fc;
	boolean_t		retvalue_mu_int_reg;

	error_def(ERR_CTRLY);
	error_def(ERR_CTRLC);
	error_def(ERR_DBRDONLY);
	error_def(ERR_INTEGERRS);
	error_def(ERR_MUPCLIERR);
	error_def(ERR_MUNOACTION);
	error_def(ERR_MUNOFINISH);
	error_def(ERR_DBRBNNEG);
	error_def(ERR_DBRBNTOOLRG);
	error_def(ERR_DBRBNLBMN);
	error_def(ERR_DBNOREGION);
	error_def(ERR_DBTNRESETINC);
	error_def(ERR_DBTNRESET);
	error_def(ERR_DBTNLTCTN);
	error_def(ERR_DBBTUWRNG);
	error_def(ERR_DBBTUFIXED);

	error_mupip = FALSE;
	if (NULL == gv_target)
		gv_target = (gv_namehead *)targ_alloc(DUMMY_GLOBAL_VARIABLE_LEN, NULL);
 	gv_altkey = (gv_key *)malloc(sizeof(gv_key) + MAX_KEY_SZ - 1);
	if (CLI_PRESENT == (cli_status = cli_present("MAXKEYSIZE")))
	{
		assert(sizeof(disp_maxkey_errors) == sizeof(int4));
		if (0 == cli_get_int("MAXKEYSIZE", (int4 *)&disp_maxkey_errors))
			mupip_exit(ERR_MUPCLIERR);
		if (disp_maxkey_errors < 1)
			disp_maxkey_errors = 1;
	} else  if (CLI_NEGATED == cli_status)
		disp_maxkey_errors = APPROX_ALL_ERRORS;
	else
		disp_maxkey_errors = DEFAULT_ERR_LIMIT;
	if (CLI_PRESENT == (cli_status = cli_present("TRANSACTION")))
	{
		assert(sizeof(disp_trans_errors) == sizeof(int4));
		if (0 == cli_get_int("TRANSACTION", (int4 *)&disp_trans_errors))
			mupip_exit(ERR_MUPCLIERR);
		if (disp_trans_errors < 1)
			disp_trans_errors = 1;
	} else  if (CLI_NEGATED == cli_status)
		disp_trans_errors = APPROX_ALL_ERRORS;
	else
		disp_trans_errors = DEFAULT_ERR_LIMIT;
	if (CLI_PRESENT == (cli_status = cli_present("MAP")))
	{
		assert(sizeof(disp_map_errors) == sizeof(int4));
		if (0 == cli_get_int("MAP", (int4 *)&disp_map_errors))
			mupip_exit(ERR_MUPCLIERR);
		if (disp_map_errors < 1)
			disp_map_errors = 1;
	} else  if (CLI_NEGATED == cli_status)
		disp_map_errors = APPROX_ALL_ERRORS;
	else
		disp_map_errors = DEFAULT_ERR_LIMIT;
	if (CLI_PRESENT == cli_present("ADJACENCY"))
	{
		assert(sizeof(muint_adj) == sizeof(int4));
		if (0 == cli_get_int("ADJACENCY", (int4 *)&muint_adj))
			mupip_exit(ERR_MUPCLIERR);
	} else
		muint_adj = DEFAULT_ADJACENCY;
	if (CLI_PRESENT == cli_present("BRIEF"))
		full = FALSE;
	else  if (CLI_PRESENT == cli_present("FULL"))
		full = TRUE;
	else
		full = FALSE;
	if (CLI_PRESENT == cli_present("FAST"))
		muint_fast = TRUE;
	else
		muint_fast = FALSE;
	if (CLI_PRESENT == cli_present("REGION"))
	{
		gvinit();
		region = TRUE;
		mu_getlst("WHAT", sizeof(tp_region));
                if (!grlist)
                {
			error_mupip = TRUE;
			mu_int_errknt++;
			mu_int_err(ERR_DBNOREGION, 0, 0, 0, 0, 0, 0, 0);
                        mupip_exit(ERR_MUNOACTION);
                }
		rptr = grlist;
	}
	if (CLI_PRESENT == cli_present("SUBSCRIPT"))
	{
		keylen = sizeof(key_buff);
		if (0 == cli_get_str("SUBSCRIPT", (char *)key_buff, &keylen))
			mupip_exit(ERR_MUPCLIERR);
		if (FALSE == mu_int_getkey(key_buff, keylen))
			mupip_exit(ERR_MUPCLIERR);
		if (muint_key)
			disp_map_errors = 0;
	}
	tn_reset_specified = (CLI_PRESENT == cli_present("TN_RESET"));
	mu_outofband_setup();
#ifdef UNIX
	ESTABLISH(mu_int_ch);
#endif
	if (region)
		ESTABLISH(mu_freeze_ch);
	for (total_errors = mu_int_errknt = 0;  ;  total_errors += mu_int_errknt, mu_int_errknt = 0)
	{
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
			break;
		if (region)
		{
			if ((NULL == rptr) || (!mupfndfil(rptr->reg, NULL)))
			{
				mu_int_errknt++;
				rptr = rptr->fPtr;
				if (NULL == rptr)
					break;
				continue;
			}
		}
		QWASSIGNDW(mu_dir_size, 0);
		QWASSIGNDW(mu_index_size, 0);
		QWASSIGNDW(mu_data_size, 0);
		mu_index_adj = mu_data_adj = 0;
		mu_dir_blks = mu_dir_recs = 0;
		mu_data_blks = mu_data_recs = 0;
		mu_index_blks = mu_index_recs = 0;
		mu_int_err_ranges = (CLI_NEGATED != cli_present("KEYRANGES"));
		mu_int_root_level = (unsigned char)-1;
		mu_map_errs = 0, prev_errknt = 0, largest_tn = 0;
		mu_int_blks_to_upgrd = 0;
		for (idx = 0;  idx <= MAX_BT_DEPTH;  idx++)
		{
			QWASSIGNDW(mu_int_size[idx], 0);
			mu_int_blks[idx] = mu_int_recs[idx] = 0;
		}
		mu_int_path[0] = 0;
		mu_int_offset[0] = 0;
		mu_int_plen = 1;
		memset(mu_int_adj, 0, sizeof(mu_int_adj));
		if (region)
		{
			util_out_print("!/!/Integ of region !AD", TRUE, REG_LEN_STR(rptr->reg));
			mu_int_reg(rptr->reg, &retvalue_mu_int_reg);
			if (TRUE != retvalue_mu_int_reg)
			{
				rptr = rptr->fPtr;
				if (NULL == rptr)
					break;
				continue;
			}
		} else  if (FALSE == mu_int_init())
			mupip_exit(ERR_INTEGERRS);
		trees_tail = trees = (global_list *)malloc(sizeof(global_list));
		memset(trees, 0, sizeof(global_list));
		trees->root = dir_root = get_dir_root();
		master_dir = TRUE;
		trees_tail->nct = 0;
		trees_tail->act = 0;
		trees_tail->ver = 0;
		tn_reset_this_reg = update_header_tn = FALSE;
		if (tn_reset_specified)
		{
			if (gv_cur_region->read_only)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, gv_cur_region->dyn.addr->fname_len,
					gv_cur_region->dyn.addr->fname);
				mu_int_errknt++;
				mu_int_err(ERR_DBTNRESET, 0, 0, 0, 0, 0, 0, 0);
				mu_int_errknt-=2;
				/* is this error supposed to update error count, or leave it ( then mu_int_errknt-- instead)*/
				mu_int_plen++;  /* continuing, so compensate for mu_int_err decrement */
			} else
				tn_reset_this_reg = update_header_tn = TRUE;
		}
		if (CLI_PRESENT == cli_present("BLOCK"))
		{
			if (0 == cli_get_hex("BLOCK", (uint4 *)&trees->root))
			{
				if (region)
				{
					region_freeze(rptr->reg, FALSE, FALSE);
					if (!rptr->reg->read_only)
					{
						fc = rptr->reg->dyn.addr->file_cntl;
						fc->op = FC_WRITE;
						fc->op_buff = (unsigned char *)FILE_INFO(rptr->reg)->s_addrs.hdr;
						fc->op_len = ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);
						fc->op_pos = 1;
						dbfilop(fc);
					}
				}
				mupip_exit(SS_NORMAL);
			}
			master_dir = FALSE;
			block = TRUE;
			disp_map_errors = 0;
		}
		for (trees->link = 0;  ;  master_dir = FALSE, temp = (char*)trees,  trees = trees->link,  free(temp))
		{
			if (mu_ctrly_occurred || mu_ctrlc_occurred)
			{
				if (region)
				{
					region_freeze(rptr->reg, FALSE, FALSE);
					if (!rptr->reg->read_only)
					{
						fc = gv_cur_region->dyn.addr->file_cntl;
						fc->op = FC_WRITE;
						fc->op_buff = (unsigned char *)FILE_INFO(rptr->reg)->s_addrs.hdr;
						fc->op_len = ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);
						fc->op_pos = 1;
						dbfilop(fc);
					}
				}
				gtm_putmsg(VARLSTCNT(1) mu_ctrly_occurred ? ERR_CTRLY : ERR_CTRLC);
				mupip_exit(ERR_MUNOFINISH);
			}
			if (!trees)
				break;
			mu_int_path[0] = trees->root;
			if (trees->root < 0)
			{
				mu_int_err(ERR_DBRBNNEG, 0, 0, 0, 0, 0, 0, mu_int_root_level);
				continue;
			}
			if (trees->root >= mu_int_data.trans_hist.total_blks)
			{
				mu_int_err(ERR_DBRBNTOOLRG, 0, 0, 0, 0, 0, 0,
					mu_int_root_level);
				continue;
			}
			if (0 == (trees->root % mu_int_data.bplmap))
			{
				mu_int_err(ERR_DBRBNLBMN, 0, 0, 0, 0, 0, 0, mu_int_root_level);
				continue;
			}
			mu_int_plen = 0;
			memset(mu_int_adj_prev, 0, sizeof(mu_int_adj_prev));
 			gv_target->nct = trees->nct;
 			gv_target->act = trees->act;
 			gv_target->ver = trees->ver;
 			gv_altkey->prev = 0;
 			gv_altkey->top = MAX_KEY_SZ;
 			gv_altkey->end = strlen(trees->key);
 			memcpy(gv_altkey->base, trees->key, gv_altkey->end);
 			gv_altkey->base[gv_altkey->end++] = '\0';
 			gv_altkey->base[gv_altkey->end] = '\0';
 			if (gv_target->act)
 				act_in_gvt();
			if (mu_int_blk(trees->root, MAX_BT_DEPTH, TRUE, gv_altkey->base, gv_altkey->end, &dummy, 0, 0))
			{
				if (full)
				{
					if (trees->root == dir_root)
						util_out_print("!/Directory tree", TRUE);
					else
						util_out_print("!/Global variable ^!AD", TRUE, LEN_AND_STR(trees->key));
					if (mu_int_errknt > prev_errknt)
					{
						if (trees->root == dir_root)
							util_out_print("Total error count for directory tree:   !UL",
									TRUE, mu_int_errknt - prev_errknt);
						else
							util_out_print("Total error count for global !AD:	!UL.",
								TRUE, LEN_AND_STR(trees->key), mu_int_errknt - prev_errknt);
						prev_errknt = mu_int_errknt;
					}
					util_out_print("Level          Blocks         Records          % Used      Adjacent", TRUE);
					for (idx = mu_int_root_level;  idx >= 0;  idx--)
					{
						if ((0 == idx) && muint_fast && (trees->root != dir_root))
						util_out_print("!5UL    !12UL              NA              NA            NA", TRUE,
								idx, mu_int_blks[idx]);
						else
						{
							QWPERCENTCALC(leftpt, rightpt, mu_int_size[idx], mu_int_blks[idx],
													mu_int_data.blk_size);
							if (trees->root != dir_root)
							{
								util_out_print("!5UL    !12UL    !12UL    !8UL.!3ZL  !12UL",
									TRUE, idx, mu_int_blks[idx], mu_int_recs[idx],
									leftpt, rightpt, mu_int_adj[idx]);
							} else
								util_out_print("!5UL    !12UL    !12UL    !8UL.!3ZL           NA",
									TRUE, idx, mu_int_blks[idx], mu_int_recs[idx],
									leftpt, rightpt);
						}
					}
				}
				if (dir_root == trees->root)
				{
					for (idx = mu_int_root_level;  idx >= 0;  idx--)
					{
						mu_dir_blks += mu_int_blks[idx];
						QWINCRBY(mu_dir_size, mu_int_size[idx]);
						mu_dir_recs += mu_int_recs[idx];
						QWASSIGNDW(mu_int_size[idx], 0);
						mu_int_adj[0] = mu_int_blks[idx] = mu_int_recs[idx] = 0;
					}
				} else
				{
					for (idx = mu_int_root_level;  idx > 0;  idx--)
					{
						mu_index_blks += mu_int_blks[idx];
						QWINCRBY(mu_index_size, mu_int_size[idx]);
						mu_index_recs += mu_int_recs[idx];
						mu_index_adj += mu_int_adj[idx];
						QWASSIGNDW(mu_int_size[idx], 0);
						mu_int_adj[idx] = mu_int_blks[idx] = mu_int_recs[idx] = 0;
					}
					mu_data_blks += mu_int_blks[0];
					mu_data_adj += mu_int_adj[0];
					QWINCRBY(mu_data_size, mu_int_size[0]);
					mu_data_recs += mu_int_recs[0];
					QWASSIGNDW(mu_int_size[0], 0);
					mu_int_adj[0] = mu_int_blks[0] = mu_int_recs[0] = 0;
				}
			} else  if (update_header_tn)
			{
				update_header_tn = FALSE;
				mu_int_err(ERR_DBTNRESETINC, 0, 0, 0, 0, 0, 0, 0);
				mu_int_plen++;  /* continuing, so compensate for mu_int_err decrement */
				mu_int_errknt--; /* if this error is not supposed to increment the error count */
			}
		}
		if ((FALSE == block) && (FALSE == muint_key))
		{
			mu_int_maps();
			if (! mu_int_errknt)
			{
				blocks_free = mu_int_data.trans_hist.total_blks -
					(mu_int_data.trans_hist.total_blks + mu_int_data.bplmap - 1) / mu_int_data.bplmap -
					mu_data_blks - mu_index_blks - mu_dir_blks;
				if ((region ? cs_addrs->hdr->trans_hist.free_blocks : mu_int_data.trans_hist.free_blocks)
					!= blocks_free)
				{
					if (gv_cur_region->read_only)
						mu_int_errknt++;
					util_len = sizeof("!/Free blocks counter in file header:  ") - 1;
					memcpy(util_buff, "!/Free blocks counter in file header:  ", util_len);
					util_len += i2hex_nofill(region ? cs_addrs->hdr->trans_hist.free_blocks :
							mu_int_data.trans_hist.free_blocks, (uchar_ptr_t)&util_buff[util_len], 8);
					MEMCPY_LIT(&util_buff[util_len], TEXT1);
					util_len += sizeof(TEXT1) - 1;
					util_len += i2hex_nofill(blocks_free, (uchar_ptr_t)&util_buff[util_len], 8);
					util_buff[util_len] = 0;
					util_out_print(util_buff, TRUE);
				} else
					blocks_free = LEAVE_BLOCKS_ALONE;
			}
			if (!muint_fast &&
				(mu_int_blks_to_upgrd != (region ? cs_addrs->hdr->blks_to_upgrd : mu_int_data.blks_to_upgrd)))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_DBBTUWRNG, 2, mu_int_blks_to_upgrd,
					region ? cs_addrs->hdr->blks_to_upgrd : mu_int_data.blks_to_upgrd);
				if (gv_cur_region->read_only || mu_int_errknt)
					mu_int_errknt++;
				else
					gtm_putmsg(VARLSTCNT(1) ERR_DBBTUFIXED);
			}
			if ((0 != mu_int_data.kill_in_prog) && (!mu_map_errs) && !region && !gv_cur_region->read_only)
			{
				assert(mu_int_errknt > 0);
				mu_int_errknt--;
			}
		}
		if (muint_fast)
		{
			if (mu_int_errknt)
				util_out_print("!/Total error count from fast integ:		!UL.", TRUE, mu_int_errknt);
			else
				util_out_print("!/No errors detected by fast integ.", TRUE);
		} else
		{
			if (mu_int_errknt)
				util_out_print("!/Total error count from integ:		!UL.", TRUE, mu_int_errknt);
			else
				util_out_print("!/No errors detected by integ.", TRUE);
		}
		util_out_print("!/Type           Blocks         Records          % Used      Adjacent!/", TRUE);
		QWPERCENTCALC(leftpt, rightpt, mu_dir_size, mu_dir_blks, mu_int_data.blk_size);
		util_out_print("Directory    !8UL    !12UL    !8UL.!3ZL            NA", TRUE, mu_dir_blks, mu_dir_recs,
					leftpt, rightpt);
		QWPERCENTCALC(leftpt, rightpt, mu_index_size, mu_index_blks, mu_int_data.blk_size);
		util_out_print("Index    !12UL    !12UL    !8UL.!3ZL  !12UL", TRUE, mu_index_blks, mu_index_recs, leftpt, rightpt,
			mu_index_adj);
		if (muint_fast)
			util_out_print("Data     !12UL              NA              NA            NA", TRUE, mu_data_blks);
		else
		{
			QWPERCENTCALC(leftpt, rightpt, mu_data_size, mu_data_blks, mu_int_data.blk_size);
			util_out_print("Data     !12UL    !12UL    !8UL.!3ZL  !12UL", TRUE, mu_data_blks, mu_data_recs,
					leftpt, rightpt, mu_data_adj);
		}
		if ((FALSE == block) && (FALSE == muint_key))
			util_out_print("Free     !12UL              NA              NA            NA", TRUE,
				mu_int_data.trans_hist.total_blks -
				(mu_int_data.trans_hist.total_blks + mu_int_data.bplmap - 1) / mu_int_data.bplmap -
				mu_data_blks - mu_index_blks - mu_dir_blks);
		if (muint_fast)
		{
			util_out_print("Total    !12UL              NA              NA  !12UL", TRUE,
				mu_int_data.trans_hist.total_blks -
				(mu_int_data.trans_hist.total_blks + mu_int_data.bplmap - 1) / mu_int_data.bplmap,
				mu_data_adj + mu_index_adj);
		} else
		{
			util_out_print("Total    !12UL    !12UL              NA  !12UL", TRUE,
				mu_int_data.trans_hist.total_blks -
				(mu_int_data.trans_hist.total_blks + mu_int_data.bplmap - 1) / mu_int_data.bplmap,
				mu_dir_recs + mu_index_recs + mu_data_recs, mu_data_adj + mu_index_adj);
		}
		if (largest_tn)
		{
			mu_int_err(ERR_DBTNLTCTN, 0, 0, 0, 0, 0, 0, 0);
			mu_int_plen--;
			mu_int_errknt--;
			if (trans_errors > disp_trans_errors)
			{
				util_out_print("Maximum number of transaction number errors to display:  !UL, was exceeded",
						TRUE, disp_trans_errors);
				util_out_print("!UL transaction number errors encountered.", TRUE, trans_errors);
			}
			MEMCPY_LIT(util_buff, TEXT2);
			util_len = sizeof(TEXT2) - 1;
			util_len += i2hexl_nofill(largest_tn, (uchar_ptr_t)&util_buff[util_len], 16);
			util_buff[util_len] = 0;
			util_out_print(util_buff, TRUE);
			MEMCPY_LIT(util_buff, TEXT3);
			util_len = sizeof(TEXT3) - 1;
			util_len += i2hexl_nofill(mu_int_data.trans_hist.curr_tn, (uchar_ptr_t)&util_buff[util_len], 16);
			util_buff[util_len] = 0;
			util_out_print(util_buff, TRUE);
		}
		if (maxkey_errors > disp_maxkey_errors)
		{
			util_out_print("Maximum number of keys too large errors to display:  !UL, was exceeded",
					TRUE, disp_maxkey_errors);
			util_out_print("!UL keys too large errors encountered.", TRUE, maxkey_errors);
		}
		if (region)
		{
			if (!gv_cur_region->read_only)
			{
				if (LEAVE_BLOCKS_ALONE != blocks_free)
					cs_addrs->hdr->trans_hist.free_blocks = blocks_free;	/* Not if "online" integ */
				if (!mu_int_errknt && !muint_fast)
				{
					if (mu_int_blks_to_upgrd != cs_addrs->hdr->blks_to_upgrd)
						cs_addrs->hdr->blks_to_upgrd = mu_int_blks_to_upgrd;
				}
				region_freeze(gv_cur_region, FALSE, FALSE);
				fc = gv_cur_region->dyn.addr->file_cntl;
				fc->op = FC_WRITE;
				fc->op_buff = (unsigned char *)FILE_INFO(gv_cur_region)->s_addrs.hdr;
				fc->op_len = ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);
				fc->op_pos = 1;
				dbfilop(fc);
			} else
				region_freeze(gv_cur_region, FALSE, FALSE);
			rptr = rptr->fPtr;
			if (NULL == rptr)
				break;
		} else  if (!gv_cur_region->read_only)
		{
			update_filehdr = FALSE;
			if ((FALSE == block) && (FALSE == muint_key))
			{
				if ((0 == mu_map_errs) && (0 != mu_int_data.kill_in_prog))
				{
					mu_int_data.kill_in_prog = 0;
					update_filehdr = TRUE;
				}
				if ((LEAVE_BLOCKS_ALONE != blocks_free) && (mu_int_data.trans_hist.free_blocks != blocks_free))
				{	/* this update should NOT be made if other updates are permitted during [online] integ */
					mu_int_data.trans_hist.free_blocks = blocks_free;
					update_filehdr = TRUE;
				}
				if (!mu_int_errknt && !muint_fast)
				{
					if (mu_int_blks_to_upgrd != mu_int_data.blks_to_upgrd)
					{
						mu_int_data.blks_to_upgrd = mu_int_blks_to_upgrd;
						update_filehdr = TRUE;
					}
				}
			}
			if (update_header_tn)
			{
				mu_int_data.trans_hist.early_tn = mu_int_data.trans_hist.header_open_tn = 1;
				mu_int_data.trans_hist.curr_tn = 0;
				/* curr_tn = 0 + 1 is done (instead of = 1) so as to use INCREMENT_CURR_TN macro.
				 * this way all places that update db curr_tn are easily obtained by searching for the macro */
				INCREMENT_CURR_TN(&mu_int_data);
				update_filehdr = TRUE;
			}
			if (FALSE != update_filehdr)
			{
				fc = gv_cur_region->dyn.addr->file_cntl;
				fc->op = FC_WRITE;
				fc->op_buff = (unsigned char *)&mu_int_data;
				fc->op_len = ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);
				fc->op_pos = 1;
				dbfilop(fc);
				fc->op = FC_CLOSE;
				dbfilop(fc);
			}
			break;
		} else
			break;
	}
	if (!region)
	{
		REVERT;
#ifdef UNIX
		db_ipcs_reset(gv_cur_region, FALSE);
		REVERT;
#endif
	}
	total_errors += mu_int_errknt;
	if (error_mupip)
		total_errors++;
	if (mu_ctrly_occurred || mu_ctrlc_occurred)
	{
		gtm_putmsg(VARLSTCNT(1) mu_ctrly_occurred ? ERR_CTRLY : ERR_CTRLC);
		mupip_exit(ERR_MUNOFINISH);
	}
	if (0 != total_errors)
		mupip_exit(ERR_INTEGERRS);
	mupip_exit(SS_NORMAL);
}
