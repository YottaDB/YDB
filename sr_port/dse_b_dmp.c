/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gtm_fcntl.h"
#include "gtm_stat.h"

#include <sys/mman.h>

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
#include "eintr_wrappers.h"
#include "filestruct.h"

/* Include prototypes */
#include "t_qread.h"

#define REUSABLE_CHAR	":"
#define FREE_CHAR	DOT_CHAR
#define BUSY_CHAR	"X"
#define CORRUPT_CHAR	"?"

GBLREF gd_region	*gv_cur_region;
GBLREF int		patch_is_fdmp, patch_rec_counter;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF VSIG_ATOMIC_T	util_interrupt;

LITREF char		*gtm_dbversion_table[];

error_def(ERR_BITMAPSBAD);
error_def(ERR_CTRLC);
error_def(ERR_DSEBLKRDFAIL);

#ifdef DEBUG
/* Debug GT.M versions support the ability to decode a block image. This capability is useful to
 * display PBLKs from journal extracts. See test commit PBLK2 for more information on how to use
 * this debug-only feature. */
#define CAN_USE_IMAGE
#endif

boolean_t dse_b_dmp(void)
{
	cache_rec_ptr_t	cr;
	block_id	blk, count, lmap_num;
<<<<<<< HEAD
	boolean_t	bm_free, invalid_bitmap = FALSE, was_crit, was_hold_onto_crit;
=======
	boolean_t	bm_free, invalid_bitmap = FALSE, is_mm, was_crit, was_hold_onto_crit = FALSE;
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
	enum db_ver	ondsk_blkver;
#	ifndef BLK_NUM_64BIT
	gtm_int8	count2;
#	endif
	int4		bplmap, dummy_int, head, iter1, iter2, len, mapsize, nocrit_present, util_len, lmap_indx, mask2;
	sm_uc_ptr_t	bp = NULL, b_top, mb, rp;
	unsigned char	mask, util_buff[MAX_UTIL_LEN];
	char		image_fn[MAX_FN_LEN + 1];
	unsigned short	image_fn_len = SIZEOF(image_fn);
	int		image_fd, image_status;
	struct stat     image_stat_buf;
	boolean_t	use_image = FALSE;

	head = cli_present("HEADER");
	if (BADDSEBLK == (blk = dse_getblk("BLOCK", DSEBMLOK, DSEBLKCUR)))		/* WARNING: assignment */
		return FALSE;
	if (CLI_PRESENT == cli_present("COUNT"))
	{
#		ifdef BLK_NUM_64BIT
		if (!cli_get_hex64("COUNT", (gtm_uint8 *)&count))
			return FALSE;
#		else
		if (!cli_get_hex64("COUNT", (gtm_uint8 *)&count2))
			return FALSE;
		else
		{
			assert(count2 == (int4)count2);
			count = (int4)count2;
		}
#		endif
		if (count < 1)
			return FALSE;
	} else
		count = 1;
#ifdef CAN_USE_IMAGE
	if (CLI_PRESENT == cli_present("IMAGE"))
	{
		if (FALSE == cli_get_str("IMAGE", image_fn, &image_fn_len))
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEBLKRDFAIL);
		image_fn[image_fn_len] = '\0';
		image_fd = OPEN(image_fn, O_RDONLY);
		if (FD_INVALID == image_fd)
		{
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(13) ERR_SYSCALL, 5, LEN_AND_LIT("open"), CALLFROM,
				errno, 0, ERR_TEXT, 2, image_fn_len, image_fn);
		}
		FSTAT_FILE(image_fd, &image_stat_buf, image_status);
		if (-1 == image_status)
		{
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(13) ERR_SYSCALL, 5, LEN_AND_LIT("fstat"), CALLFROM,
				errno, 0, ERR_TEXT, 2, image_fn_len, image_fn);
		}
		bp = (sm_uc_ptr_t)mmap(NULL, image_stat_buf.st_size, PROT_READ, MAP_PRIVATE, image_fd, 0);
		if (NULL == bp)
		{
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(13) ERR_SYSCALL, 5, LEN_AND_LIT("mmap"), CALLFROM,
				errno, 0, ERR_TEXT, 2, image_fn_len, image_fn);
		}
		util_out_print(0, TRUE);
		use_image = TRUE;
	}
#endif
	util_out_print(0, TRUE);
	bplmap = cs_addrs->hdr->bplmap;
	mapsize = BM_SIZE(bplmap);
	patch_rec_counter = 1;
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	for ( ; ; )
	{
		if (!use_image && !(bp = t_qread(blk, &dummy_int, &cr)))
		{
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEBLKRDFAIL);
		}
#ifdef CAN_USE_IMAGE
		else if (use_image)
			util_out_print("Dumping contents of !AD", TRUE, image_fn_len, image_fn);
#endif
		assert(bp);
		if (((blk / bplmap) * bplmap) != blk)
		{
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
			{
				memcpy(util_buff, "Block ", 6);
				util_len = 6;
				util_len += i2hexl_nofill(blk, &util_buff[util_len], MAX_HEX_INT8);
				memcpy(&util_buff[util_len], "   Size ", 8);
				util_len += 8;
				util_len += i2hex_nofill(((blk_hdr_ptr_t)bp)->bsiz, &util_buff[util_len], MAX_HEX_INT);
				memcpy(&util_buff[util_len], "   Level !UL   TN ", 18);
				util_len += 18;
				util_len += i2hexl_nofill(((blk_hdr_ptr_t)bp)->tn, &util_buff[util_len], MAX_HEX_INT8);
				memcpy(&util_buff[util_len++], " ", 1);
#ifdef CAN_USE_IMAGE
				if (use_image)
					ondsk_blkver = ((blk_hdr_ptr_t)bp)->bver;
				else
#endif
				ondsk_blkver = (0 < ((blk_hdr_ptr_t)bp)->bsiz) ?
					((blk_hdr_ptr_t)bp)->bver : cs_addrs->hdr->desired_db_format;
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
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
				break;
			}
			if (CLI_NEGATED == head)
				util_out_print(0, TRUE);
		} else if (!patch_is_fdmp)
		{
			if (CLI_NEGATED != head)
			{
				if (0 == bplmap)
				{
					memcpy(util_buff, "Block ", 6);
					util_len = 6;
					util_len += i2hexl_nofill(blk, &util_buff[util_len], MAX_HEX_INT8);
					memcpy(&util_buff[util_len], "   Size ", 8);
					util_len += 8;
					/* Using MAX_HEX_SHORT for int value because to save line space
					 * since the value should always fit in 2-bytes
					 */
					util_len += i2hex_nofill(mapsize, &util_buff[util_len], MAX_HEX_SHORT);
					memcpy(&util_buff[util_len], "   Master Status: Cannot Determine (bplmap == 0)!/", 50);
					util_len += 50;
					util_buff[util_len] = 0;
					util_out_print((caddr_t)util_buff, TRUE );
				} else
				{
					assert(bp);
					mb = cs_addrs->bmm + (blk / (8 * bplmap));
					lmap_num = blk / bplmap;
					mask = 1 << (lmap_num - ((lmap_num / 8) * 8));
					bm_free = mask & *mb;
					memcpy(util_buff, "Block ", 6);
					util_len = 6;
					util_len += i2hexl_nofill(blk, &util_buff[util_len], MAX_HEX_INT8);
					memcpy(&util_buff[util_len], "  Size ", 7);
					util_len += 7;
					util_len += i2hex_nofill(((blk_hdr_ptr_t)bp)->bsiz, &util_buff[util_len], MAX_HEX_INT);
					memcpy(&util_buff[util_len], "  Level !SB  TN ", 16);
					util_len += 16;
					util_len += i2hexl_nofill(((blk_hdr_ptr_t)bp)->tn, &util_buff[util_len], MAX_HEX_INT8);
					memcpy(&util_buff[util_len], " ", 1);
					util_len++;
					len = STRLEN(gtm_dbversion_table[((blk_hdr_ptr_t)bp)->bver]);
					memcpy(&util_buff[util_len], gtm_dbversion_table[((blk_hdr_ptr_t)bp)->bver], len);
					util_len += len;
					util_buff[util_len] = 0;
					util_out_print((caddr_t)util_buff, FALSE, ((blk_hdr_ptr_t)bp)->levl );
					util_len = 0;
					memcpy(&util_buff[util_len], "   Master Status: !AD!/",23);
					util_len = 23;
					util_buff[util_len] = 0;
					util_out_print((caddr_t)util_buff, TRUE, bm_free ? 10 : 4, bm_free ? "Free Space" : "Full");
				}
			}
			if (CLI_PRESENT != head)
			{
				util_out_print("                       !_Low order                         High order", TRUE);
				lmap_indx = 0;
				while (lmap_indx < bplmap)
				{
					memcpy(util_buff, "Block ", 6);
					util_len = 6;
					i2hexl_blkfill(blk + lmap_indx, &util_buff[util_len], MAX_HEX_INT8);
					util_len += 16;
					memcpy(&util_buff[util_len], ":!_|  ", 6);
					util_len += 6;
					util_buff[util_len] = 0;
					util_out_print((caddr_t)util_buff, FALSE);
					for (iter1 = 0; iter1 < 4; iter1++)
					{
						for (iter2 = 0; iter2 < 8; iter2++)
						{
							assert(bp);
							mask2 = dse_lm_blk_free(lmap_indx, bp + SIZEOF(blk_hdr));
							if (!mask2)
								util_out_print("!AD", FALSE, 1, BUSY_CHAR);
							else if (BLK_FREE == mask2)
								util_out_print("!AD", FALSE, 1, FREE_CHAR);
							else if (BLK_RECYCLED == mask2)
								util_out_print("!AD", FALSE, 1, REUSABLE_CHAR);
							else {
								invalid_bitmap = TRUE;
								util_out_print("!AD", FALSE, 1, CORRUPT_CHAR);
							}
							if (++lmap_indx >= bplmap)
								break;
						}
						util_out_print("  ", FALSE);
						if (lmap_indx >= bplmap)
							break;
					}
					util_out_print("|", TRUE);
					if (util_interrupt)
					{
						DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs,
										gv_cur_region);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
					}
				}
				util_out_print("!/'!AD' == BUSY  '!AD' == FREE  '!AD' == REUSABLE  '!AD' == CORRUPT!/",
					TRUE,1, BUSY_CHAR, 1, FREE_CHAR, 1, REUSABLE_CHAR, 1, CORRUPT_CHAR);
				if (invalid_bitmap)
				{
					DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs,
									gv_cur_region);
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_BITMAPSBAD);
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
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	return TRUE;
}
