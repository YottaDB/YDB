/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries.	*
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

#include "gtmctype.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "cli.h"
#include "copy.h"
#include "util.h"
#include "dse.h"
#include "print_target.h"
#include "op.h"
#include "gvcst_protos.h"

#ifdef GTM_TRIGGER
#include "hashtab_mname.h"
#include "targ_alloc.h"
#endif
#ifdef UTF8_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLDEF bool		wide_out;
GBLDEF char		patch_comp_key[MAX_KEY_SZ + 1];
GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLDEF int		patch_rec_counter;
GBLREF int		patch_is_fdmp;
GBLREF int		patch_fdmp_recs;
GBLREF sgmnt_addrs	*cs_addrs;
GBLDEF unsigned short	patch_comp_count;
GBLREF VSIG_ATOMIC_T	util_interrupt;

error_def(ERR_DSEINVALBLKID);

#define NUM_BYTES_PER_LINE	20

sm_uc_ptr_t dump_record(sm_uc_ptr_t rp, block_id blk, sm_uc_ptr_t bp, sm_uc_ptr_t b_top)
{
	block_id	blk_id;
	boolean_t	rechdr_displayed = FALSE, long_blk_id;
	char		key_buf[MAX_KEY_SZ + 1], *temp_ptr, *temp_key, util_buff[MAX_UTIL_LEN];
	char		*prefix_str, *space_str, *dot_str, *format_str;
	int		tmp_cmpc, buf_len, field_width, fastate, chwidth = 0;
	int4		util_len, head;
	long		blk_id_size;
	sm_uc_ptr_t	r_top, key_top, cptr0, cptr1, cptr_top, cptr_base = NULL, cptr_next = NULL;
	ssize_t		chlen = -1;
	uint4		ch;
	unsigned short	cc, size;

	if (rp >= b_top)
		return NULL;
	head = cli_present("HEADER");
	GET_SHORT(size, &((rec_hdr_ptr_t)rp)->rsiz);
	cc = EVAL_CMPC((rec_hdr_ptr_t)rp);
	if(((blk_hdr_ptr_t)bp)->bver > BLK_ID_32_VER) /* Check blk version to see if using 32 or 64 bit block_id */
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
	if ((CLI_NEGATED != head) && !patch_is_fdmp)
	{
		MEMCPY_LIT(util_buff, "Rec:");
		util_len = SIZEOF("Rec:") - 1;
		util_len += i2hex_nofill(patch_rec_counter, (uchar_ptr_t)&util_buff[util_len], 4);
		MEMCPY_LIT(&util_buff[util_len], "  Blk ");
		util_len += SIZEOF("  Blk ") - 1;
		util_len += i2hexl_nofill(blk, (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
		MEMCPY_LIT(&util_buff[util_len], "  Off ");
		util_len += SIZEOF("  Off ") - 1;
		util_len += i2hex_nofill((int)(rp - bp), (uchar_ptr_t)&util_buff[util_len], 4);
		MEMCPY_LIT(&util_buff[util_len], "  Size ");
		util_len += SIZEOF("  Size ") - 1;
		util_len += i2hex_nofill(size, (uchar_ptr_t)&util_buff[util_len], 4);
		MEMCPY_LIT(&util_buff[util_len], "  Cmpc ");
		util_len += SIZEOF("  Cmpc ") - 1;
		util_len += i2hex_nofill(cc, (uchar_ptr_t)&util_buff[util_len], 3);
		MEMCPY_LIT(&util_buff[util_len], "  ");
		util_len += SIZEOF("  ") - 1;
		util_buff[util_len] = 0;
		util_out_print(util_buff, FALSE);
	}
	r_top = rp + size;
	if (r_top > b_top)
		r_top = b_top;
	else if (r_top < rp + SIZEOF(rec_hdr))
		r_top = rp + SIZEOF(rec_hdr);
	if (cc > patch_comp_count)
		cc = patch_comp_count;
	if (((blk_hdr_ptr_t)bp)->levl)
		key_top = r_top - blk_id_size;
	else
	{
		for (key_top = rp + SIZEOF(rec_hdr); key_top < r_top; )
		{
			if (!*key_top++ && (key_top < r_top) && !*key_top++)
				break;
		}
	}
	size = key_top - rp - SIZEOF(rec_hdr);
	if (size > SIZEOF(patch_comp_key) - 2 - cc)
		size = SIZEOF(patch_comp_key) - 2 - cc;
	memcpy(&patch_comp_key[cc], rp + SIZEOF(rec_hdr), size);
	patch_comp_count = cc + size;
	patch_comp_key[patch_comp_count] = patch_comp_key[patch_comp_count + 1] = '\0';
	if (patch_is_fdmp)
	{
		if (dse_fdmp(key_top, (int)(r_top - key_top)))
			patch_fdmp_recs++;
	} else
	{
		if ((r_top - blk_id_size) >= key_top)
		{
			if(long_blk_id == TRUE)
#				ifdef BLK_NUM_64BIT
				GET_BLK_ID_64(blk_id, key_top);
#				else
				RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_DSEINVALBLKID);
#				endif
			else
				GET_BLK_ID_32(blk_id, key_top);
			if ((((blk_hdr_ptr_t)bp)->levl) || (((ublock_id)blk_id) <= cs_addrs->ti->total_blks))
			{
				MEMCPY_LIT(util_buff, "Ptr ");
				util_len = SIZEOF("Ptr ") - 1;
				util_len += i2hexl_nofill(blk_id, (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
				MEMCPY_LIT(&util_buff[util_len], "  ");
				util_len += SIZEOF("  ") - 1;
				util_buff[util_len] = 0;
				util_out_print(util_buff, FALSE);
			}
		}
		util_out_print("Key ", FALSE);
		if ((r_top == b_top)
			&& ((blk_hdr_ptr_t)bp)->levl && !EVAL_CMPC((rec_hdr_ptr_t)rp)
			&& ((r_top - rp) == (SIZEOF(rec_hdr) + blk_id_size)))
		{	/* *-key */
			assert('\0' == patch_comp_key[0]);
			assert('\0' == patch_comp_key[1]);
			util_out_print("*", FALSE);
			/* it is ok not to set gv_target in this case as "print_target" does not need it */
		} else if (patch_comp_key[0])
		{
			util_out_print("^", FALSE);
			temp_ptr = patch_comp_key;
			temp_key = key_buf;
			/* Copy from temp_ptr to temp_key until the first zero byte (copy that as well) */
			for (buf_len = 0; (*temp_key++ = *temp_ptr++); buf_len++)
				;
			*temp_key = KEY_DELIMITER;	/* Set the second zero byte */
			gv_target = dse_find_gvt(gv_cur_region, key_buf, buf_len); /* sets up gv_target */
			SET_GV_CURRKEY_FROM_GVT(gv_target);	/* gv_currkey is needed by GVCST_ROOT_SEARCH */
			GVCST_ROOT_SEARCH; /* sets up gv_target->collseq (needed by print_target --> gvsub2str) */
		}
		print_target((uchar_ptr_t)patch_comp_key);
		util_out_print(0, TRUE);
		if (CLI_PRESENT != head)
		{
			prefix_str = "           |";
			if (wide_out)
			{
				format_str = "   !AD";
				dot_str = "   .";
				space_str = "    ";
				field_width = 4;
			} else
			{
				format_str = "  !AD";
				dot_str = "  .";
				space_str = "   ";
				field_width = 3;
			}
			fastate = 0;
			for (cptr0 = rp;  cptr0 < r_top;  cptr0 += NUM_BYTES_PER_LINE)
			{
				if (util_interrupt)
				{/* return, rather than signal ERR_CTRLC so that the calling routine
				  * can deal with that signal and do the appropriate cleanup
				  */
					return NULL;
				}
				util_len = 8;
				i2hex_blkfill((int)(cptr0 - bp), (uchar_ptr_t)util_buff, 8);
				MEMCPY_LIT(&util_buff[util_len], " : |");
				util_len += SIZEOF(" : |") - 1;
				util_buff[util_len] = 0;
				util_out_print(util_buff, FALSE);
				/* Dump hexadecimal byte values */
				for (cptr1 = cptr0;  cptr1 < (cptr0 + NUM_BYTES_PER_LINE);  cptr1++)
				{
					if (cptr1 < r_top)
					{
						i2hex_blkfill(*(sm_uc_ptr_t)cptr1, (uchar_ptr_t)util_buff, field_width);
						util_buff[field_width] = 0;
						util_out_print(util_buff, FALSE);
					} else
						util_out_print(space_str, FALSE);
				}
				util_out_print("|", TRUE);
				util_out_print(prefix_str, FALSE);
				/* Display character/wide-character glyphs */
				for (cptr1 = cptr0, cptr_top = cptr0 + NUM_BYTES_PER_LINE;  cptr1 < cptr_top;  cptr1++)
				{
					if (!rechdr_displayed && (cptr1 == (rp + SIZEOF(rec_hdr))))
						rechdr_displayed = TRUE;
					assert(rechdr_displayed || (cptr1 < (rp + SIZEOF(rec_hdr))));
					assert(!rechdr_displayed || (cptr1 >= (rp + SIZEOF(rec_hdr))));
					switch (fastate)
					{
					case 0: /* prints single-byte characters or intepret
						   multi-byte characters */
						if (cptr1 >= r_top)
							util_out_print(space_str, FALSE);
						else if (!gtm_utf8_mode || IS_ASCII(*cptr1) || !rechdr_displayed)
						{ /* single-byte characters */
							if (PRINTABLE(*(sm_uc_ptr_t)cptr1))
								util_out_print(format_str, FALSE, 1, cptr1);
							else
								util_out_print(dot_str, FALSE);
						}
#ifdef UTF8_SUPPORTED
						else { /* multi-byte characters */
							cptr_next = UTF8_MBTOWC(cptr1, r_top, ch);
							chlen = cptr_next - cptr1;
							if (WEOF == ch || !U_ISPRINT(ch))
							{ /* illegal or non-printable characters */
								cptr1--;
								fastate = 1;
							} else
							{ /* multi-byte printable characters */
								cptr_base = cptr1;
								chwidth = UTF8_WCWIDTH(ch);
								assert(chwidth >= 0 && chwidth <= 2);
								cptr1--;
								fastate = 2;
							}
						}
#endif
						break;

					case 1: /* illegal or non-printable characters */
						assert(0 <= chlen);
						util_out_print(dot_str, FALSE);
						if (--chlen <= 0)
							fastate = 0;
						break;

					case 2: /* printable multi-byte characters */
						assert(0 <= chlen);
						if (chlen-- > 1) /* fill leading bytes with spaces */
							util_out_print(space_str, FALSE);
						else
						{
							util_out_print("!AD", FALSE, field_width - chwidth, space_str);
							if (0 < chwidth)
								util_out_print("!AD", FALSE, cptr_next - cptr_base, cptr_base);
							fastate = 0;
						}
						break;
					}
				}
				util_out_print("|", TRUE);
			}
		}
		if (CLI_NEGATED != head)
			util_out_print(0, TRUE);
	}
	return (r_top == b_top) ? NULL : r_top;
}
