/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_ctype.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsdbver.h"
#include "gdsblk.h"
#include "copy.h"
#include "mupint.h"
#include "subscript.h"
#include "spec_type.h"
#include "mmemory.h"
#include "util.h"
#include "gdsbml.h"
#include "gtmmsg.h"
#include "get_spec.h"
#ifdef GTM_TRIGGER
#include <rtnhdr.h>		/* for rtn_tabent in gv_trigger.h */
#include "gv_trigger.h"
#endif

#define NEG_SUB	127
#define NO_SUBSCRIPTS -1
#define MAX_UTIL_SIZE 32
#define MIN_DATA (3 * SIZEOF(char)) /* a non-empty data block rec must have at least one character of key and two of terminator */
#define TEXT2 "Block "
#define TEXT3 " doubly allocated"
#define SPAN_SUBS_LENGTH   5
#define SPAN_START_BYTE 0x02
#define SPAN_BYTE_MAX  	255
#define SPAN_BYTE_MIN	1
#define SPAN_SUBS_OFF	48

GBLDEF unsigned char		muint_temp_buff[MAX_MIDENT_LEN + 1];
GBLREF unsigned char		*mu_int_locals;
GBLREF unsigned char		mu_int_root_level;
GBLREF bool			mu_ctrly_occurred, mu_ctrlc_occurred;
GBLREF boolean_t		block;
GBLREF boolean_t		master_dir;
GBLREF boolean_t		muint_fast;
GBLREF boolean_t		muint_key;
GBLREF boolean_t		muint_subsc;
GBLREF boolean_t		tn_reset_this_reg;
GBLREF int			disp_maxkey_errors;
GBLREF int			disp_trans_errors;
GBLREF int			maxkey_errors;
GBLREF int			muint_adj;
GBLREF int			muint_end_keyend;
GBLREF int			muint_start_keyend;
GBLREF int			mu_int_plen;
GBLREF int			trans_errors;
GBLREF int4			mu_int_adj[];
GBLREF uint4			mu_int_blks[];
GBLREF uint4			mu_int_offset[];
GBLREF uint4			mu_int_recs[];
GBLREF qw_num			mu_int_size[];
GBLREF uint4			mu_int_errknt;
GBLREF block_id			mu_int_adj_prev[];
GBLREF block_id			mu_int_path[];
GBLREF int4			mu_int_blks_to_upgrd;
GBLREF global_list		*trees;
GBLREF global_list		*trees_tail;
GBLREF gv_key			*muint_end_key;
GBLREF gv_key			*muint_start_key;
GBLREF sgmnt_data		mu_int_data;
GBLREF trans_num		largest_tn;
GBLREF span_node_integ		*sndata;

error_def(ERR_DBBADKYNM);
error_def(ERR_DBBADNSUB);
error_def(ERR_DBBADPNTR);
error_def(ERR_DBBDBALLOC);
error_def(ERR_DBBNPNTR);
error_def(ERR_DBBSIZMN);
error_def(ERR_DBBSIZMX);
error_def(ERR_DBCMPBAD);
error_def(ERR_DBCMPNZRO);
error_def(ERR_DBCOMPTOOLRG);
error_def(ERR_DBGTDBMAX);
error_def(ERR_DBINCLVL);
error_def(ERR_DBINVGBL);
error_def(ERR_DBKEYGTIND);
error_def(ERR_DBKEYMN);
error_def(ERR_DBKEYMX);
error_def(ERR_DBKEYORD);
error_def(ERR_DBKGTALLW);
error_def(ERR_DBLRCINVSZ);
error_def(ERR_DBLTSIBL);
error_def(ERR_DBMAXNRSUBS); /* same error as ERR_MAXNRSUBSCRIPTS, but has a string output as well */
error_def(ERR_DBPTRMX);
error_def(ERR_DBPTRNOTPOS);
error_def(ERR_DBRLEVTOOHI);
error_def(ERR_DBRLEVLTONE);
error_def(ERR_DBSTARCMP);
error_def(ERR_DBRSIZMN);
error_def(ERR_DBRSIZMX);
error_def(ERR_DBDATAMX);
error_def(ERR_DBTN);
error_def(ERR_DBTNTOOLG);
error_def(ERR_DBSPANGLOINCMP);
error_def(ERR_DBSPANCHUNKORD);

LITDEF boolean_t mu_int_possub[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

LITDEF boolean_t mu_int_negsub[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

LITDEF boolean_t mu_int_exponent[256] = {
	0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0};


boolean_t mu_int_blk(
		block_id blk,
		char level,
		boolean_t is_root,
		unsigned char *bot_key,
		int bot_len,
		unsigned char *top_key,
		int top_len,
		boolean_t eb_ok)	/* boolean indicating whether an empty block here is ok.  This is true when
					   the parent is a root with only a star key */
{
	typedef struct
	{
		boolean_t	numeric;
		int		index;
	} sub_list;

	unsigned char	buff[MAX_KEY_SZ + 1], old_buff[MAX_KEY_SZ + 1], temp_buff[MAX_MIDENT_LEN + 1], util_buff[MAX_UTIL_SIZE];
	unsigned char	blk_levl, *c1, ch, *ctrlbytes;
	unsigned short	cc, rec_cmpc;
	uchar_ptr_t	c0, c2, c_base, blk_base, blk_top, key_base, ptr, rec_base, rec_top, span_key;
	unsigned short	temp_ushort;
	boolean_t	first_key, is_top, pstar, valid_gbl, hasht_global;
	boolean_t	muint_range_done = FALSE;
	int		blk_size, buff_length, b_index, cmcc, comp_length, key_size, len, name_len,
			num_len, rec_size, s_index, start_index, hdr_len, idx;
	int		tmp_cmpc, tmp_numsubs, max_allowed_key_size;
	block_id	child, root_pointer;
	sub_list	mu_sub_list[MAX_GVSUBSCRIPTS + 1];
	sub_num		check_vals;
	trans_num	blk_tn;
	uchar_ptr_t	subrec_ptr;
	enum db_ver	ondsk_blkver;
	uint4		cnt, span_curr_blk, rval_len, gblsize;
	unsigned short	numsubs;

	mu_int_offset[mu_int_plen] = 0;
	mu_int_path[mu_int_plen++] = blk;
	mu_int_path[mu_int_plen] = 0;
	if (!bml_busy(blk, mu_int_locals)) /* block already marked busy */
	{
		mu_int_err(ERR_DBBDBALLOC, TRUE, TRUE, bot_key, bot_len, top_key, top_len, (unsigned int)(level));
		return FALSE;
	}
	blk_base = mu_int_read(blk, &ondsk_blkver);	/* ondsk_blkver set to GDSV4 or GDSV6 (GDSVCURR) */
	if (!blk_base)
		return FALSE;
	blk_size = (int)((blk_hdr_ptr_t)blk_base)->bsiz;
	if (!muint_fast)
	{
		if (tn_reset_this_reg)
		{
			((blk_hdr_ptr_t)blk_base)->tn = 1;
			mu_int_write(blk, blk_base);
			if (GDSVCURR != mu_int_data.desired_db_format)
				mu_int_blks_to_upgrd++;
		} else if (GDSVCURR != ondsk_blkver)
			mu_int_blks_to_upgrd++;
	}
	/* pstar indicates that the current block is a (root block with only a star key) or not.
		This is passed into mu_int_blk() as eb_ok */
	pstar = (is_root && (SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + SIZEOF(block_id) == blk_size));
	if (blk_size < (SIZEOF(blk_hdr) + (eb_ok ? 0 : (SIZEOF(rec_hdr) + (level ? SIZEOF(block_id) : MIN_DATA)))))
	{
		mu_int_err(ERR_DBBSIZMN, TRUE, TRUE, bot_key, bot_len, top_key, top_len,
				(unsigned int)((blk_hdr_ptr_t)blk_base)->levl);
		free(blk_base);
		return FALSE;
	}
	if (blk_size > mu_int_data.blk_size)
	{
		mu_int_err(ERR_DBBSIZMX, TRUE, TRUE, bot_key, bot_len, top_key, top_len,
			(unsigned int)((blk_hdr_ptr_t)blk_base)->levl);
		free(blk_base);
		return FALSE;
	}
	blk_top = blk_base + blk_size;
	blk_levl = ((blk_hdr_ptr_t)blk_base)->levl;
	if (block)
		mu_int_root_level = level = blk_levl;
	else  if (is_root)
	{
		if (blk_levl >= MAX_BT_DEPTH)
		{
			mu_int_err(ERR_DBRLEVTOOHI, 0, 0, 0, 0, 0, 0, (unsigned int)blk_levl);
			free(blk_base);
			return FALSE;
		}
		if (blk_levl < 1)
		{
			mu_int_err(ERR_DBRLEVLTONE, 0, 0, 0, 0, 0, 0, (unsigned int)blk_levl);
			free(blk_base);
			return FALSE;
		}
		mu_int_root_level = level = blk_levl;
	} else  if (blk_levl != level)
	{
		mu_int_err(ERR_DBINCLVL, TRUE, TRUE, bot_key, bot_len, top_key, top_len, (unsigned int)blk_levl);
		free(blk_base);
		return FALSE;
	}
	if (!master_dir)
	{
		if (mu_int_adj_prev[level] <= blk + muint_adj && mu_int_adj_prev[level] >= blk - muint_adj)
			mu_int_adj[level] += 1;
		mu_int_adj_prev[level] = blk;
	}
	blk_tn = ((blk_hdr_ptr_t)blk_base)->tn;
	if (blk_tn >= mu_int_data.trans_hist.curr_tn)
	{
		if (trans_errors < disp_trans_errors)
		{
			mu_int_err(ERR_DBTNTOOLG, TRUE, TRUE, bot_key, bot_len, top_key, top_len,
					(unsigned int)blk_levl);
			mu_int_plen++;	/* continuing, so compensate for mu_int_err decrement */
			gtm_putmsg(VARLSTCNT(3) ERR_DBTN, 1, &blk_tn);
			trans_errors++;
		} else
		{
			mu_int_errknt++;
			trans_errors++;
		}
		/* Stop searching the sub-tree when TN in block is larger than integ_start_tn for fast_integ. The reason being,
		 * fast_integ skips writing free blocks and level-0 block in GV tree to snapshot file. However, some blocks can be
		 * mistakenly marked free or its level is messed-up as 0. After updating these blocks, thse blocks will have TN
		 * larger than integ_start_tn. In this case, the child tree pointed to by one such updated block may result in
		 * arbitrary error report. Since we already capture the core reason for the integ error, we should not proceed
		 * searching its child tree; otherwise, we will have meaningless report content
		 */
		if (muint_fast)
			return FALSE;
		if (blk_tn > largest_tn)
			largest_tn = blk_tn;
	}
	mu_int_blks[level]++;
	QWINCRBYDW(mu_int_size[level], blk_size);
	first_key = TRUE;
	buff_length = 0;
	comp_length = bot_len;
	is_top = FALSE;
	memcpy(buff, bot_key, bot_len);
	mu_sub_list[0].index = NO_SUBSCRIPTS;
	for (rec_base = blk_base + SIZEOF(blk_hdr);  (rec_base < blk_top) && (FALSE == muint_range_done);
		rec_base = rec_top, comp_length = buff_length)
	{
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
			return FALSE;
		mu_int_recs[level]++;
		GET_USHORT(temp_ushort, &(((rec_hdr_ptr_t)rec_base)->rsiz));
		rec_size = temp_ushort;
		mu_int_offset[mu_int_plen - 1] = (uint4)(rec_base - blk_base);
		if (rec_size <= SIZEOF(rec_hdr))
		{
			mu_int_err(ERR_DBRSIZMN, TRUE, TRUE, buff, comp_length, top_key, top_len, (unsigned int)blk_levl);
			free(blk_base);
			return FALSE;
		}
		if ((rec_size > blk_top - rec_base))
		{
			mu_int_err(ERR_DBRSIZMX, TRUE, TRUE, buff, comp_length, top_key, top_len, (unsigned int)blk_levl);
			free(blk_base);
			return FALSE;
		}
		rec_top = rec_base + rec_size;
		rec_cmpc = EVAL_CMPC((rec_hdr_ptr_t)rec_base);
		if (level && (rec_top == blk_top))
		{
			is_top = TRUE;
			if (SIZEOF(rec_hdr) + SIZEOF(block_id) != rec_size)
			{
				mu_int_err(ERR_DBLRCINVSZ, TRUE, TRUE, buff, comp_length, top_key, top_len, (unsigned int)blk_levl);
				free(blk_base);
				return FALSE;
			}
			if (rec_cmpc)
			{
				mu_int_err(ERR_DBSTARCMP, TRUE, TRUE, buff, comp_length, top_key, top_len, (unsigned int)blk_levl);
				free(blk_base);
				return FALSE;
			}
			ptr = rec_base + SIZEOF(rec_hdr);
		} else
		{
			if (first_key)
			{
				if (rec_cmpc)
				{
					mu_int_err(ERR_DBCMPNZRO, TRUE, TRUE, buff,
						comp_length, top_key, top_len, (unsigned int)blk_levl);
					free(blk_base);
					return FALSE;
				}
			} else  if ((rec_cmpc < name_len) && (FALSE == master_dir))
			{
				mu_int_err(ERR_DBINVGBL, TRUE, TRUE, buff, comp_length, top_key, top_len,
					(unsigned int)blk_levl);
				if (!level)
				{	/* since global names are mixed, the numeric subscript check done later below
					 * (using mu_sub_list) needs to not use the optimization which relies on the assumption
					 * that all keys within a block have the same global name prefix.
					 * reset mu_sub_list[0].index here to ensure the same.
					 */
					mu_sub_list[0].index = NO_SUBSCRIPTS;
				}
				mu_int_plen++;	/* continuing, so compensate for mu_int_err decrement */
			}
			if (rec_cmpc && (short int)rec_cmpc >= buff_length)
			{
				mu_int_err(ERR_DBCOMPTOOLRG, TRUE, TRUE, buff, comp_length, top_key,
					top_len, (unsigned int)blk_levl);
				free(blk_base);
				return FALSE;
			}
			key_base = rec_base + SIZEOF(rec_hdr);
			for (ptr = key_base;  ;)
			{
				if (ptr >= rec_top)
				{
					mu_int_err(ERR_DBKEYMX, TRUE, TRUE, buff, comp_length, top_key, top_len,
							(unsigned int)blk_levl);
					free(blk_base);
					return FALSE;
				}
				if (KEY_DELIMITER == *ptr++)
				{
					if (first_key)
					{
						first_key = FALSE;
						name_len = (int)(ptr - key_base);
						if (!master_dir)
						{	/* If NOT directory tree and this is the first key in the block,
							 * make sure the global name part of the key matches the name
							 * corresponding to the current global variable tree.
							 */
							assert(strlen(trees->key) == trees->keysize);
							if (0 != memcmp(trees->key, key_base, trees->keysize + 1))
							{
								mu_int_err(ERR_DBINVGBL, TRUE, TRUE, bot_key, bot_len,
									top_key, top_len, (unsigned int)blk_levl);
								free(blk_base);
								return FALSE;
							}
						}
					}
					if (KEY_DELIMITER == *ptr++)
						break;
				}
			}
			key_size = (int)(ptr - key_base);
			if (level && (rec_size - SIZEOF(block_id) - SIZEOF(rec_hdr) != key_size))
			{
				mu_int_err(ERR_DBKEYMN, TRUE, TRUE, buff, comp_length, top_key, top_len,
						(unsigned int)blk_levl);
				free(blk_base);
				return FALSE;
			}
			if (key_size + rec_cmpc > MAX_KEY_SZ)
			{	/* We'll allow index keys to be whatever length, so long as they don't exceed MAX_KEY_SZ */
				mu_int_err(ERR_DBKGTALLW, TRUE, TRUE, buff, comp_length, top_key,
						top_len, (unsigned int)blk_levl);
				free(blk_base);
				return FALSE;
			}
			if ((short int)rec_cmpc < buff_length && buff[rec_cmpc] == *key_base)
			{
				mu_int_err(ERR_DBCMPBAD, TRUE, TRUE, buff, comp_length, top_key, top_len,
					(unsigned int)blk_levl);
				free(blk_base);
				return FALSE;
			}
			if (memvcmp(buff + rec_cmpc, comp_length - rec_cmpc, key_base, key_size) >= 0)
			{
				if (3 == mu_int_offset[mu_int_plen - 1])
				{
					mu_int_err(ERR_DBLTSIBL,
							TRUE, TRUE, buff, comp_length, top_key, top_len, (unsigned int)blk_levl);
					mu_int_plen++;	/* continuing, so compensate for mu_int_err decrement */
				} else
				{
					mu_int_err(ERR_DBKEYORD, TRUE, TRUE, buff, comp_length, top_key, top_len,
						(unsigned int)blk_levl);
					free(blk_base);
					return FALSE;
				}
			}
			memcpy(old_buff, buff, comp_length);
			memcpy(buff + rec_cmpc, key_base, key_size);
			buff_length = rec_cmpc + key_size;
			/* Now that we have the uncompressed global variable name, check for global name validity.
			 * Note that it is enough to check the validity on the leaf level directory tree and not
			 * for every block. Invalid global names in non-directory tree blocks will encounter
			 * either DBKEYORD error or DBINVGBL. Below are the rules for validating.
			 * a) The first character should be ALPHA or '%' or '#'
			 * b) If first character is '#' then the following character should be 't' followed by 2 KEY_DELIMITERS
			 * c) If first character is '%' then the following characters should be ALPHANUMERIC and 2 KEY_DELIMITERS
			 */
			if (master_dir && !level)
			{
				hasht_global = FALSE;
				ch = buff[0];
				switch (ch)
				{
#					ifdef GTM_TRIGGER
					case HASHT_GBL_CHAR1:
						hasht_global = valid_gbl = (HASHT_GBL_CHAR2 == buff[1]);
						idx = 2;
						break;
#					endif
					case '%':
					default:
						valid_gbl = VALFIRSTCHAR(ch);
						idx = 1;
						break;
				}
				if (!hasht_global)
				{
					for (; valid_gbl && (idx <= buff_length - 3); idx++)
						valid_gbl = (valid_gbl && VALKEY(buff[idx]));
				}
				valid_gbl = (valid_gbl && (KEY_DELIMITER == buff[idx]) && (KEY_DELIMITER == buff[idx + 1]));
				if (!valid_gbl)
				{
					mu_int_err(ERR_DBBADKYNM, TRUE, TRUE, buff, buff_length, top_key, top_len,
						(unsigned int)blk_levl);
					mu_int_plen++;  /* continuing, so compensate for mu_int_err decrement */
				}
			}
			if (!master_dir)
			{	/* master_directory has no subscripts; block splits don't preserve numeric integrity in index */
				if (muint_subsc)
				{
					if (muint_end_key)
					{
						if (memcmp(buff, muint_end_key->base, muint_end_key->end + 1) > 0)
						{
							if (level)
								muint_range_done = TRUE;
							else
							{
								mu_int_recs[level]--;
								mu_int_plen--;
								free(blk_base);
								return TRUE;
							}
						}
						if (memcmp(buff, muint_start_key->base, muint_start_key->end + 1) < 0)
						{
							mu_int_recs[level]--;
							continue;
						}
					} else
					{
						if (memcmp(buff, muint_start_key->base, muint_start_key->end + 1) > 0)
						{
							if (level)
								muint_range_done = TRUE;
							else
							{
								mu_int_recs[level]--;
								mu_int_plen--;
								free(blk_base);
								return TRUE;
							}
						}
						if (memcmp(buff, muint_start_key->base, muint_start_key->end + 1) < 0)
						{
							mu_int_recs[level]--;
							continue;
						}
					}
				}
				if (!level)
				{
					s_index = 0;
					if (NO_SUBSCRIPTS != mu_sub_list[0].index)
					{
						for (;  (mu_sub_list[s_index].index < (short int)rec_cmpc - 1) &&
								mu_sub_list[s_index].index > 0;)
							if (MAX_GVSUBSCRIPTS <= s_index++)
								break;
						if (s_index)
							s_index--;
					} else		/* scan off key */
					{
						for (b_index = 0;  buff[b_index];  b_index++)
							;
						b_index++;
						mu_sub_list[0].index = b_index;
					}
					b_index = mu_sub_list[s_index].index;
					start_index = s_index;
					while (buff[b_index])
					{
						if (mu_int_exponent[buff[b_index]])
							mu_sub_list[s_index].numeric = TRUE;
						else
							mu_sub_list[s_index].numeric = FALSE;
						mu_sub_list[s_index].index = b_index;
						for (;  buff[b_index];  b_index++)
							;
						b_index++;
						if (MAX_GVSUBSCRIPTS <= s_index++)
							break;
					}
					if (MAX_GVSUBSCRIPTS < s_index)
					{
						mu_int_err(ERR_DBMAXNRSUBS, TRUE, TRUE, buff,
							comp_length, top_key, top_len, (unsigned int)blk_levl);
						mu_int_plen++;	/* continuing, so compensate for mu_int_err decrement */
						break;
					}
					mu_sub_list[s_index].index = 0;
					for (;  (start_index != s_index) && (0 != mu_sub_list[start_index].index);  start_index++)
					{
						if (mu_sub_list[start_index].numeric)
						{
							b_index = mu_sub_list[start_index].index;
							if (buff[b_index] > NEG_SUB)
							{
								b_index++;
								while (buff[b_index])
								{
									memcpy(&check_vals, &buff[b_index], 1);
									if (!mu_int_possub[check_vals.one][check_vals.two])
									{
										mu_int_err(ERR_DBBADNSUB, TRUE, TRUE,
											buff, comp_length, top_key, top_len,
											(unsigned int)blk_levl);
										free(blk_base);
										return FALSE;
									}
									b_index++;
								}
							} else
							{
								b_index++;
								while ((STR_SUB_PREFIX != buff[b_index]) && (0 != buff[b_index]))
								{
									memcpy(&check_vals, &buff[b_index], 1);
									if (!mu_int_negsub[check_vals.one][check_vals.two])
									{
										mu_int_err(ERR_DBBADNSUB, TRUE, TRUE,
											buff, comp_length, top_key, top_len,
											(unsigned int)blk_levl);
										free(blk_base);
										return FALSE;
									}
									b_index++;
								}
								if (STR_SUB_PREFIX != buff[b_index++] || (buff[b_index]))
								{
									mu_int_err(ERR_DBBADNSUB, TRUE, TRUE, buff, comp_length,
											top_key, top_len, (unsigned int)blk_levl);
									free(blk_base);
									return FALSE;
								}
							}
						}
					}
				}
			}
			if (!level && !master_dir)
			{
				rval_len = rec_size - SIZEOF(rec_hdr) - key_size;
				if (mu_int_data.max_rec_size < rval_len)
				{
					mu_int_err(ERR_DBDATAMX, TRUE, TRUE, buff, comp_length,
							top_key, top_len, (unsigned int)blk_levl);
					free(blk_base);
					return FALSE;
				}
				span_key = buff + buff_length - SPAN_SUBS_LENGTH - 1;
				max_allowed_key_size = mu_int_data.max_key_size;
				if ((SPAN_SUBS_LENGTH < key_size + rec_cmpc) && (KEY_DELIMITER == *span_key++)
						&& (SPAN_START_BYTE == *span_key))
				{	/* Hidden subscript detected */
					max_allowed_key_size += 4;
					if (0 == (span_curr_blk = SPAN_GVSUBS2INT((span_subs *)span_key)))
					{	/* First record of spanning node. Get the number blocks. */
						ctrlbytes = key_base + key_size;
						if (rec_top - ctrlbytes == 6)
						{
							GET_NSBCTRL(ctrlbytes, numsubs, gblsize);
						} else
						{
							SSCANF((char *)ctrlbytes, "%d,%d", &tmp_numsubs, &gblsize);
							numsubs = tmp_numsubs;
						}
						sndata->span_tot_blks = numsubs + 1;
						sndata->span_node_sz = gblsize;
						sndata->val_len = 0;
						sndata->sn_type = SPAN_NODE;
						sndata->span_prev_blk = 0;
						sndata->span_blk_cnt = 1;
						sndata->key_len = buff_length;
						sndata->sn_cnt += 1;
						memcpy(sndata->span_node_buf, buff, buff_length);
					} else
					{
						switch (sndata->sn_type)
						{
						case SN_NOT:	/*First block of the node-fragment*/
							sndata->sn_type = SN_CHUNK;
							sndata->span_prev_blk = span_curr_blk;
							sndata->span_blk_cnt = 1;
							sndata->span_frag_off = span_curr_blk;
							sndata->key_len = buff_length;
							/*Spanning node can never have 0 rval*/
							sndata->span_node_sz = 0;
							sndata->val_len = 0;
							sndata->sn_cnt += 1;
							memcpy(sndata->span_node_buf, buff, buff_length);
							break;
						case SPAN_NODE:	/* Already in the spanning node */
							if((sndata->span_prev_blk + 1) == span_curr_blk)
							{	/*Logical continuity of block is present*/
								sndata->span_prev_blk = span_curr_blk;
								sndata->span_blk_cnt = sndata->span_blk_cnt + 1;
								sndata->val_len += rval_len;
								if(sndata->span_blk_cnt == sndata->span_tot_blks)
								{	/* All the blocks of the spanning node are seen */
									sndata->sn_type = SN_NOT;
									sndata->sn_blk_cnt += sndata->span_blk_cnt;
								}
								if (sndata->val_len > sndata->span_node_sz)
								{
									mu_int_err(ERR_DBDATAMX, TRUE, TRUE,
										   sndata->span_node_buf,
										   sndata->key_len, top_key,
										   top_len,
										   (unsigned int)blk_levl);
									free(blk_base);
									return FALSE;
								}
							}
							else { /* ERROR 1: There is discontinuity in the spanning node
							  	* blocks; adjacent spanning block is missing
							  	*/
								mu_int_err(ERR_DBSPANGLOINCMP, TRUE, FALSE,
									   sndata->span_node_buf,
									   sndata->key_len, top_key,
								 	   top_len, (unsigned int)blk_levl);
								sndata->sn_type = SN_NOT;
								sndata->sn_blk_cnt += sndata->span_blk_cnt;
								/* continuing, so compensate for mu_int_err decrement */
								mu_int_plen++;
								maxkey_errors++;
							}
							break;
						case SN_CHUNK:	/* Already in the spanning node fragment */
							if((sndata->span_prev_blk + 1) == span_curr_blk)
							{	/*Logical continuity of block is present*/
								sndata->span_prev_blk = span_curr_blk;
								sndata->span_blk_cnt = sndata->span_blk_cnt + 1;
							}
							else { /* ERROR 2: Unexpected spanning-node-block occurred in
							 	* the middle of spanning-node fragment
							  	*/
								mu_int_err(ERR_DBSPANCHUNKORD, TRUE, FALSE,
									   sndata->span_node_buf,
									   sndata->key_len, top_key,
									   top_len, (unsigned int)blk_levl);
								sndata->sn_type = SN_CHUNK;
								sndata->span_prev_blk = span_curr_blk;
								sndata->span_blk_cnt = 1;
								sndata->span_frag_off = span_curr_blk;
								sndata->key_len = buff_length;
								memcpy(sndata->span_node_buf, buff, buff_length);
								sndata->sn_blk_cnt += sndata->span_blk_cnt;
								/* continuing, so compensate for mu_int_err decrement */
								mu_int_plen++;
								maxkey_errors++;
							}
							break;
						}
					}
				} else if (sndata->sn_type)
				{
					if (SPAN_NODE == sndata->sn_type) /*INCOMPLETE SPANNING NODE*/
					{ /* ERROR 1: There is discontinuity in the spanning node blocks;
					   * adjacent spanning block is missing
					   */
						mu_int_err(ERR_DBSPANGLOINCMP, TRUE, FALSE,
							   sndata->span_node_buf, sndata->key_len, top_key,
							   top_len, (unsigned int)blk_levl);
					} else /*INCOMPLETE SPANNING NODE FRAGMENT*/
					{  /* ERROR 2: Spanning-node-block occurred in the middle
					    * of non-spanning block
					    */
						mu_int_err(ERR_DBSPANCHUNKORD, TRUE, FALSE,
							   sndata->span_node_buf, sndata->key_len, top_key,
							   top_len, (unsigned int)blk_levl);
					}
					sndata->sn_blk_cnt += sndata->span_blk_cnt;
					/* continuing, so compensate for mu_int_err decrement */
					mu_int_plen++;
					maxkey_errors++;
					sndata->sn_type = SN_NOT;
				}
				if (key_size + rec_cmpc > max_allowed_key_size)
				{
					if (maxkey_errors < disp_maxkey_errors)
					{
						mu_int_err(ERR_DBGTDBMAX, TRUE, FALSE, buff, comp_length, top_key,
							top_len, (unsigned int)blk_levl);
						mu_int_plen++;	/* continuing, so compensate for mu_int_err decrement */
						maxkey_errors++;
					} else
					{
						mu_int_errknt++;
						maxkey_errors++;
					}
				}
			}
		}
		if (level)
		{
			GET_LONG(child, ptr);
			if (child < 0)
			{
				mu_int_err(ERR_DBPTRNOTPOS, TRUE, TRUE, buff, comp_length, top_key, top_len,
						(unsigned int)blk_levl);
				free(blk_base);
				return FALSE;
			}
			if (child > mu_int_data.trans_hist.total_blks)
			{
				mu_int_err(ERR_DBPTRMX, TRUE, TRUE, buff, comp_length, top_key,
						top_len, (unsigned int)blk_levl);
				free(blk_base);
				return FALSE;
			}
			if (!(child % mu_int_data.bplmap))
			{
				mu_int_err(ERR_DBBNPNTR, TRUE, TRUE, buff, comp_length, top_key, top_len,
						(unsigned int)blk_levl);
				free(blk_base);
				return FALSE;
			}
			if (!muint_fast || (level > 1) || master_dir)
			{
				if (is_top)
					mu_int_blk(child, level - 1, FALSE, buff, comp_length, top_key, top_len, pstar);
				else
					mu_int_blk(child, level - 1, FALSE, old_buff, comp_length, buff, buff_length, pstar);
			} else
			{
				if (!bml_busy(child, mu_int_locals))
				{
					mu_int_offset[mu_int_plen]=0;
					mu_int_path[mu_int_plen++]=child;
					mu_int_err(ERR_DBBDBALLOC, TRUE, TRUE, old_buff, comp_length, buff, buff_length,
							(unsigned int)((blk_hdr_ptr_t)ptr)->levl);
					mu_int_plen--;
					free(blk_base);
					return FALSE;
				}
				mu_int_blks[0]++;
			}
		} else
		{
			if (master_dir)
			{
				for (c0 = c_base = (uchar_ptr_t)rec_base + SIZEOF(rec_hdr);  *c0;  c0++);
				GET_LONG(root_pointer, ((block_id *)(c0 + 2)));
				if (root_pointer > mu_int_data.trans_hist.total_blks || root_pointer < 2)
				{	/* 0=master map, 1=dir root*/
					mu_int_err(ERR_DBBADPNTR, TRUE, TRUE, buff, comp_length, top_key,
							top_len, (unsigned int)blk_levl);
					mu_int_plen++;	/* continuing, so compensate for mu_int_err decrement */
				}
				c1 = temp_buff;
				if (rec_cmpc)
					for (c2 = muint_temp_buff, cc = 0;  *c2 && (cc < rec_cmpc);  cc++)
						*c1++ = *c2++;
				for (;  c_base < c0;)
					*c1++ = *c_base++;
				*c1 = 0;
				assert(SIZEOF(muint_temp_buff) == SIZEOF(temp_buff));
				memcpy(muint_temp_buff, temp_buff, SIZEOF(temp_buff));
				if (muint_key)
				{
					if (muint_end_key)	/* range */
					{
						len = (int)(c1 - temp_buff + 1);
						if ((0 < memcmp(muint_start_key->base, temp_buff, len < muint_start_keyend ?
									len : muint_start_keyend))
							|| (0 >  memcmp(muint_end_key->base, temp_buff, len < muint_end_keyend ?
									len : muint_end_keyend)))
								continue;
					} else
					{
						if (((muint_start_keyend - 1) != (c1 - temp_buff))
							|| (memcmp(muint_start_key->base, temp_buff, muint_start_keyend)))
								continue;
					}
				}
				trees_tail->link = (global_list *)malloc(SIZEOF(global_list));
				trees_tail = trees_tail->link;
				trees_tail->link = 0;
				trees_tail->root = root_pointer;

				memcpy(trees_tail->path, mu_int_path, SIZEOF(block_id) * (MAX_BT_DEPTH + 1));
				memcpy(trees_tail->offset, mu_int_offset, SIZEOF(uint4) * (MAX_BT_DEPTH + 1));
				assert(SIZEOF(trees_tail->key) == SIZEOF(muint_temp_buff));
				memcpy(trees_tail->key, muint_temp_buff, SIZEOF(muint_temp_buff));
				trees_tail->keysize = STRLEN((char *)muint_temp_buff);
				hdr_len = SIZEOF(rec_hdr) + STRLEN(trees_tail->key) + 2 - rec_cmpc; /* We cannot use
									mid_len() which expects mident_fixed structure */
				/* +2 in the above hdr_len calculation is to take into account
				   two \0's after the end of the key
				*/
				if (rec_size > hdr_len + SIZEOF(block_id))
				{
					subrec_ptr = get_spec((sm_uc_ptr_t)rec_base + hdr_len + SIZEOF(block_id),
									(int)(rec_size - (hdr_len + SIZEOF(block_id))), COLL_SPEC);
					if (subrec_ptr)
					{
						trees_tail->nct = *(subrec_ptr + COLL_NCT_OFFSET);
						trees_tail->act = *(subrec_ptr + COLL_ACT_OFFSET);
						trees_tail->ver = *(subrec_ptr + COLL_VER_OFFSET);
					} else
					{
						trees_tail->nct = 0;
						trees_tail->act = 0;
						trees_tail->ver = 0;
					}
				} else
				{
					trees_tail->nct = 0;
					trees_tail->act = mu_int_data.def_coll;
					trees_tail->ver = mu_int_data.def_coll_ver;
				}
			}
		}
	}
	if (top_len)
	{
		if ((cmcc = memvcmp(buff, comp_length, top_key, top_len)) >= 0)
		{
			if ((0 != cmcc) || level)
			{
				mu_int_err(ERR_DBKEYGTIND, TRUE, TRUE, buff, comp_length, top_key, top_len,
					(unsigned int)blk_levl);
				free(blk_base);
				return FALSE;
			}
		}
	}
	mu_int_plen--;
	free(blk_base);
	return TRUE;
}
