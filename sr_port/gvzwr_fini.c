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

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "zshow.h"
#include "zwrite.h"
#include "error.h"
#include "op.h"
#include "change_reg.h"
#include "patcode.h"
#include "sgnl.h"
#include "gvzwrite_clnup.h"
#include "mvalconv.h"
#include "gtmimagename.h"

GBLDEF zshow_out	*zwr_output;

GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gvzwrite_datablk	*gvzwrite_block;
GBLREF gd_addr		*gd_header;
GBLREF bool		undef_inhibit;

error_def(ERR_GVNAKED);

void gvzwr_fini(zshow_out *out, int pat)
{
	char 		m[SIZEOF(mident_fixed)];
	mval 		local, data;
	gv_key		*old;
	gvnh_reg_t	*gvnh_reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!gv_currkey)
		gvinit();
	ESTABLISH(gvzwrite_ch);
	zwr_output = out;
	assert(INVALID_GV_TARGET == reset_gv_target);
	reset_gv_target = gv_target;
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	gvzwrite_block->gd_reg = gv_cur_region;
	gvzwrite_block->old_targ = (unsigned char *)gv_target;
	old = (gv_key *)malloc(SIZEOF(gv_key) + gv_currkey->end);
	gvzwrite_block->old_key = (unsigned char *)old;
	memcpy(gvzwrite_block->old_key, gv_currkey, SIZEOF(gv_key) + gv_currkey->end);
	gvzwrite_block->gv_last_subsc_null = TREF(gv_last_subsc_null);
	gvzwrite_block->gv_some_subsc_null = TREF(gv_some_subsc_null);
	if (!pat)
	{
		local = *gvzwrite_block->pat;
		if (local.str.len)  /* New reference. Will get new gv_target.. */
		{
			gv_target = NULL;
			gv_currkey->base[0] = '\0';
			op_gvname(VARLSTCNT(1) &local);
 			op_gvdata(&data);
			if (!(MV_FORCE_INTD(&data)))
			{
				if (!undef_inhibit)
					sgnl_gvundef();
			} else
			{
				gvzwrite_block->fixed = (gvzwrite_block->fixed ? TRUE : FALSE);
				gvzwr_var(MV_FORCE_INTD(&data), 0);
			}
		} else	/* Old (naked) reference. Keep previous gv_target reference */
		{
			if (gv_currkey->prev == 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GVNAKED);
			gv_currkey->end = gv_currkey->prev;
			gv_currkey->base[gv_currkey->end] = 0;
			gv_currkey->prev = 0;
			/* If gvnh_reg corresponds to a spanning global, then determine
			 * gv_cur_region/gv_target/gd_targ_* variables based on updated gv_currkey.
			 */
			gvnh_reg = TREF(gd_targ_gvnh_reg);	/* set by op_gvname in previous call */
			GV_BIND_SUBSNAME_FROM_GVNH_REG_IF_GVSPAN(gvnh_reg, gd_header, gv_currkey);
			op_gvdata(&data);
			if (!(MV_FORCE_INTD(&data)))
			{
				if (!undef_inhibit)
					sgnl_gvundef();
			} else
			{
				gvzwrite_block->fixed = (gvzwrite_block->fixed ? TRUE : FALSE);
				gvzwr_var((int4)MV_FORCE_INTD(&data), 0);
			}
		}
	} else
	{
		gv_target = NULL;
		gv_currkey->base[0] = '\0';
		local.mvtype = MV_STR;
		local.str.addr = &m[0];
		local.str.len = 1;
		m[0] = '%';

		gvzwrite_block->fixed = FALSE;
		for (; ;)
		{
			op_gvname(VARLSTCNT(1) &local);
			if (do_pattern(&local, gvzwrite_block->pat))
			{
				op_gvdata(&data);
				if ((MV_FORCE_INTD(&data)))
				{
					gvzwr_var((int4)MV_FORCE_INTD(&data), 0);
				}
			}
			op_gvorder(&local);
			if (local.str.len)
			{
				assert(local.str.len <= MAX_MIDENT_LEN + 1);
				local.str.addr++;
				local.str.len--;
				memcpy(&m[0], local.str.addr, local.str.len);
				local.str.addr = &m[0];
			} else
				break;
		}
	}
	gvzwrite_clnup();	/* this routine is called by gvzwrite_ch() too */
	REVERT;
	return;
}
