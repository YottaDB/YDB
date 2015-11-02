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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "cli.h"
#include "dse.h"
#include "util.h"

/* Include prototypes */
#include "t_qread.h"

#define REUSABLE_CHAR	":"
#define FREE_CHAR	"."
#define BUSY_CHAR	"X"
#define CORRUPT_CHAR	"?"
#define MAX_UTIL_LEN	80

GBLREF VSIG_ATOMIC_T	util_interrupt;
GBLREF block_id		patch_curr_blk;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF int		patch_is_fdmp;
GBLREF int		patch_fdmp_recs;
GBLREF int		patch_rec_counter;
LITREF char		*gtm_dbversion_table[];

error_def(ERR_BITMAPSBAD);
error_def(ERR_CTRLC);
error_def(ERR_DSEBLKRDFAIL);

boolean_t dse_b_dmp(void)
{
	int4		util_len, head, lmap_num, iter1, iter2, mapsize, bplmap, nocrit_present, dummy_int, len, count;
	unsigned char	util_buff[MAX_UTIL_LEN], mask;
	boolean_t	free, was_crit, was_hold_onto_crit, invalid_bitmap = FALSE, is_mm;
	block_id	blk;
	sm_uc_ptr_t	bp, b_top, rp, mb, dump_record(sm_uc_ptr_t rp, block_id blk, sm_uc_ptr_t bp, sm_uc_ptr_t b_top);
	cache_rec_ptr_t cr;
	enum db_ver	ondsk_blkver;

	head = cli_present("HEADER");
	if (CLI_PRESENT == cli_present("BLOCK"))
	{
		if (!cli_get_hex("BLOCK", (uint4 *)&blk))
			return FALSE;
		if (blk < 0 || blk >= cs_addrs->ti->total_blks)
		{
			util_out_print("Error: invalid block number.", TRUE);
			return FALSE;
		}
		patch_curr_blk = blk;
	} else
		blk = patch_curr_blk;
	if (CLI_PRESENT == cli_present("COUNT"))
	{
		if (!cli_get_hex("COUNT", (uint4 *)&count))
			return FALSE;
		if (count < 1)
			return FALSE;
	} else
		count = 1;

	util_out_print(0, TRUE);
	bplmap = cs_addrs->hdr->bplmap;
	is_mm = (dba_mm == cs_addrs->hdr->acc_meth);
	mapsize = BM_SIZE(bplmap);
	patch_rec_counter = 1;
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	for ( ; ; )
	{
		if (blk / bplmap * bplmap != blk)
		{
			if (!(bp = t_qread(blk, &dummy_int, &cr)))
			{
				DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
				rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
			}
			if (((blk_hdr_ptr_t) bp)->levl && patch_is_fdmp)
			{
				DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
				util_out_print("Error:  cannot perform GLO/ZWR dump on index block.", TRUE);
				return FALSE;
			}
			if (((blk_hdr_ptr_t) bp)->bsiz > cs_addrs->hdr->blk_size)
				b_top = bp + cs_addrs->hdr->blk_size;
			else if (((blk_hdr_ptr_t) bp)->bsiz < SIZEOF(blk_hdr))
				b_top = bp + SIZEOF(blk_hdr);
			else
				b_top = bp + ((blk_hdr_ptr_t) bp)->bsiz;
			if (CLI_NEGATED != head && !patch_is_fdmp)
			{	memcpy(util_buff, "Block ", 6);
				util_len = 6;
				util_len += i2hex_nofill(blk, &util_buff[util_len], 8);
				memcpy(&util_buff[util_len], "   Size ", 8);
				util_len += 8;
				util_len += i2hex_nofill(((blk_hdr_ptr_t)bp)->bsiz, &util_buff[util_len], 8);
				memcpy(&util_buff[util_len], "   Level !UL   TN ", 18);
				util_len += 18;
				util_len += i2hexl_nofill(((blk_hdr_ptr_t)bp)->tn, &util_buff[util_len], 16);
				memcpy(&util_buff[util_len], " ", 1);
				util_len++;
				ondsk_blkver = (!is_mm ? cr->ondsk_blkver : GDSV6);
				len = STRLEN(gtm_dbversion_table[ondsk_blkver]);
				memcpy(&util_buff[util_len], gtm_dbversion_table[ondsk_blkver], len);
				util_len += len;
				memcpy(&util_buff[util_len], "!/", 2);
				util_len += 2;
				util_buff[util_len] = 0;
				util_out_print((caddr_t)util_buff, TRUE, ((blk_hdr_ptr_t) bp)->levl );
			}
			rp = bp + SIZEOF(blk_hdr);
			if (CLI_PRESENT != head && (!patch_is_fdmp || ((blk_hdr_ptr_t) bp)->levl == 0))
			{
				while (!util_interrupt && (rp = dump_record(rp, blk, bp, b_top)))
					patch_rec_counter += 1;
			}
			if (util_interrupt)
			{
				DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
				rts_error(VARLSTCNT(1) ERR_CTRLC);
				break;
			}
			if (CLI_NEGATED == head)
				util_out_print(0, TRUE);
		} else if (!patch_is_fdmp)
		{
			if (!(bp = t_qread(blk, &dummy_int, &cr)))
			{
				DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
				rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
			}
			if (CLI_NEGATED != head)
			{

				if (bplmap == 0)
				{
					memcpy(util_buff, "Block ", 6);
					util_len = 6;
					util_len += i2hex_nofill(blk, &util_buff[util_len], 8);
					memcpy(&util_buff[util_len], "   Size ", 8);
					util_len += 8;
					util_len += i2hex_nofill(mapsize, &util_buff[util_len], 4);
					memcpy(&util_buff[util_len], "   Master Status: Cannot Determine (bplmap == 0)!/", 50);
					util_len += 50;
					util_buff[util_len] = 0;
					util_out_print((caddr_t)util_buff, TRUE );
				} else
				{
					mb = cs_addrs->bmm + blk / (8 * bplmap);
					lmap_num = blk / bplmap;
					mask = 1 << ( lmap_num - lmap_num / 8 * 8);
					free = 	mask & *mb;
					memcpy(util_buff, "Block ", 6);
					util_len = 6;
					util_len += i2hex_nofill(blk, &util_buff[util_len], 8);
					memcpy(&util_buff[util_len], "  Size ", 7);
					util_len += 7;
					util_len += i2hex_nofill(((blk_hdr_ptr_t)bp)->bsiz, &util_buff[util_len], 8);
					memcpy(&util_buff[util_len], "  Level !SB  TN ", 16);
					util_len += 16;
					util_len += i2hexl_nofill(((blk_hdr_ptr_t)bp)->tn, &util_buff[util_len], 16);
					memcpy(&util_buff[util_len], " ", 1);
					util_len++;
					ondsk_blkver = (!is_mm ? cr->ondsk_blkver : GDSV6);
					len = STRLEN(gtm_dbversion_table[ondsk_blkver]);
					memcpy(&util_buff[util_len], gtm_dbversion_table[ondsk_blkver], len);
					util_len += len;
					util_buff[util_len] = 0;
					util_out_print((caddr_t)util_buff, FALSE, ((blk_hdr_ptr_t) bp)->levl );
					util_len = 0;
					memcpy(&util_buff[util_len], "   Master Status: !AD!/",23);
					util_len = 23;
					util_buff[util_len] = 0;
					util_out_print((caddr_t)util_buff, TRUE, free ? 10 : 4, free ? "Free Space" : "Full");
				}
			}
			if (CLI_PRESENT != head)
			{
				util_out_print("           !_Low order                         High order", TRUE);

				lmap_num = 0;
				while (lmap_num < bplmap)
				{	memcpy(util_buff, "Block ", 6);
					util_len = 6;
					i2hex_blkfill(blk + lmap_num, &util_buff[util_len], 8);
					util_len += 8;
					memcpy(&util_buff[util_len], ":!_|  ", 6);
					util_len += 6;
					util_buff[util_len] = 0;
					util_out_print((caddr_t)util_buff, FALSE);
					for (iter1 = 0; iter1 < 4; iter1++)
					{
						for (iter2 = 0; iter2 < 8; iter2++)
						{
							mask = dse_lm_blk_free(lmap_num * BML_BITS_PER_BLK, bp + SIZEOF(blk_hdr));
							if (!mask)
								util_out_print("!AD", FALSE, 1, BUSY_CHAR);
							else if (BLK_FREE == mask)
								util_out_print("!AD", FALSE, 1, FREE_CHAR);
							else if (BLK_RECYCLED == mask)
								util_out_print("!AD", FALSE, 1, REUSABLE_CHAR);
							else {
								invalid_bitmap = TRUE;
								util_out_print("!AD", FALSE, 1, CORRUPT_CHAR);
							}
							if (++lmap_num >= bplmap)
								break;
						}
						util_out_print("  ", FALSE);
						if (lmap_num >= bplmap)
							break;
					}
					util_out_print("|", TRUE);
					if (util_interrupt)
					{
						DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs,
										gv_cur_region);
						rts_error(VARLSTCNT(1) ERR_CTRLC);
					}
				}
				util_out_print("!/'!AD' == BUSY  '!AD' == FREE  '!AD' == REUSABLE  '!AD' == CORRUPT!/",
					TRUE,1, BUSY_CHAR, 1, FREE_CHAR, 1, REUSABLE_CHAR, 1, CORRUPT_CHAR);
				if (invalid_bitmap)
				{
					DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs,
									gv_cur_region);
					rts_error(VARLSTCNT(1) ERR_BITMAPSBAD);
				}
			}
		}
		count--;
		if (count <= 0 || util_interrupt)
			break;
		blk++;
		if (blk >= cs_addrs->ti->total_blks)
			blk = 0;
	}
	patch_curr_blk = blk;
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	return TRUE;
}
