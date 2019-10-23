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

#include "mdef.h"

#include <signal.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "cli.h"
#include "dse.h"
#include "util.h"
#include "skan_offset.h"
#include "skan_rnum.h"

/* Include prototypes */
#include "t_qread.h"

GBLREF gd_region	*gv_cur_region;
GBLREF int		patch_is_fdmp, patch_rec_counter;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF VSIG_ATOMIC_T	util_interrupt;

error_def(ERR_CTRLC);
error_def(ERR_DSEBLKRDFAIL);

boolean_t dse_r_dmp(void)
{
	block_id	blk;
	boolean_t	was_crit, was_hold_onto_crit;
	cache_rec_ptr_t	dummy_cr;
	int4		count, dummy_int, nocrit_present;
	sm_uc_ptr_t	bp, b_top, rp;

	if (BADDSEBLK == (blk = dse_getblk("BLOCK", DSENOBML, DSEBLKCUR)))		/* WARNING: assignment */
		return FALSE;
	if (CLI_PRESENT == cli_present("COUNT"))
	{
		if (!cli_get_hex("COUNT", (uint4 *)&count))
			return FALSE;
	} else
		count = 1;
	was_crit = cs_addrs->now_crit;
	nocrit_present = (CLI_NEGATED == cli_present("CRIT"));
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	if (!(bp = t_qread(blk, &dummy_int, &dummy_cr)))
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DSEBLKRDFAIL);
	if (((blk_hdr_ptr_t) bp)->bsiz > cs_addrs->hdr->blk_size)
		b_top = bp + cs_addrs->hdr->blk_size;
	else if (((blk_hdr_ptr_t) bp)->bsiz < SIZEOF(blk_hdr))
		b_top = bp + SIZEOF(blk_hdr);
	else
		b_top = bp + ((blk_hdr_ptr_t) bp)->bsiz;
	if (((blk_hdr_ptr_t) bp)->levl && patch_is_fdmp)
	{
		DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
		util_out_print("Error:  cannot perform GLO/ZWR dump on index block.", TRUE);
		return FALSE;
	}
	if (CLI_PRESENT == cli_present("RECORD"))
	{
		if (!(rp = skan_rnum (bp, FALSE)))
		{
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
			return FALSE;
		}
	} else if (!(rp = skan_offset (bp, FALSE)))
	{
		DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
		return FALSE;
	}
	util_out_print(0, TRUE);
	for ( ; 0 < count; count--)
	{
		if (util_interrupt || !(rp = dump_record(rp, blk, bp, b_top)))
			break;
		patch_rec_counter += 1;
	}
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, cs_addrs, gv_cur_region);
	if (util_interrupt)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CTRLC);
	else if (CLI_NEGATED == cli_present("HEADER"))
		util_out_print(0, TRUE);
	return TRUE;
}
