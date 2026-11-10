/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023-2026 YottaDB LLC and/or its subsidiaries.	*
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
#include "gvt_inline.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */

GBLDEF zshow_out	*zwr_output;

GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gvzwrite_datablk	*gvzwrite_block;
GBLREF gd_addr		*gd_header;
GBLREF bool		undef_inhibit;
GBLREF uint4		dollar_tlevel;

error_def(ERR_GVNAKED);

/* Scan the subscripts of "gv_currkey": set "*last_null" if the last subscript is the null subscript, and "*some_null" if
 * another subscript is. A null subscript is 0xFF 0x00 with the default null collation and 0x01 0x00 with the standard null
 * collation, and no other subscript starts with either sequence.
 */
STATICFNDEF void gvzwr_scan_null_subsc(boolean_t *some_null, boolean_t *last_null)
{
	boolean_t	is_null, was_null;
	unsigned char	*ptr, *top;

	is_null = was_null = FALSE;
	top = &gv_currkey->base[gv_currkey->end];
	for (ptr = gv_currkey->base; KEY_DELIMITER != *ptr; ptr++)
		;	/* Skip the global name */
	for (ptr++; ptr < top; ptr++)
	{	/* "ptr" points to the first byte of a subscript */
		was_null |= is_null;
		is_null = (((STR_SUB_PREFIX == *ptr) || (SUBSCRIPT_STDCOL_NULL == *ptr)) && (KEY_DELIMITER == *(ptr + 1)));
		while (KEY_DELIMITER != *ptr)
			ptr++;
	}
	*some_null = was_null;
	*last_null = is_null;
}

/* For a naked ZWRITE, gvzwr_fini() sets "gv_currkey->end" to "gv_currkey->prev", which removes the last subscript of the key
 * of the previous reference, and then calls this function. Set "gv_last_subsc_null" (the last subscript of "gv_currkey" is
 * the null subscript) and "gv_some_subsc_null" (another subscript is) for this shorter key, as op_gvname() and op_gvnaked()
 * do for the key they build. On entry, "gv_some_subsc_null" is still that of the previous reference (a subscript other than
 * its last is null), so if it is FALSE, no subscript of the shorter key is null and the key need not be scanned.
 */
STATICFNDEF void gvzwr_set_null_subsc_flags(void)
{
	boolean_t	is_null, was_null;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!TREF(gv_some_subsc_null))
	{	/* As null subscripts are rarely used, this avoids the scan in most cases */
		DEBUG_ONLY(gvzwr_scan_null_subsc(&was_null, &is_null));
		assert(!was_null && !is_null);
		TREF(gv_last_subsc_null) = FALSE;
		return;
	}
	gvzwr_scan_null_subsc(&was_null, &is_null);
	TREF(gv_some_subsc_null) = was_null;
	TREF(gv_last_subsc_null) = is_null;
}

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
	old = (gv_key *)malloc(SIZEOF(gv_key) + gv_currkey->end + 1);
	gvzwrite_block->old_key = (unsigned char *)old;
	memcpy(gvzwrite_block->old_key, gv_currkey, SIZEOF(gv_key) + gv_currkey->end + 1);
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
			if (IS_ILLEGAL_GVNAKED(gv_currkey))
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_GVNAKED);
			gv_currkey->end = gv_currkey->prev;
			gv_currkey->base[gv_currkey->end] = 0;
			gv_currkey->prev = 0;
			/* If gvnh_reg corresponds to a spanning global, then determine
			 * gv_cur_region/gv_target/gd_targ_* variables based on updated gv_currkey.
			 */
			gvnh_reg = TREF(gd_targ_gvnh_reg);	/* set by op_gvname in previous call */
			GV_BIND_SUBSNAME_FROM_GVNH_REG_IF_GVSPAN(gvnh_reg, gd_header, gv_currkey);
			/* As op_gvnaked() does: the naked reference can be from before the current transaction, so set up the
			 * region for the transaction, and do the root search if it has not found the global yet (another
			 * process might have created the global since). Then set the null subscript flags for the shorter key,
			 * as those of the previous reference no longer apply and op_gvdata() checks them.
			 */
			if (IS_REG_BG_OR_MM(gv_cur_region))
			{
				if (dollar_tlevel)
					tp_set_sgm();
				GVCST_ROOT_SEARCH;
			}
			gvzwr_set_null_subsc_flags();
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
			if (0 < local.str.len)
			{
				assert(local.str.len <= (MAX_MIDENT_LEN + 1));
				local.str.addr++;
				local.str.len--;
				memcpy(&m[0], local.str.addr, (size_t)local.str.len);
				local.str.addr = &m[0];
			} else
				break;
		}
	}
	gvzwrite_clnup();	/* this routine is called by gvzwrite_ch() too */
	REVERT;
	return;
}
