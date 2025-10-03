/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gtm_signal.h"

#include "error.h"
#include "gdsroot.h"
#include "gdsdbver.h"
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
#include "filestruct.h"

/* Include prototypes*/
#include "t_qread.h"

GBLDEF boolean_t	patch_exh_found, patch_find_root_search, patch_find_sibs;
GBLDEF block_id		patch_find_blk, patch_left_sib, patch_right_sib;
GBLDEF block_id		patch_path[MAX_BT_DEPTH + 1], patch_path1[MAX_BT_DEPTH + 1];
GBLREF gd_region	*gv_cur_region;
GBLDEF global_root_list	*global_roots_head, *global_roots_tail;
GBLDEF int4		patch_offset[MAX_BT_DEPTH + 1], patch_offset1[MAX_BT_DEPTH + 1];
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLDEF short int	patch_dir_path_count, patch_path_count;

static boolean_t	was_crit, was_hold_onto_crit, nocrit_present;

error_def(ERR_CTRLC);
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEINVALBLKID);

void dse_f_blk(void)
{
	block_id		last, look;
	boolean_t		exhaust, long_blk_id;
	cache_rec_ptr_t		dummy_cr;
	char			targ_key[MAX_KEY_SZ + 1], util_buff[MAX_UTIL_LEN];
	global_dir_path		*d_ptr, *dtemp;
	global_root_list	*temp;
	int			util_len, lvl, parent_lvl;
	int4			dummy_int;
	long			blk_id_size;
	short int		count, rsize, size;
	sm_uc_ptr_t		blk_id, bp, b_top, key_top, rp, r_top, sp, srp, s_top;

	DSE_DB_IS_TOO_OLD(cs_addrs, cs_data, gv_cur_region);
	if (BADDSEBLK == (patch_find_blk = dse_getblk("BLOCK", DSENOBML, DSEBLKCUR)))		/* WARNING: assignment */
		return;
	patch_find_sibs = (CLI_PRESENT == cli_present("SIBLINGS"));
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	/* ESTABLISH is done here because dse_f_blk_ch() assumes we already have crit. */
	ESTABLISH(dse_f_blk_ch);
	look = patch_find_blk;
	parent_lvl = 0;
	do
	{
		if (!(bp = t_qread(look, &dummy_int, &dummy_cr)))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_DSEBLKRDFAIL, 1, look);
		if (((blk_hdr_ptr_t)bp)->bver > BLK_ID_32_VER) /* Check blk version to see if using 32 or 64 bit block_id */
		{
#			ifdef BLK_NUM_64BIT
			long_blk_id = TRUE;
			blk_id_size = SIZEOF(block_id_64);
#			else
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			REVERT;
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#			endif
		} else
		{
			long_blk_id = FALSE;
			blk_id_size = SIZEOF(block_id_32);
		}
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
		lvl = ((blk_hdr_ptr_t)bp)->levl;
		if (lvl && (key_top > (blk_id = r_top - blk_id_size)))	/* NOTE assignment */
			key_top = blk_id;
		size = key_top - rp - SIZEOF(rec_hdr);
		if (SIZEOF(targ_key) < size)
			size = SIZEOF(targ_key);
		if (!lvl || size)
			break;	/* data block OR index block with a non-* key found. break right away to do search */
		if ((0 > lvl) || (lvl >= MAX_BT_DEPTH) || (parent_lvl && (parent_lvl != (lvl + 1))))
			break; /* out-of-design level (integ error in db). do not descend anymore. do exhaustive search */
		parent_lvl = lvl;
		/* while it is an index block with only a *-record keep looking in child blocks for key */
		if (long_blk_id == TRUE)
		{
#			ifdef BLK_NUM_64BIT
			GET_BLK_ID_64(look, blk_id);
#			else
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			REVERT;
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#			endif
		} else
			GET_BLK_ID_32(look, blk_id);
	} while (TRUE);
	patch_path_count = 1;
	patch_path[0] = get_dir_root();
	patch_left_sib = patch_right_sib = 0;
	patch_find_root_search = TRUE;
	if ((exhaust = (CLI_PRESENT == cli_present("EXHAUSTIVE"))) || (0 >= size))		/* NOTE assignment */
	{
		if ((patch_exh_found = (patch_find_blk == patch_path[0])))			/* NOTE assignment */
		{
			if (patch_find_sibs)
			{
				util_out_print("!/!_Left sibling!_!_Current block!_!_Right sibling", TRUE);
				util_out_print("!_none!_!_!_0x!16@XQ!_none",TRUE, &patch_find_blk);
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
				if ((patch_exh_found = (patch_find_blk == patch_path[0])))	/* NOTE assignment */
				{
					if (patch_find_sibs)
					{
						util_out_print("!/!_Left sibling!_!_Current block!_!_Right sibling", TRUE);
						util_out_print("!_none!_!_!_0x!16@XQ!_none",TRUE, &patch_find_blk);
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
								util_len += i2hexl_nofill(d_ptr->block,
									(uchar_ptr_t)&util_buff[util_len], 16);
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
							util_len += i2hexl_nofill(patch_path[count],
								(uchar_ptr_t)&util_buff[util_len], 16);
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
						util_len += i2hexl_nofill(patch_path[count], (uchar_ptr_t)&util_buff[util_len],
								MAX_HEX_INT8);
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
		util_out_print("!/!_Left sibling!_!_Current block!_!_Right sibling", TRUE);
		if (!patch_left_sib)
		{
			for (last = 0, lvl = (patch_find_root_search ? patch_dir_path_count : patch_path_count) - 1; 0 <= --lvl;)
			{
				if (!(sp = t_qread(patch_find_root_search ? patch_path[lvl] : patch_path1[lvl], &dummy_int,
					&dummy_cr)))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_DSEBLKRDFAIL, 1,
							(patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]));
				if (((blk_hdr_ptr_t)sp)->bver > BLK_ID_32_VER) /* Check to see if using 32 or 64 bit block_id */
				{
#					ifdef BLK_NUM_64BIT
					long_blk_id = TRUE;
					blk_id_size = SIZEOF(block_id_64);
#					else
					DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
							nocrit_present, cs_addrs, gv_cur_region);
					REVERT;
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#					endif
				} else
				{
					long_blk_id = FALSE;
					blk_id_size = SIZEOF(block_id_32);
				}
				if (((blk_hdr_ptr_t)sp)->bsiz > cs_addrs->hdr->blk_size)
					s_top = sp + cs_addrs->hdr->blk_size;
				else if (SIZEOF(blk_hdr) > ((blk_hdr_ptr_t)sp)->bsiz)
				{
					util_out_print("Error: sibling search hit problem blk 0x!16@XQ",
						TRUE, patch_find_root_search ? &(patch_path[lvl]) : &(patch_path1[lvl]));
					lvl = -1;
					break;
				} else
					s_top = sp + ((blk_hdr_ptr_t)sp)->bsiz;
				srp = sp + SIZEOF(blk_hdr);
				GET_SHORT(rsize, &((rec_hdr_ptr_t)srp)->rsiz);
				srp += rsize;
				if (long_blk_id == TRUE)
				{
#					ifdef BLK_NUM_64BIT
					GET_BLK_ID_64(look, srp - SIZEOF(block_id_64));
#					else
					DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
							nocrit_present, cs_addrs, gv_cur_region);
					REVERT;
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#					endif
				} else
					GET_BLK_ID_32(look, srp - SIZEOF(block_id_32));
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
					if (long_blk_id == TRUE)
					{
#						ifdef BLK_NUM_64BIT
						GET_BLK_ID_64(look, srp - SIZEOF(block_id_64));
#						else
						DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
								nocrit_present, cs_addrs, gv_cur_region);
						REVERT;
						RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#						endif
					} else
						GET_BLK_ID_32(look, srp - SIZEOF(block_id_32));
				}
				if ((patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]) != look)
				{
					util_out_print("Error: sibling search hit problem blk 0x!16@XQ",
						TRUE, patch_find_root_search ? &(patch_path[lvl]) : &(patch_path1[lvl]));
					last = 0;
					lvl = (patch_find_root_search ? patch_dir_path_count : patch_path_count);
				} else if (last >= cs_addrs->ti->total_blks)
				{	/* should never come here as block was previously OK, but this is dse so be careful */
					util_out_print("Error: sibling search got 0x!16@XQ which exceeds total blocks 0x!16@XQ",
						       TRUE, &last, &(cs_addrs->ti->total_blks));
					last = 0;
					lvl = (patch_find_root_search ? patch_dir_path_count : patch_path_count);
				}
				for (lvl++; lvl < (patch_find_root_search ? patch_dir_path_count : patch_path_count); lvl++)
				{
					if (!(sp = t_qread(last, &dummy_int, &dummy_cr)))
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_DSEBLKRDFAIL, 1, last);
					if (((blk_hdr_ptr_t)sp)->bver > BLK_ID_32_VER) /* Check if using 32 or 64 bit block_id */
					{
#						ifdef BLK_NUM_64BIT
						long_blk_id = TRUE;
						blk_id_size = SIZEOF(block_id_64);
#						else
						DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
								nocrit_present, cs_addrs, gv_cur_region);
						REVERT;
						RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#						endif
					} else
					{
						long_blk_id = FALSE;
						blk_id_size = SIZEOF(block_id_32);
					}
					if (((blk_hdr_ptr_t)sp)->bsiz > cs_addrs->hdr->blk_size)
						s_top = sp + cs_addrs->hdr->blk_size;
					else if (SIZEOF(blk_hdr) > ((blk_hdr_ptr_t)sp)->bsiz)
					{
						util_out_print("Error: sibling search hit problem blk 0x!16@XQ", TRUE, &last);
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
					if (long_blk_id == TRUE)
					{
#						ifdef BLK_NUM_64BIT
						GET_BLK_ID_64(last, s_top - SIZEOF(block_id_64));
#						else
						DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
								nocrit_present, cs_addrs, gv_cur_region);
						REVERT;
						RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#						endif
					} else
						GET_BLK_ID_32(last, s_top - SIZEOF(block_id_32));
					if (last >= cs_addrs->ti->total_blks)
					{
						util_out_print(
							"Error: sibling search got 0x!16@XQ which exceeds total blocks 0x!16@XQ",
							TRUE, &last, &(cs_addrs->ti->total_blks));
						break;
					}
				}
			}
			patch_left_sib = last;
		}
		if (patch_left_sib)
			util_out_print("!_0x!16@XQ", FALSE, &patch_left_sib);
		else
			util_out_print("!_none!_!_", FALSE);
		util_out_print("!_0x!16@XQ!_", FALSE, &patch_find_blk);
		if (!patch_right_sib)
		{
			for (lvl = (patch_find_root_search ? patch_dir_path_count : patch_path_count) - 1; 0 <= --lvl;)
			{
				if (!(sp = t_qread(patch_find_root_search ? patch_path[lvl] : patch_path1[lvl], &dummy_int,
					&dummy_cr)))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_DSEBLKRDFAIL, 1,
						(patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]));
				if (((blk_hdr_ptr_t)sp)->bver > BLK_ID_32_VER) /* Check to see if using 32 or 64 bit block_id */
				{
#					ifdef BLK_NUM_64BIT
					long_blk_id = TRUE;
					blk_id_size = SIZEOF(block_id_64);
#					else
					DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
							nocrit_present, cs_addrs, gv_cur_region);
					REVERT;
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#					endif
				} else
				{
					long_blk_id = FALSE;
					blk_id_size = SIZEOF(block_id_32);
				}
				if (((blk_hdr_ptr_t)sp)->bsiz > cs_addrs->hdr->blk_size)
					s_top = sp + cs_addrs->hdr->blk_size;
				else if (SIZEOF(blk_hdr) > ((blk_hdr_ptr_t)sp)->bsiz)
				{
					util_out_print("Error: sibling search hit problem blk 0x!16@XQ",
						TRUE, patch_find_root_search ? &(patch_path[lvl]) : &(patch_path1[lvl]));
					lvl = -1;
					break;
				} else
					s_top = sp + ((blk_hdr_ptr_t)sp)->bsiz;
				if (long_blk_id == TRUE)
				{
#					ifdef BLK_NUM_64BIT
					GET_BLK_ID_64(look, s_top - SIZEOF(block_id_64));
#					else
					DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
							nocrit_present, cs_addrs, gv_cur_region);
					REVERT;
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#					endif
				} else
					GET_BLK_ID_32(look, s_top - SIZEOF(block_id_32));
				if (look >= cs_addrs->ti->total_blks)
				{
					util_out_print("Error: sibling search got 0x!16@XQ which exceeds total blocks 0x!16@XQ",
						       TRUE, &look, &(cs_addrs->ti->total_blks));
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
					if (long_blk_id == TRUE)
					{
#						ifdef BLK_NUM_64BIT
						GET_BLK_ID_64(look, srp - SIZEOF(block_id_64));
#						else
						DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
								nocrit_present, cs_addrs, gv_cur_region);
						REVERT;
						RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#						endif
					} else
						GET_BLK_ID_32(look, srp - SIZEOF(block_id_32));
					if (look >= cs_addrs->ti->total_blks)
					{
						util_out_print(
							"Error: sibling search got 0x!16@XQ which exceeds total blocks 0x!16@XQ",
							TRUE, &look, &(cs_addrs->ti->total_blks));
						break;
					}
				}
				if ((patch_find_root_search ? patch_path[lvl] : patch_path1[lvl]) != last)
				{
					util_out_print("Error: sibling search hit problem blk 0x!16@XQ",
						TRUE, patch_find_root_search ? &(patch_path[lvl]) : &(patch_path1[lvl]));
					look = 0;
					lvl = (patch_find_root_search ? patch_dir_path_count : patch_path_count);
				}
				for (lvl++; lvl < (patch_find_root_search ? patch_dir_path_count : patch_path_count); lvl++)
				{
					if (!(sp = t_qread(look, &dummy_int, &dummy_cr)))	/* NOTE assignment */
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_DSEBLKRDFAIL, 1, look);
					if (((blk_hdr_ptr_t)sp)->bver > BLK_ID_32_VER) /* Check if using 32 or 64 bit block_id */
					{
#						ifdef BLK_NUM_64BIT
						long_blk_id = TRUE;
						blk_id_size = SIZEOF(block_id_64);
#						else
						DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
								nocrit_present, cs_addrs, gv_cur_region);
						REVERT;
						RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#						endif
					} else
					{
						long_blk_id = FALSE;
						blk_id_size = SIZEOF(block_id_32);
					}
					if (!(((blk_hdr_ptr_t)sp)->bsiz > cs_addrs->hdr->blk_size) &&
							(SIZEOF(blk_hdr) > ((blk_hdr_ptr_t)sp)->bsiz))
					{
						util_out_print("Error: sibling search hit problem blk 0x!XL", TRUE, look);
						look = 0;
						break;
					}
					if (0 >= (signed char)(((blk_hdr_ptr_t)sp)->levl))
					{
						util_out_print("Error: sibling search reached level 0", TRUE);
						look = 0;
						break;
					}
					srp = sp + SIZEOF(blk_hdr);
					GET_SHORT(rsize, &((rec_hdr_ptr_t)srp)->rsiz);
					srp += rsize;
					if (long_blk_id == TRUE)
					{
#						ifdef BLK_NUM_64BIT
						GET_BLK_ID_64(look, srp - SIZEOF(block_id_64));
#						else
						DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit,
								nocrit_present, cs_addrs, gv_cur_region);
						REVERT;
						RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#						endif
					} else
						GET_BLK_ID_32(look, srp - SIZEOF(block_id_32));
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
			util_out_print("0x!16@XQ!/", TRUE, &patch_right_sib);
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
		util_len += i2hexl_nofill(patch_path[count], (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
		memcpy(&util_buff[util_len], ":", 1);
		util_len += 1;
		/* Using MAX_HEX_SHORT for int value because to save line space
		 * since the value should always fit in 2-bytes
		 */
		util_len += i2hex_nofill(patch_offset[count], (uchar_ptr_t)&util_buff[util_len], MAX_HEX_SHORT);
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
			util_len += i2hexl_nofill(patch_path1[count], (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
			memcpy(&util_buff[util_len], ":", 1);
			util_len += 1;
			/* Using MAX_HEX_SHORT for int value because to save line space
			 * since the value should always fit in 2-bytes
			 */
			util_len += i2hex_nofill(patch_offset1[count], (uchar_ptr_t)&util_buff[util_len], MAX_HEX_SHORT);
			memcpy(&util_buff[util_len], ",", 1);
			util_len += 1;
			util_buff[util_len] = 0;
			util_out_print(util_buff, FALSE);
		}
	} else
		assert(patch_find_root_search);		/* OK to assert since pro works as desired */
	memcpy(util_buff, "	", 1);
	util_len = 1;
	util_len += i2hexl_nofill(patch_find_root_search ? patch_path[count] : patch_path1[count],
		(uchar_ptr_t)&util_buff[util_len], 16);
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
	START_CH(TRUE);

	if (ERR_CTRLC == SIGNAL)
		DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	NEXTCH;
}
