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

#include "gtmctype.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "cli.h"
#include "copy.h"
#include "min_max.h"		/* needed for init_root_gv.h */
#include "init_root_gv.h"
#include "util.h"
#include "dse.h"
#include "print_target.h"
#include "op.h"

#ifdef GTM_TRIGGER
#include "hashtab_mname.h"
#include <rtnhdr.h>		/* needed for gv_trigger.h */
#include "gv_trigger.h"		/* needed for INIT_ROOT_GVT */
#include "targ_alloc.h"
#endif
#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif


GBLDEF bool             wide_out;
GBLDEF char             patch_comp_key[MAX_KEY_SZ + 1];
GBLDEF unsigned short   patch_comp_count;
GBLDEF int              patch_rec_counter;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF VSIG_ATOMIC_T	util_interrupt;
GBLREF mval             curr_gbl_root;
GBLREF int              patch_is_fdmp;
GBLREF int              patch_fdmp_recs;
GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;

LITREF	mval		literal_hasht;

#define MAX_UTIL_LEN		80
#define	NUM_BYTES_PER_LINE	20

sm_uc_ptr_t  dump_record(sm_uc_ptr_t rp, block_id blk, sm_uc_ptr_t bp, sm_uc_ptr_t b_top)
{
	sm_uc_ptr_t	r_top, key_top, cptr0, cptr1, cptr_top, cptr_base = NULL, cptr_next = NULL;
	char		key_buf[MAX_KEY_SZ + 1], *temp_ptr, *temp_key, util_buff[MAX_UTIL_LEN];
	char		*prefix_str, *space_str, *dot_str, *format_str;
	unsigned short	cc;
	int		tmp_cmpc;
	unsigned short 	size;
	int4		util_len, head;
	uint4 		ch;
	int		buf_len, field_width,fastate, chwidth = 0;
        ssize_t   	chlen;
	block_id	blk_id;
	boolean_t	rechdr_displayed = FALSE;
	sgmnt_addrs	*csa;

	if (rp >= b_top)
		return NULL;
	head = cli_present("HEADER");
	GET_SHORT(size, &((rec_hdr_ptr_t)rp)->rsiz);
	cc = EVAL_CMPC((rec_hdr_ptr_t)rp);
	if ((CLI_NEGATED != head) && !patch_is_fdmp)
	{
		MEMCPY_LIT(util_buff, "Rec:");
		util_len = SIZEOF("Rec:") - 1;
		util_len += i2hex_nofill(patch_rec_counter, (uchar_ptr_t)&util_buff[util_len], 4);
		MEMCPY_LIT(&util_buff[util_len], "  Blk ");
		util_len += SIZEOF("  Blk ") - 1;
		util_len += i2hex_nofill(blk, (uchar_ptr_t)&util_buff[util_len], 8);
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
	else  if (r_top < rp + SIZEOF(rec_hdr))
		r_top = rp + SIZEOF(rec_hdr);
	if (cc > patch_comp_count)
		cc = patch_comp_count;
	if (((blk_hdr_ptr_t)bp)->levl)
		key_top = r_top - SIZEOF(block_id);
	else
	{
		for (key_top = rp + SIZEOF(rec_hdr);  key_top < r_top;)
			if (!*key_top++ && !*key_top++)
				break;
	}
	size = key_top - rp - SIZEOF(rec_hdr);
	if (size > SIZEOF(patch_comp_key) - 2 - cc)
		size = SIZEOF(patch_comp_key) - 2 - cc;
	memcpy(&patch_comp_key[cc], rp + SIZEOF(rec_hdr), size);
	patch_comp_count = cc + size;
	patch_comp_key[patch_comp_count] = patch_comp_key[patch_comp_count + 1] = 0;
	if (patch_is_fdmp)
	{
		if (dse_fdmp(key_top, (int)(r_top - key_top)))
			patch_fdmp_recs++;
	} else
	{
		if (r_top - SIZEOF(block_id) >= key_top)
		{
			GET_LONG(blk_id, key_top);
			if ((((blk_hdr_ptr_t)bp)->levl) || (blk_id <= cs_addrs->ti->total_blks))
			{
				MEMCPY_LIT(util_buff, "Ptr ");
				util_len = SIZEOF("Ptr ") - 1;
				util_len += i2hex_nofill(blk_id, (uchar_ptr_t)&util_buff[util_len], SIZEOF(blk_id) * 2);
				MEMCPY_LIT(&util_buff[util_len], "  ");
				util_len += SIZEOF("  ") - 1;
				util_buff[util_len] = 0;
				util_out_print(util_buff, FALSE);
			}
		}
		util_out_print("Key ", FALSE);
		if (r_top == b_top
			&& ((blk_hdr_ptr_t)bp)->levl && !EVAL_CMPC((rec_hdr_ptr_t)rp)
			&& r_top - rp == SIZEOF(rec_hdr) + SIZEOF(block_id))
				util_out_print("*", FALSE);
		else  if (patch_comp_key[0])
		{
			util_out_print("^", FALSE);
			csa = cs_addrs;
			RETRIEVE_ROOT_VAL(patch_comp_key, key_buf, temp_ptr, temp_key, buf_len);
			INIT_ROOT_GVT(key_buf, buf_len, curr_gbl_root);
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
				{ /* return, rather than signal ERR_CTRLC so that the calling routine
				     can deal with that signal and do the appropriate cleanup */
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
#ifdef UNICODE_SUPPORTED
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
						util_out_print(dot_str, FALSE);
						if (--chlen <= 0)
							fastate = 0;
						break;

					case 2: /* printable multi-byte characters */
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
