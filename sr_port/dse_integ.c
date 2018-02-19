/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;

error_def(ERR_DSEBLKRDFAIL);

#define MAX_UTIL_LEN 40

void dse_integ(void)
{
	block_id	blk;
	boolean_t	was_crit, was_hold_onto_crit;
	cache_rec_ptr_t	dummy_cr;
	char		util_buff[MAX_UTIL_LEN];
	int		util_len;
	int4		dummy_int, nocrit_present;
	sm_uc_ptr_t	bp;
	unsigned char	*r_ptr;
	char		key_buff[MAX_KEY_SZ + 1];
	int		key_len;
	gv_namehead	*gvt = NULL;

	if (BADDSEBLK == (blk = dse_getblk("BLOCK", DSEBMLOK, DSEBLKCUR)))		/* WARNING: assignment */
		return;
	memcpy(util_buff, "!/Checking integrity of block ", 30);
	util_len = 30;
	util_len += i2hex_nofill(blk, (uchar_ptr_t)&util_buff[util_len], 8);
	memcpy(&util_buff[util_len], ":", 1);
	util_len += 1;
	util_buff[util_len] = 0;
	util_out_print(util_buff, TRUE);
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	if (!(bp = t_qread(blk, &dummy_int, &dummy_cr)))
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	if (0 == ((blk_hdr_ptr_t)bp)->levl)
	{
		r_ptr = (unsigned char *)((sm_uc_ptr_t)bp + SIZEOF(blk_hdr)) + SIZEOF(rec_hdr);
		for (key_len = 0; KEY_DELIMITER != *r_ptr; r_ptr++)
			key_buff[key_len++] = *r_ptr;;
		gvt = dse_find_gvt(gv_cur_region, (char *)key_buff, (key_len));
	}
	if (TRUE == cert_blk(gv_cur_region, blk, (blk_hdr_ptr_t)bp, 0, RTS_ERROR_ON_CERT_FAIL, gvt))
		util_out_print("!/  No errors detected.!/", TRUE);
	else
		util_out_print(NULL, TRUE);
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	return;
}
