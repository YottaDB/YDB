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

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "copy.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "op.h"
#include "gvcst_root_search.h"
#include "tp_set_sgm.h"

#define DIR_ROOT 1

GBLREF	gd_addr		*gd_header;
GBLREF	gd_binding	*gd_map;
GBLREF	gd_addr		*gd_targ_addr;
GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgm_info	*first_sgm_info;
GBLREF	short		dollar_tlevel;
GBLREF	bool		gv_curr_subsc_null;




void op_gvrectarg (mval *v)
{
	int		len, n;
	mstr		temp;
	unsigned char	*c;

	/* You might be somewhat apprehensive at the seemingly cavalier use of GTMASSERT
		in this routine.  First, let me explain myself.  The mvals passed to
		RECTARG are supposed to come only from SAVTARG, and are to represent
		the state of gv_currkey when SAVTARG was done.  Consequently, there are
		certain preconditions that one can expect.  Namely,

	1)	SAVTARG makes all mvals MV_STR's.  If this one isn't, it should be.
   	2)	If gv_currkey existed for SAVTARG (len is > 0), it had better exist for RECTARG.
	3)	All gv_keys end with 00.  When reading this mval, if you run out of characters
		before you see a 0, something is amiss.
	*/

	if (!MV_IS_STRING(v))
		GTMASSERT;

	n = len = v->str.len - sizeof(short) - sizeof(gd_targ_addr);
	if (len <= 0)
	{
		if (gv_currkey)
		{	gv_currkey->end = gv_currkey->prev = 0;
		}
		return;
	}
	if (!gv_currkey)
		GTMASSERT;

	c = (unsigned char *) (v->str.addr + sizeof(short) + sizeof(gd_targ_addr));
	temp.addr = (char *)c;
	while (*c++)
	{	n--;
		if (n <= 0)
			GTMASSERT;
	}

	temp.len = (char *)c - temp.addr - 1;
	if (   (gd_header != 0  &&  gd_header->maps == gd_map)
	    && (gv_currkey->base[temp.len] == 0  &&  memcmp(gv_currkey->base, temp.addr, temp.len) == 0)  )
	{
		gv_currkey->end = temp.len + 1;
		gv_currkey->prev = 0;
		gv_currkey->base[temp.len + 1] = 0;
		if (gv_cur_region->dyn.addr->acc_meth == dba_bg || gv_cur_region->dyn.addr->acc_meth == dba_mm)
		{
			if (dollar_tlevel != 0  &&  first_sgm_info == NULL)
				tp_set_sgm();
			if (gv_target->root == 0 || gv_target->root == DIR_ROOT)
				gvcst_root_search();
		}
	}
	else
	{
		memcpy(&gd_targ_addr, v->str.addr + sizeof(short), sizeof(gd_targ_addr));
		gv_bind_name(gd_targ_addr, &(temp));
	}
	c = (unsigned char *)v->str.addr;
	GET_SHORT(gv_currkey->prev,c);
	c += sizeof(short) + sizeof(gd_targ_addr);
	gv_currkey->end  = len;
	memcpy(gv_currkey->base, c, len);
	gv_currkey->base[len] = 0;
	c = &gv_currkey->base [ gv_currkey->prev ];
	gv_curr_subsc_null = ( *c++ == 255 && *c == 0);
	return;
}
