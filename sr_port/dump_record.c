/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "min_max.h"          /* needed for init_root_gv.h */
#include "init_root_gv.h"
#include "util.h"
#include "dse.h"
#include "print_target.h"
#include "op.h"

GBLDEF bool             wide_out;
GBLDEF char             patch_comp_key[MAX_KEY_SZ + 1];
GBLDEF unsigned char    patch_comp_count;
GBLDEF int              patch_rec_counter;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF VSIG_ATOMIC_T	util_interrupt;
GBLREF mval             curr_gbl_root;
GBLREF int              patch_is_fdmp;
GBLREF int              patch_fdmp_recs;

#define MAX_UTIL_LEN 80

sm_uc_ptr_t  dump_record(sm_uc_ptr_t rp, block_id blk, sm_uc_ptr_t bp, sm_uc_ptr_t b_top)
{
	sm_uc_ptr_t	r_top, key_top, cptr0, cptr1;
	char		key_buf[MAX_KEY_SZ + 1], *temp_ptr, *temp_key, util_buff[MAX_UTIL_LEN];
	unsigned char	cc;
	short int	size;
	int4		util_len, head;
	int		buf_len;
	block_id	blk_id;

	if (rp >= b_top)
		return NULL;
	head = cli_present("HEADER");
	GET_SHORT(size, &((rec_hdr_ptr_t)rp)->rsiz);
	cc = ((rec_hdr_ptr_t)rp)->cmpc;
	if ((CLI_NEGATED != head) && !patch_is_fdmp)
	{
		memcpy(util_buff, "Rec:", sizeof("Rec:") - 1);
		util_len = sizeof("Rec:") - 1;
		util_len += i2hex_nofill(patch_rec_counter, (uchar_ptr_t)&util_buff[util_len], 4);
		memcpy(&util_buff[util_len], "  Blk ", sizeof("  Blk ") - 1);
		util_len += sizeof("  Blk ") - 1;
		util_len += i2hex_nofill(blk, (uchar_ptr_t)&util_buff[util_len], 8);
		memcpy(&util_buff[util_len], "  Off ", sizeof("  Off ") - 1);
		util_len += sizeof("  Off ") - 1;
		util_len += i2hex_nofill(rp - bp, (uchar_ptr_t)&util_buff[util_len], 4);
		memcpy(&util_buff[util_len], "  Size ", sizeof("  Size ") - 1);
		util_len += sizeof("  Size ") - 1;
		util_len += i2hex_nofill(size, (uchar_ptr_t)&util_buff[util_len], 4);
		memcpy(&util_buff[util_len], "  Cmpc ", sizeof("  Cmpc ") - 1);
		util_len += sizeof("  Cmpc ") - 1;
		util_len += i2hex_nofill(cc, (uchar_ptr_t)&util_buff[util_len], 2);
		memcpy(&util_buff[util_len], "  ", sizeof("  ") - 1);
		util_len += sizeof("  ") - 1;
		util_buff[util_len] = 0;
		util_out_print(util_buff, FALSE);
	}
	r_top = rp + size;
	if (r_top > b_top)
		r_top = b_top;
	else  if (r_top < rp + sizeof(rec_hdr))
		r_top = rp + sizeof(rec_hdr);
	if (cc > patch_comp_count)
		cc = patch_comp_count;
	if (((blk_hdr_ptr_t)bp)->levl)
		key_top = r_top - sizeof(block_id);
	else
	{
		for (key_top = rp + sizeof(rec_hdr);  key_top < r_top;)
			if (!*key_top++ && !*key_top++)
				break;
	}
	size = key_top - rp - sizeof(rec_hdr);
	if (size < 0)
		size = 0;
	else  if (size > sizeof(patch_comp_key) - 2)
		size = sizeof(patch_comp_key) - 2;
	memcpy(&patch_comp_key[cc], rp + sizeof(rec_hdr), size);
	patch_comp_count = cc + size;
	patch_comp_key[patch_comp_count] = patch_comp_key[patch_comp_count + 1] = 0;
	if (patch_is_fdmp)
	{
		if (dse_fdmp(key_top, r_top - key_top))
			patch_fdmp_recs++;
	} else
	{
		if (r_top - sizeof(block_id) >= key_top)
		{
			GET_LONG(blk_id, key_top);
			if ((((blk_hdr_ptr_t)bp)->levl) || (blk_id <= cs_addrs->ti->total_blks))
			{
				memcpy(util_buff, "Ptr ", sizeof("Ptr ") - 1);
				util_len = sizeof("Ptr ") - 1;
				util_len += i2hex_nofill(blk_id, (uchar_ptr_t)&util_buff[util_len], sizeof(blk_id) * 2);
				memcpy(&util_buff[util_len], "  ", sizeof("  ") - 1);
				util_len += sizeof("  ") - 1;
				util_buff[util_len] = 0;
				util_out_print(util_buff, FALSE);
			}
		}
		util_out_print("Key ", FALSE);
		if (r_top == b_top
			&& ((blk_hdr_ptr_t)bp)->levl && !((rec_hdr_ptr_t)rp)->cmpc
			&& r_top - rp == sizeof(rec_hdr) + sizeof(block_id))
				util_out_print("*", FALSE);
		else  if (patch_comp_key[0])
		{
			util_out_print("^", FALSE);
			RETRIEVE_ROOT_VAL(patch_comp_key, key_buf, temp_ptr, temp_key, buf_len);
			INIT_ROOT_GVT(key_buf, buf_len, curr_gbl_root);
		}
		print_target((uchar_ptr_t)patch_comp_key);
		util_out_print(0, TRUE);
		if (CLI_PRESENT != head)
		{
			for (cptr0 = rp;  cptr0 < r_top;  cptr0 += 20)
			{
				if (util_interrupt)
				{
					/* return, rather than signal ERR_CTRLC so
					 * that the calling routine can deal with
					 * that signal and do the appropriate
					 * cleanup.
					 */
					return NULL;
				}

				util_len = 8;
				i2hex_blkfill(cptr0 - bp, (uchar_ptr_t)util_buff, 8);
				memcpy(&util_buff[util_len], " : |", sizeof(" : |") - 1 );
				util_len += sizeof(" : |") - 1;
				util_buff[util_len] = 0;
				util_out_print(util_buff, FALSE);
				for (cptr1 = cptr0;  cptr1 < (cptr0 + 20);  cptr1++)
				{
					if (wide_out)
					{
						if (cptr1 < r_top)
						{
							i2hex_blkfill(*(sm_uc_ptr_t)cptr1, (uchar_ptr_t)util_buff, 4);
							util_buff[4] = 0;
							util_out_print(util_buff, FALSE);
						} else
							util_out_print("    ", FALSE);
					} else
					{
						if (cptr1 < r_top)
						{
							i2hex_blkfill(*(sm_uc_ptr_t)cptr1, (uchar_ptr_t)util_buff, 3);
							util_buff[3] = 0;
							util_out_print(util_buff, FALSE);
						} else
							util_out_print("   ", FALSE);
					}
				}
				if (wide_out)
				{
					util_out_print("|    |", FALSE);
					for (cptr1 = cptr0;  cptr1 < (cptr0 + 20);  cptr1++)
					{
						if (cptr1 < r_top)
						{
							if (PRINTABLE(*(sm_uc_ptr_t)cptr1))
								util_out_print("!AD", FALSE, 1, cptr1);
							else
								util_out_print(".", FALSE);
						} else
							util_out_print(" ", FALSE);
					}
				} else
				{	util_out_print("|", TRUE);
					util_out_print("           |", FALSE);
					for (cptr1 = cptr0;  cptr1 < (cptr0 + 20);  cptr1++)
					{
						if (cptr1 < r_top)
						{
							if (PRINTABLE(*(sm_uc_ptr_t)cptr1))
								util_out_print("  !AD", FALSE, 1, cptr1);
							else
								util_out_print("  .", FALSE);
						}
						else
							util_out_print("   ", FALSE);
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
