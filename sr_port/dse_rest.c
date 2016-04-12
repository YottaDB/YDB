/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "gtm_string.h"
#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "gdscc.h"
#include "dse.h"
#include "cli.h"
#include "filestruct.h"
#include "jnl.h"
#include "util.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "process_deferred_stale.h"
#include "gvcst_blk_build.h"
#include "t_abort.h"
#include "gtmmsg.h"

#define MAX_UTIL_LEN 80

GBLREF block_id		patch_curr_blk;
GBLREF char		*update_array, *update_array_ptr;
GBLREF cw_set_element	cw_set[];
GBLREF gd_addr		*original_header;
GBLREF gd_region	*gv_cur_region;
GBLREF save_strct	patch_save_set[PATCH_SAVE_SIZE];
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF srch_hist	dummy_hist;
GBLREF uint4		patch_save_count, update_array_size;

error_def(ERR_AIMGBLKFAIL);
error_def(ERR_DBRDONLY);
error_def(ERR_DSEBLKRDFAIL);
error_def(ERR_DSEFAIL);
error_def(ERR_DSENOTOPEN);
error_def(ERR_NOREGION);

void dse_rest(void)
{
	blk_segment	*bs1, *bs_ptr;
	block_id	to, from;
	char		util_buff[MAX_UTIL_LEN], rn[MAX_RN_LEN + 1];
	gd_binding	*map;
	gd_region	*region;
	int		i, util_len;
	int4		blk_seg_cnt, blk_size;
	srch_blk_status	blkhist;
	uchar_ptr_t	lbp;
	uint4		found_index, version;
	unsigned short	rn_len;

	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	if (cli_get_int("VERSION", (int4 *)&version))
	{
		if (0 == version)
		{
			util_out_print("Error:  no such version.", TRUE);
			return;
		}
	} else
		version = 0;
	if (BADDSEBLK == (to = dse_getblk("BLOCK", DSEBMLOK, DSEBLKCUR)))			/* WARNING: assignment */
		return;
	if (CLI_PRESENT == cli_present("FROM"))
	{	/* don't use dse_getblk because we're working out of the save set, not the db */
		if (!cli_get_hex("FROM", (uint4 *)&from))
			from = patch_curr_blk;
	} else
	 	from = to;
	if (CLI_PRESENT == cli_present("REGION"))
	{
		rn_len = SIZEOF(rn);
		if (!cli_get_str("REGION", rn, &rn_len))
			return;
		for (i = 0; i < rn_len; i++)
			rn[i] = TOUPPER(rn[i]);	/* Region names are always upper-case ASCII and thoroughly NUL terminated */
		for ( ; i < ARRAYSIZE(rn); i++)
			rn[i] = 0;
		found_index = 0;
		for (i = 0, region = original_header->regions; i < original_header->n_regions ;i++, region++)
			if (found_index = !memcmp(&region->rname[0], &rn[0], MAX_RN_LEN))	/* WARNING: assignment */
				break;
		if (!found_index)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOREGION, 2, rn_len, rn);
			return;
		}
		if (!region->open)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DSENOTOPEN, 2, rn_len, rn);
			return;
		}
	} else
		region = gv_cur_region;
	found_index = 0;
	for (i = 0; i < patch_save_count; i++)
	{
		if ((patch_save_set[i].blk == from) && (patch_save_set[i].region == region))
		{
			if (version == patch_save_set[i].ver)
			{
				assert(version);
				found_index = i + 1;
				break;
			}
			if (!version)
			{
				if (found_index)
				{
					util_out_print("Error:  save version number must be specified.", TRUE);
					return;
				}
				found_index = i + 1;
			}
		}
	}
	if (0 == found_index)
	{
		if (version)
			util_out_print("Error: Version !UL of block !XL not found in set of saved blocks", TRUE, version, from);
		else
			util_out_print("Error: Block !XL not found in set of saved blocks", TRUE, from);
		return;
	}
	if (!version)
	{
		i = found_index - 1;
		version = patch_save_set[i].ver;
	}
	memcpy(util_buff, "!/Restoring block ", 18);
	util_len = 18;
	util_len += i2hex_nofill(to, (uchar_ptr_t)&util_buff[util_len], 8);
	memcpy(&util_buff[util_len], " from version !UL", 17);
	util_len += 17;
	util_buff[util_len] = 0;
	assert(ARRAYSIZE(util_buff) >= util_len);
	util_out_print(util_buff, FALSE, version);
	if (to != from)
	{
		memcpy(util_buff, " of block ", 10);
		util_len = 10;
		util_len += i2hex_nofill(from, (uchar_ptr_t)&util_buff[util_len], 8);
		util_buff[util_len] = 0;
		assert(ARRAYSIZE(util_buff) >= util_len);
		util_out_print(util_buff, FALSE);
	}
	if (region != gv_cur_region)
		util_out_print(" in region !AD", FALSE, LEN_AND_STR(rn));
	util_out_print("!/", TRUE);
	t_begin_crit(ERR_DSEFAIL);
	if (to >= cs_addrs->ti->total_blks)
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	blk_size = cs_addrs->hdr->blk_size;
	blkhist.blk_num = to;
	if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	lbp = (uchar_ptr_t)patch_save_set[i].bp;

	BLK_INIT(bs_ptr, bs1);
	BLK_SEG(bs_ptr, (uchar_ptr_t)lbp + SIZEOF(blk_hdr), (int)((blk_hdr_ptr_t)lbp)->bsiz - SIZEOF(blk_hdr));
	if (!BLK_FINI(bs_ptr, bs1))
	{
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_AIMGBLKFAIL, 3, to, DB_LEN_STR(gv_cur_region));
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	t_write(&blkhist, (unsigned char *)bs1, 0, 0, ((blk_hdr_ptr_t)lbp)->levl, TRUE, FALSE, GDS_WRITE_KILLTN);
	BUILD_AIMG_IF_JNL_ENABLED_AND_T_END_WITH_EFFECTIVE_TN(cs_addrs, cs_data, ((blk_hdr_ptr_t)lbp)->tn, &dummy_hist);
	return;
}
