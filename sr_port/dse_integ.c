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
#include "gdsblk.h"
#include "gdsfhead.h"
#include "cli.h"
#include "util.h"
#include "dse.h"

/* Include prototypes */
#include "t_qread.h"
#include "cert_blk.h"

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF block_id		patch_curr_blk;

error_def(ERR_DSEBLKRDFAIL);

#define MAX_UTIL_LEN 40

void dse_integ(void)
{
	block_id	blk;
	char		util_buff[MAX_UTIL_LEN];
	sm_uc_ptr_t	bp;
	int4		dummy_int, nocrit_present;
	cache_rec_ptr_t	dummy_cr;
	int		util_len;
	boolean_t	was_crit, was_hold_onto_crit;

	if (CLI_PRESENT == cli_present("BLOCK"))
	{
		if (!cli_get_hex("BLOCK", (uint4 *)&blk))
			return;
		if (blk < 0 || blk >= cs_addrs->ti->total_blks)
		{
			util_out_print("Error: invalid block number.", TRUE);
			return;
		}
		patch_curr_blk = blk;
	}
	memcpy(util_buff, "!/Checking integrity of block ", 30);
	util_len = 30;
	util_len += i2hex_nofill(patch_curr_blk, (uchar_ptr_t)&util_buff[util_len], 8);
	memcpy(&util_buff[util_len], ":", 1);
	util_len += 1;
	util_buff[util_len] = 0;
	util_out_print(util_buff, TRUE);
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	if (!(bp = t_qread(patch_curr_blk, &dummy_int, &dummy_cr)))
		rts_error(VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	if (TRUE == cert_blk(gv_cur_region, patch_curr_blk, (blk_hdr_ptr_t)bp, 0, FALSE))
		util_out_print("!/  No errors detected.!/", TRUE);
	else
		util_out_print(NULL, TRUE);
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	return;
}
