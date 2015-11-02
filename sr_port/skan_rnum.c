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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "copy.h"
#include "util.h"
#include "cli.h"
#include "skan_rnum.h"

#define MAX_UTIL_LEN	80

GBLREF block_id		patch_curr_blk;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF char		patch_comp_key[MAX_KEY_SZ + 1];
GBLREF unsigned short	patch_comp_count;
GBLREF int		patch_rec_counter;

sm_uc_ptr_t skan_rnum(sm_uc_ptr_t bp, bool over_run)
{
	char 		util_buff[MAX_UTIL_LEN];
	unsigned short	cc;
	int		tmp_cmpc;
	sm_uc_ptr_t 	b_top, rp, r_top, key_top;
	short int 	size, rec_size;
	int4 		record;

	if (((blk_hdr_ptr_t) bp)->bsiz > cs_addrs->hdr->blk_size)
		b_top = bp + cs_addrs->hdr->blk_size;
	else if (((blk_hdr_ptr_t) bp)->bsiz < SIZEOF(blk_hdr))
		b_top = bp + SIZEOF(blk_hdr);
	else
		b_top = bp + ((blk_hdr_ptr_t) bp)->bsiz;
	if (!cli_get_hex("RECORD", (uint4 *)&record))
		return 0;
	if (record < 1)
	{
		util_out_print("Error: record less than 1",TRUE);
		return 0;
	}

	patch_rec_counter = 1;
	rp = bp + SIZEOF(blk_hdr);
	patch_comp_key[0] = patch_comp_key[1] = 0;
	patch_comp_count = 0;
	for ( ; record > 1 && rp < b_top ;record--)
	{
		GET_SHORT(rec_size, &((rec_hdr_ptr_t)rp)->rsiz);
		if (rec_size < SIZEOF(rec_hdr))
			r_top = rp + SIZEOF(rec_hdr);
		else
			r_top = rp + rec_size;
		if (r_top >= b_top)
		{
			if (!over_run)
				break;
			r_top = b_top;
		}
		patch_rec_counter++;
		if (((blk_hdr_ptr_t) bp)->levl)
			key_top = r_top - SIZEOF(block_id);
		else
		{
			for (key_top = rp + SIZEOF(rec_hdr); key_top < r_top ; )
				if (!*key_top++ && !*key_top++)
					break;
		}
		EVAL_CMPC2((rec_hdr_ptr_t)rp, tmp_cmpc);
		if (tmp_cmpc > patch_comp_count)
			cc = patch_comp_count;
		else
			cc = tmp_cmpc;
		size = key_top - rp - SIZEOF(rec_hdr);
		if (size > SIZEOF(patch_comp_key) - 2 - cc)
			size = SIZEOF(patch_comp_key) - 2 - cc;
		if (size < 0)
			size = 0;
		memcpy(&patch_comp_key[cc], rp + SIZEOF(rec_hdr), size);
		patch_comp_count = cc + size;
		rp = r_top;
	}
	if (record > 1)
	{
		memcpy(util_buff, "Error: last record in blk is ", 29);
		util_buff[ i2hex_nofill(patch_rec_counter, (uchar_ptr_t)&util_buff[29], 8) + 29 ] = 0;
		util_out_print(&util_buff[0],TRUE);
		return 0;
	}

	return rp;
}
