/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "dsefind.h"
#include "copy.h"
#include "util.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"

GBLREF block_id		patch_find_blk, patch_left_sib, patch_path[MAX_BT_DEPTH + 1], patch_right_sib;
GBLREF boolean_t	patch_exh_found, patch_find_root_search, patch_find_sibs;
GBLREF global_root_list	*global_roots_head;
GBLREF int4		patch_offset[MAX_BT_DEPTH + 1];
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF short int	patch_path_count;
GBLREF VSIG_ATOMIC_T	util_interrupt;

error_def(ERR_CTRLC);
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEINVALBLKID);

void dse_exhaus(int4 pp, int4 op)
{
	block_id	last;
	cache_rec_ptr_t	dummy_cr;
	char		util_buff[MAX_UTIL_LEN];
	int		count, util_len;
	int4		dummy_int;
	global_dir_path	*d_ptr, *temp;
	short		temp_short;
	sm_uc_ptr_t	bp, b_top, nrp, nr_top, ptr, rp, r_top;
	boolean_t	long_blk_id;
	long		blk_id_size;

	last = 0;
	patch_path_count++;
	if (!(bp = t_qread(patch_path[pp - 1], &dummy_int, &dummy_cr)))
		RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	if (((blk_hdr_ptr_t)bp)->bver > BLK_ID_32_VER) /* Check blk version to see if using 32 or 64 bit block_id */
	{
#		ifdef BLK_NUM_64BIT
		long_blk_id = TRUE;
		blk_id_size = SIZEOF(block_id_64);
#		else
		RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#		endif
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
	for (rp = bp + SIZEOF(blk_hdr); rp < b_top; rp = r_top)
	{
		if (util_interrupt)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
			break;
		}
		GET_SHORT(temp_short, &((rec_hdr_ptr_t)rp)->rsiz);
		r_top = rp + temp_short;
		if (r_top > b_top)
			r_top = b_top;
		if (blk_id_size > (r_top - rp))
			break;
		if (((blk_hdr_ptr_t)bp)->levl)
		{
			if (long_blk_id == TRUE)
#				ifdef BLK_NUM_64BIT
				GET_BLK_ID_64(patch_path[pp], (r_top - SIZEOF(block_id_64)));
#				else
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#				endif
			else
				GET_BLK_ID_32(patch_path[pp], (r_top - SIZEOF(block_id_32)));
		} else
		{
			for (ptr = rp + SIZEOF(rec_hdr); (*ptr++ || *ptr++) && (ptr <= r_top);)
				;
			if (long_blk_id == TRUE)
#				ifdef BLK_NUM_64BIT
				GET_BLK_ID_64(patch_path[pp], ptr);
#				else
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#				endif
			else
				GET_BLK_ID_32(patch_path[pp], ptr);
		}
		patch_offset[op] = (int4)(rp - bp);
		if (patch_path[pp] == patch_find_blk)
		{
			if (!patch_exh_found)
			{
				if (patch_find_sibs)
					util_out_print("!/!_Left sibling!_!_Current block!_!_Right sibling", TRUE);
				patch_exh_found = TRUE;
			}
			if (patch_find_sibs)
			{
				patch_left_sib = last;
				if (r_top < b_top)
				{
					nrp = r_top;
					GET_SHORT(temp_short, &((rec_hdr_ptr_t)rp)->rsiz);
					nr_top = nrp + temp_short;
					if (nr_top > b_top)
						nr_top = b_top;
					if (blk_id_size <= (nr_top - nrp))
					{
						if (((blk_hdr_ptr_t)bp)->levl)
							if (long_blk_id == TRUE)
#								ifdef BLK_NUM_64BIT
								GET_BLK_ID_64(patch_right_sib, (nr_top - SIZEOF(block_id_64)));
#								else
								RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#								endif
							else
								GET_BLK_ID_32(patch_right_sib, (nr_top - SIZEOF(block_id_32)));
						else
						{
							for (ptr = rp + SIZEOF(rec_hdr); (*ptr++ || *ptr++) && (ptr <= nr_top);)
								;
							if (long_blk_id == TRUE)
#								ifdef BLK_NUM_64BIT
								GET_BLK_ID_64(patch_right_sib, ptr);
#								else
								RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#								endif
							else
								GET_BLK_ID_32(patch_right_sib, ptr);
						}
					}
				} else
					patch_right_sib = 0;
				if (patch_left_sib)
					util_out_print("!_0x!16@XQ", FALSE, &patch_left_sib);
				else
					util_out_print("!_none!_", FALSE);
				util_out_print("!_0x!16@XQ!_", FALSE, &patch_find_blk);
				if (patch_right_sib)
					util_out_print("0x!16@XQ!/", TRUE, &patch_right_sib);
				else
					util_out_print("none!/", TRUE);
			} else  /* !patch_find_sibs */
			{
				patch_path_count--;
				util_out_print("	Directory path!/	Path--blk:off", TRUE);
				if (!patch_find_root_search)
				{
					d_ptr = global_roots_head->link->dir_path;
					while (d_ptr)
					{
						memcpy(util_buff, "	", 1);
						util_len = 1;
						util_len += i2hexl_nofill(d_ptr->block, (uchar_ptr_t)&util_buff[util_len],
								MAX_HEX_INT8);
						memcpy(&util_buff[util_len], ":", 1);
						util_len += 1;
						/* Using MAX_HEX_SHORT for int value because to save line space
						 * since the value should always fit in 2-bytes
						 */
						util_len += i2hex_nofill(d_ptr->offset, (uchar_ptr_t)&util_buff[util_len],
								MAX_HEX_SHORT);
						util_buff[util_len] = 0;
						util_out_print(util_buff, FALSE);
						temp = d_ptr;
						d_ptr = d_ptr->next;
						free(temp);
					}
					global_roots_head->link->dir_path = 0;
					util_out_print("!/!/	Global paths!/	Path--blk:off", TRUE);
				}
				for (count = 0; count < patch_path_count; count++)
				{
					memcpy(util_buff, "	", 1);
					util_len = 1;
					util_len += i2hexl_nofill(patch_path[count], (uchar_ptr_t)&util_buff[util_len],
							MAX_HEX_INT8);
					memcpy(&util_buff[util_len], ":", 1);
					util_len += 1;
					/* Using MAX_HEX_SHORT for int value because to save line space
					 * since the value should always fit in 2-bytes
					 */
					util_len += i2hex_nofill(patch_offset[count], 	(uchar_ptr_t)&util_buff[util_len],
							MAX_HEX_SHORT);
					util_buff[util_len] = 0;
					util_out_print(util_buff,FALSE);
				}
				memcpy(util_buff, "	", 1);
				util_len = 1;
				util_len += i2hexl_nofill(patch_path[count], (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
				util_buff[util_len] = 0;
				util_out_print(util_buff, TRUE);
				patch_path_count++;
			}
		}
		if ((0 < patch_path[pp]) && (patch_path[pp] < cs_addrs->ti->total_blks) && (patch_path[pp] % cs_addrs->hdr->bplmap))
		{
			if (1 < ((blk_hdr_ptr_t)bp)->levl)
				dse_exhaus(pp + 1, op + 1);
			else if ((1 == ((blk_hdr_ptr_t)bp)->levl) && patch_find_root_search)
				dse_find_roots(patch_path[pp]);
		}
		last = patch_path[pp];
	}
	patch_path_count--;
	return;
}
