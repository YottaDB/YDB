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

#include <signal.h>

#include "error.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "dsefind.h"
#include "cli.h"
#include "copy.h"
#include "util.h"
#include "dse.h"

/* Include prototypes*/
#include "t_qread.h"

GBLDEF bool		patch_exh_found, patch_find_root_search, patch_find_sibs;
GBLDEF block_id		patch_find_blk, patch_left_sib, patch_right_sib;
GBLDEF block_id		patch_path[MAX_BT_DEPTH + 1], patch_path1[MAX_BT_DEPTH + 1];
GBLDEF global_root_list	*global_roots_head, *global_roots_tail;
GBLDEF int4		patch_offset[MAX_BT_DEPTH + 1], patch_offset1[MAX_BT_DEPTH + 1];
GBLDEF short int	patch_dir_path_count;

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF block_id		patch_curr_blk;
GBLREF short int	patch_path_count;

#define MAX_UTIL_LEN 33

static boolean_t	was_crit, was_hold_onto_crit, nocrit_present;

error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_CTRLC);

void dse_f_blk(void)
{
	block_id		blk, last, look;
	boolean_t		exhaust;
	cache_rec_ptr_t		dummy_cr;
	char			targ_key[MAX_KEY_SZ + 1], util_buff[MAX_UTIL_LEN];
	global_root_list	*temp;
	global_dir_path		*d_ptr, *dtemp;
	int			util_len;
	int4			dummy_int;
	sm_uc_ptr_t		blk_id, bp, b_top, key_top, rp, r_top, sp, srp, s_top;
	short int		count, rsize, size;
	char			lvl;

	if (CLI_PRESENT == cli_present("BLOCK"))
	{
		if (!cli_get_hex("BLOCK", (uint4 *)&blk))
			return;
		if ((0 > blk) || (blk >= cs_addrs->ti->total_blks) || !(blk % cs_addrs->hdr->bplmap))
		{
			util_out_print("Error: invalid block number.", TRUE);
			return;
		}
		patch_curr_blk = blk;
	}
	patch_find_sibs = (CLI_PRESENT == cli_present("SIBLINGS"));
	patch_find_blk = patch_curr_blk;
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	/* ESTABLISH is done here because dse_f_blk_ch() assumes we already have crit. */
	ESTABLISH(dse_f_blk_ch);
	if (!(bp = t_qread(patch_find_blk, &dummy_int, &dummy_cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	if (((blk_hdr_ptr_t) bp)->bsiz > cs_addrs->hdr->blk_size)
		b_top = bp + cs_addrs->hdr->blk_size;
	else if (SIZEOF(blk_hdr) > ((blk_hdr_ptr_t) bp)->bsiz)
		b_top = bp + SIZEOF(blk_hdr);
	else
		b_top = bp + ((blk_hdr_ptr_t)bp)->bsiz;
	rp = bp + SIZEOF(blk_hdr);
	GET_SHORT(rsize, &((rec_hdr_ptr_t) rp)->rsiz);
	if (SIZEOF(rec_hdr) > rsize)
		r_top = rp + SIZEOF(rec_hdr);
	else
		r_top = rp + rsize;
	if (r_top > b_top)
		r_top = b_top;
	for (key_top = rp + SIZEOF(rec_hdr); (key_top < r_top) && *key_top++; )
		;
	if (((blk_hdr_ptr_t)bp)->levl && key_top > (blk_id = r_top - SIZEOF(block_id)))	/* NOTE assignment */
		key_top = blk_id;
	patch_path_count = 1;
	patch_path[0] = get_dir_root();
	patch_left_sib = patch_right_sib = 0;
	size = key_top - rp - SIZEOF(rec_hdr);
	if (SIZEOF(targ_key) < size)
		size = SIZEOF(targ_key);
	patch_find_root_search = TRUE;
	if ((exhaust = (cli_present("EXHAUSTIVE") == CLI_PRESENT)) || (0 >= size))		/* NOTE assignment */
	{
		if (size < 0)
		{
			util_out_print("No keys in block, cannot perform ordered search.", TRUE);
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			REVERT;
			return;
		}
		if (patch_exh_found = (patch_find_blk == patch_path[0]))			/* NOTE assignment */
		{
			if (patch_find_sibs)
			{
				util_out_print("!/!_Left sibling!_Current block!_Right sibling", TRUE);
				util_out_print("!_none!_!_0x!XL!_none",TRUE, patch_find_blk);
			} else
			{
				assert(1 == patch_path[0]);	/* OK to assert because pro prints */
				util_out_print("!/    Directory path!/    Path--blk:off!/!_1", TRUE);
			}
		} else
		{
			global_roots_head = (global_root_list *)malloc(SIZEOF(global_root_list));
			global_roots_tail = global_roots_head;
			global_roots_head->link = NULL;
			global_roots_head->dir_path = NULL;
			dse_exhaus(1, 0);
			patch_find_root_search = FALSE;
			while (!patch_exh_found && global_roots_head->link)
			{
				patch_path[0] = global_roots_head->link->root;
				patch_path_count = 1;
				patch_left_sib = patch_right_sib = 0;
				if (patch_exh_found = (patch_find_blk == patch_path[0]))	/* NOTE assignment */
				{
					if (patch_find_sibs)
					{
						util_out_print("!/!_Left sibling!_Current block!_Right sibling", TRUE);
						util_out_print("!_none!_!_0x!XL!_none",TRUE, patch_find_blk);
					} else
					{
						patch_path_count--;
						util_out_print("!/    Directory path!/    Path--blk:off", TRUE);
						if (!patch_find_root_search)
						{
							d_ptr = global_roots_head->link->dir_path;
							while (d_ptr)
							{
								memcpy(util_buff, "	", 1);
								util_len = 1;
								util_len += i2hex_nofill(d_ptr->block,
									(uchar_ptr_t)&util_buff[util_len], 8);
								memcpy(&util_buff[util_len], ":", 1);
								util_len += 1;
								util_len += i2hex_nofill(d_ptr->offset,
									(uchar_ptr_t)&util_buff[util_len], 4);
								memcpy(&util_buff[util_len], ",", 1);
								util_len += 1;
								util_buff[util_len] = 0;
								util_out_print(util_buff, FALSE);
								temp = (global_root_list *)d_ptr;
								d_ptr = d_ptr->next;
								free(temp);
							}
							global_roots_head->link->dir_path = 0;
							util_out_print("!/    Global tree path!/    Path--blk:off", TRUE);
						}
						for (count = 0; count < patch_path_count; count++)
						{
							memcpy(util_buff, "	", 1);
							util_len = 1;
							util_len += i2hex_nofill(patch_path[count],
								(uchar_ptr_t)&util_buff[util_len], 8);
							memcpy(&util_buff[util_len], ":", 1);
							util_len += 1;
							util_len += i2hex_nofill(patch_offset[count],
								(uchar_ptr_t)&util_buff[util_len], 4);
							memcpy(&util_buff[util_len], ",", 1);
							util_len += 1;
							util_buff[util_len] = 0;
							util_out_print(util_buff, FALSE);
						}
						memcpy(util_buff, "	", 1);
						util_len = 1;
						util_len += i2hex_nofill(patch_path[count], (uchar_ptr_t)&util_buff[util_len], 8);
						util_buff[util_len] = 0;
						util_out_print(util_buff, TRUE);
					}
				} else
					dse_exhaus(1, 0);
				temp = global_roots_head;
				d_ptr = global_roots_head->link->dir_path;
				while (d_ptr)
				{
					dtemp = d_ptr;
					d_ptr = d_ptr->next;
					free(dtemp);
				}
				global_roots_head = global_roots_head->link;
				free(temp);
			}
			while (global_roots_head->link)
			{
				temp = global_roots_head;
				d_ptr = global_roots_head->link->dir_path;
				while (d_ptr)
				{
					dtemp = d_ptr;
					d_ptr = d_ptr->next;
					free(dtemp);
				}
				global_roots_head = global_roots_head->link;
				free(temp);
			}
		}
		if (!patch_exh_found)
		{
			if (exhaust)
				util_out_print("Error: exhaustive search fail.", TRUE);
			else
				util_out_print("Error: ordered search fail.", TRUE);
		} else
			util_out_print(0, TRUE);
		DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
		REVERT;
		return;
	} else /* !exhaust && size > 0 */
	{
		if (!dse_is_blk_in(rp, r_top, size))
		{
			util_out_print("Error: ordered search fail.", TRUE);
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			REVERT;
			return;
		}
	}
	if (patch_find_sibs)
	{	/* the cross-branch sib action could logically go in dse_order but is here 'cause it only gets used when needed */
		util_out_print("!/!_Left sibling!_Current block!_Right sibling", TRUE);
		if (!patch_left_sib)
		{
			for (last = 0, lvl = (patch_find_root_search ? patch_dir_path_count : patch_path_count) - 1; 0 <= --lvl;)
			{
				if (!(sp = t_qread(patch_find_root_search ? patch_path[lvl] : patch_path1[lvl], &dummy_int,
					&dummy_cr)))
						rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
				if (((blk_hdr_ptr_t)sp)->bsiz > cs_addrs->hdr->blk_size)
					s_top = sp + cs_addrs->hdr->blk_size;
				else if (SIZEOF(blk_hdr) > ((blk_hdr_ptr_t)sp)->bsiz)
				{
					util_out_print("Error: sibling search hit problem blk 0x!XL",
						TRUE, patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]);
					lvl = -1;
					break;
				} else
					s_top = sp + ((blk_hdr_ptr_t)sp)->bsiz;
				srp = sp + SIZEOF(blk_hdr);
				GET_SHORT(rsize, &((rec_hdr_ptr_t)srp)->rsiz);
				srp += rsize;
				GET_LONG(look, srp - SIZEOF(block_id));
				if ((patch_find_root_search ? patch_path[lvl + 1] : patch_path1[lvl + 1]) != look)
					break;
			}
			if (0 <= lvl)
			{
				for (lvl++; (srp < s_top)
					&& ((patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]) != look);)
				{
					last = look;
					GET_SHORT(rsize, &((rec_hdr_ptr_t)srp)->rsiz);
					srp += rsize;
					if (srp > s_top)
						break;
					GET_LONG(look, srp - SIZEOF(block_id));
				}
				if ((patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]) != look)
				{
					util_out_print("Error: sibling search hit problem blk 0x!XL",
						TRUE, patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]);
					last = 0;
					lvl = (patch_find_root_search ? patch_dir_path_count : patch_path_count);
				} else if (last >= cs_addrs->ti->total_blks)
				{	/* should never come here as block was previously OK, but this is dse so be careful */
					util_out_print("Error: sibling search got 0x!XL which exceeds total blocks 0x!XL",
						       TRUE, last, cs_addrs->ti->total_blks);
					last = 0;
					lvl = (patch_find_root_search ? patch_dir_path_count : patch_path_count);
				}
				for (lvl++; lvl < (patch_find_root_search ? patch_dir_path_count : patch_path_count); lvl++)
				{
					if (!(sp = t_qread(last, &dummy_int, &dummy_cr)))
						rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
					if (((blk_hdr_ptr_t)sp)->bsiz > cs_addrs->hdr->blk_size)
						s_top = sp + cs_addrs->hdr->blk_size;
					else if (SIZEOF(blk_hdr) > ((blk_hdr_ptr_t)sp)->bsiz)
					{
						util_out_print("Error: sibling search hit problem blk 0x!XL", TRUE, last);
						last = 0;
						break;
					} else
						s_top = sp + ((blk_hdr_ptr_t)sp)->bsiz;
					if (0 >= (signed char)(((blk_hdr_ptr_t)sp)->levl))
					{
						util_out_print("Error: sibling search reached level 0", TRUE);
						last = 0;
						break;
					}
					GET_LONG(last, s_top - SIZEOF(block_id));
					if (last >= cs_addrs->ti->total_blks)
					{
						util_out_print("Error: sibling search got 0x!XL which exceeds total blocks 0x!XL",
							TRUE, last, cs_addrs->ti->total_blks);
						break;
					}
				}
			}
			patch_left_sib = last;
		}
		if (patch_left_sib)
			util_out_print("!_0x!XL", FALSE, patch_left_sib);
		else
			util_out_print("!_none!_", FALSE);
		util_out_print("!_0x!XL!_", FALSE, patch_find_blk);
		if (!patch_right_sib)
		{
			for (lvl = (patch_find_root_search ? patch_dir_path_count : patch_path_count) - 1; 0 <= --lvl;)
			{
				if (!(sp = t_qread(patch_find_root_search ? patch_path[lvl] : patch_path1[lvl], &dummy_int,
					&dummy_cr)))
					rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
				if (((blk_hdr_ptr_t)sp)->bsiz > cs_addrs->hdr->blk_size)
					s_top = sp + cs_addrs->hdr->blk_size;
				else if (SIZEOF(blk_hdr) > ((blk_hdr_ptr_t)sp)->bsiz)
				{
					util_out_print("Error: sibling search hit problem blk 0x!XL",
						TRUE, patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]);
					lvl = -1;
					break;
				} else
					s_top = sp + ((blk_hdr_ptr_t)sp)->bsiz;
				GET_LONG(look, s_top - SIZEOF(block_id));
				if (look >= cs_addrs->ti->total_blks)
				{
					util_out_print("Error: sibling search got 0x!XL which exceeds total blocks 0x!XL",
						       TRUE, look, cs_addrs->ti->total_blks);
					lvl = -1;
					break;
				}
				if ((patch_find_root_search ? patch_path[lvl + 1] : patch_path1[lvl + 1]) != look)
					break;
			}
			if (0 <= lvl)
			{
				srp = sp + SIZEOF(blk_hdr);
				for (lvl++; (srp < s_top)
					&& ((patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]) != last);)
				{
					last = look;
					GET_SHORT(rsize, &((rec_hdr_ptr_t)srp)->rsiz);
					srp += rsize;
					if (srp > s_top)
						break;
					GET_LONG(look, srp - SIZEOF(block_id));
					if (look >= cs_addrs->ti->total_blks)
					{
						util_out_print("Error: sibling search got 0x!XL which exceeds total blocks 0x!XL",
							       TRUE, look, cs_addrs->ti->total_blks);
						break;
					}
				}
				if ((patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]) != last)
				{
					util_out_print("Error: sibling search hit problem blk 0x!XL",
						TRUE, patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]);
					look = 0;
					lvl = (patch_find_root_search ? patch_dir_path_count : patch_path_count);
				}
				for (lvl++; lvl < (patch_find_root_search ? patch_dir_path_count : patch_path_count); lvl++)
				{
					if (!(sp = t_qread(look, &dummy_int, &dummy_cr)))	/* NOTE assignment */
						rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
					if (((blk_hdr_ptr_t)sp)->bsiz > cs_addrs->hdr->blk_size)
						s_top = sp + cs_addrs->hdr->blk_size;
					else if (SIZEOF(blk_hdr) > ((blk_hdr_ptr_t)sp)->bsiz)
					{
						util_out_print("Error: sibling search hit problem blk 0x!XL", TRUE, look);
						look = 0;
						break;
					} else
						s_top = sp + ((blk_hdr_ptr_t)sp)->bsiz;
					if (0 >= (signed char)(((blk_hdr_ptr_t)sp)->levl))
					{
						util_out_print("Error: sibling search reached level 0", TRUE);
						look = 0;
						break;
					}
					srp = sp + SIZEOF(blk_hdr);
					GET_SHORT(rsize, &((rec_hdr_ptr_t)srp)->rsiz);
					srp += rsize;
					GET_LONG(look, srp - SIZEOF(block_id));
					if (look >= cs_addrs->ti->total_blks)
					{
						util_out_print("Error: sibling search got 0x!XL which exceeds total blocks 0x!XL",
							       TRUE, look, cs_addrs->ti->total_blks);
						look = 0;
						break;
					}
				}
			} else
				look = 0;
			patch_right_sib = look;
		}
		if (patch_right_sib)
			util_out_print("0x!XL!/", TRUE, patch_right_sib);
		else
			util_out_print("none!/", TRUE);
		DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
		REVERT;
		return;
	}
	util_out_print("!/    Directory path!/    Path--blk:off", TRUE);
	patch_dir_path_count--;
	for (count = 0; count < patch_dir_path_count; count++)
	{
		memcpy(util_buff, "	", 1);
		util_len = 1;
		util_len += i2hex_nofill(patch_path[count], (uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len], ":", 1);
		util_len += 1;
		util_len += i2hex_nofill(patch_offset[count], (uchar_ptr_t)&util_buff[util_len], 4);
		memcpy(&util_buff[util_len], ",", 1);
		util_len += 1;
		util_buff[util_len] = 0;
		util_out_print(util_buff, FALSE);
	}
	if (!patch_find_root_search)
	{
		assert(patch_path_count);		/* OK to assert since pro works as desired */
		util_out_print("!/    Global tree path!/    Path--blk:off", TRUE);
	}
	if (patch_path_count)
	{
		patch_path_count--;
		for (count = 0; count < patch_path_count; count++)
		{
			memcpy(util_buff, "	", 1);
			util_len = 1;
			util_len += i2hex_nofill(patch_path1[count], (uchar_ptr_t)&util_buff[util_len], 8);
			memcpy(&util_buff[util_len], ":", 1);
			util_len += 1;
			util_len += i2hex_nofill(patch_offset1[count], (uchar_ptr_t)&util_buff[util_len], 4);
			memcpy(&util_buff[util_len], ",", 1);
			util_len += 1;
			util_buff[util_len] = 0;
			util_out_print(util_buff, FALSE);
		}
	} else
		assert(patch_find_root_search);		/* OK to assert since pro works as desired */
	memcpy(util_buff, "	", 1);
	util_len = 1;
	util_len += i2hex_nofill(patch_find_root_search ? patch_path[count] : patch_path1[count],
		(uchar_ptr_t)&util_buff[util_len], 8);
	memcpy(&util_buff[util_len], "!/", 2);
	util_len += 2;
	util_buff[util_len] = 0;
	util_out_print(util_buff, TRUE);
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	REVERT;
	return;
}

/* Control-C condition handler */
CONDITION_HANDLER(dse_f_blk_ch)
{
	START_CH;

	if (ERR_CTRLC == SIGNAL)
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	NEXTCH;
}

