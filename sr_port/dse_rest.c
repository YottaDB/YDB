/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "init_root_gv.h"
#include "filestruct.h"
#include "jnl.h"
#include "util.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_end.h"
#include "t_begin_crit.h"
#include "gvcst_blk_build.h"
#include "t_abort.h"

#define MAX_UTIL_LEN 80

GBLREF char		*update_array, *update_array_ptr;
GBLREF uint4		update_array_size;
GBLREF srch_hist	dummy_hist;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr		*gd_header;
GBLREF block_id		patch_curr_blk;
GBLREF save_strct	patch_save_set[PATCH_SAVE_SIZE];
GBLREF unsigned short int patch_save_count;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF gd_addr		*original_header;
GBLREF cw_set_element   cw_set[];

void dse_rest(void)
{
	block_id	to, from;
	gd_region	*region;
	int		i, util_len;
	uchar_ptr_t	lbp;
	char		util_buff[MAX_UTIL_LEN], rn[MAX_RN_LEN + 1];
	blk_segment	*bs1, *bs_ptr;
	int4		blk_seg_cnt, blk_size;
	unsigned short	rn_len;
	uint4		version;
	gd_addr		*temp_gdaddr;
	gd_binding	*map;
	boolean_t	found;
	srch_blk_status	blkhist;

	error_def(ERR_DBRDONLY);
	error_def(ERR_DSEBLKRDFAIL);
	error_def(ERR_DSEFAIL);

	if (gv_cur_region->read_only)
		rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	if (cli_present("VERSION") != CLI_PRESENT)
	{
		util_out_print("Error:  save version number must be specified.", TRUE);
		return;
	}
	if (!cli_get_int("VERSION", (int4 *)&version))
		return;
	if (cli_present("BLOCK") == CLI_PRESENT)
	{
		if (!cli_get_hex("BLOCK", (uint4 *)&to))
			return;
		if (to < 0 || to >= cs_addrs->ti->total_blks)
		{
			util_out_print("Error: invalid block number.", TRUE);
			return;
		}
		patch_curr_blk = to;
	} else
		to = patch_curr_blk;
	if (cli_present("FROM") == CLI_PRESENT)
	{
		if (!cli_get_hex("FROM", (uint4 *)&from))
	 		return;
	 	if (from < 0 || from >= cs_addrs->ti->total_blks)
	 	{
	 		util_out_print("Error: invalid block number.", TRUE);
	 		return;
	 	}
	} else
	 	from = to;
	if (cli_present("REGION") == CLI_PRESENT)
	{

		rn_len = SIZEOF(rn);
		if (!cli_get_str("REGION", rn, &rn_len))
			return;
		for (i = rn_len; i < MAX_RN_LEN + 1; i++)
			rn[i] = 0;
		found = FALSE;

		temp_gdaddr = gd_header;
		gd_header = original_header;

		for (i=0, region = gd_header->regions; i < gd_header->n_regions ;i++, region++)
			if (found = !memcmp(&region->rname[0], &rn[0], MAX_RN_LEN))
				break;
		GET_SAVED_GDADDR(gd_header, temp_gdaddr, map, gv_cur_region);
		if (!found)
		{
			util_out_print("Error:  region not found.", TRUE);
	 		return;
		}
		if (!region->open)
		{
			util_out_print("Error:  that region was not opened because it is not bound to any namespace.", TRUE);
			return;
		}
	} else
	 	region = gv_cur_region;
	found = FALSE;
	for (i = 0; i < patch_save_count; i++)
	 	if (patch_save_set[i].blk == from && patch_save_set[i].region == region
	 		&& (found = version == patch_save_set[i].ver))
	 		break;
	if (!found)
	{
	 	util_out_print("Error:  no such version.", TRUE);
	 	return;
	}
	memcpy(util_buff, "!/Restoring block ", 18);
	util_len = 18;
	util_len += i2hex_nofill(to, (uchar_ptr_t)&util_buff[util_len], 8);
	memcpy(&util_buff[util_len]," from version !UL", 17);
	util_len += 17;
	util_buff[util_len] = 0;
	util_out_print(util_buff,FALSE,version);
	if (to != from)
	{
		memcpy(util_buff, " of block ", 10);
		util_len = 10;
		util_len += i2hex_nofill(from, (uchar_ptr_t)&util_buff[util_len], 8);
		util_buff[util_len] = 0;
		util_out_print(util_buff, FALSE);
	}
	if (region != gv_cur_region)
		util_out_print(" in region !AD", FALSE, LEN_AND_STR(rn));
	util_out_print("!/",TRUE);
	if (to > cs_addrs->ti->total_blks)
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	t_begin_crit(ERR_DSEFAIL);
	blk_size = cs_addrs->hdr->blk_size;
	blkhist.blk_num = to;
	if (!(blkhist.buffaddr = t_qread(blkhist.blk_num, &blkhist.cycle, &blkhist.cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	lbp = (uchar_ptr_t)patch_save_set[i].bp;

	BLK_INIT(bs_ptr, bs1);
	BLK_SEG(bs_ptr, (uchar_ptr_t)lbp + SIZEOF(blk_hdr), (int)((blk_hdr_ptr_t)lbp)->bsiz - SIZEOF(blk_hdr));
	if (!BLK_FINI(bs_ptr, bs1))
	{
		util_out_print("Error: bad blk build.", TRUE);
		t_abort(gv_cur_region, cs_addrs);
		return;
	}
	t_write(&blkhist, (unsigned char *)bs1, 0, 0, ((blk_hdr_ptr_t)lbp)->levl, TRUE, FALSE, GDS_WRITE_KILLTN);
	BUILD_AIMG_IF_JNL_ENABLED(cs_data, cs_addrs->ti->curr_tn);
	t_end(&dummy_hist, NULL, TN_NOT_SPECIFIED);
	return;
}
