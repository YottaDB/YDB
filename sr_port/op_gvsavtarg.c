/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "stringpool.h"
#include "op.h"

GBLREF	gd_addr		*gd_targ_addr;
GBLREF	gv_key		*gv_currkey;
GBLREF	spdesc		stringpool;
GBLREF	bool		gv_curr_subsc_null;
GBLREF	bool		gv_prev_subsc_null;

void op_gvsavtarg(mval *v)
{
	int		len;
	unsigned char	*c;

	v->mvtype = MV_STR;
	if (gv_currkey)
	{
		assert (gd_targ_addr != 0);
		len = gv_currkey->end + sizeof(short) + sizeof(gd_targ_addr)
			+ sizeof(gv_curr_subsc_null) + sizeof(gv_prev_subsc_null);
	} else
		len = sizeof(short);
	if (stringpool.top - stringpool.free < len)
		stp_gcol(len);
	v->str.len = len;
	v->str.addr = (char *)stringpool.free;
	c = stringpool.free;
	stringpool.free += len;
	if (gv_currkey)
	{
		memcpy(c, &gv_currkey->prev, sizeof(short));
		c += sizeof(short);
		memcpy(c, &gd_targ_addr, sizeof(gd_targ_addr));
		c += sizeof(gd_targ_addr);
		memcpy(c, &gv_curr_subsc_null, sizeof(gv_curr_subsc_null));
		c += sizeof(gv_curr_subsc_null);
		memcpy(c, &gv_prev_subsc_null, sizeof(gv_prev_subsc_null));
		c += sizeof(gv_prev_subsc_null);
		len = len - sizeof(short) - sizeof(gd_targ_addr) - sizeof(gv_curr_subsc_null) - sizeof(gv_prev_subsc_null);
		assert(gv_currkey->end == len);
		if (0 < len)
		{
			assert(gv_currkey->base[0]);
			memcpy(c, &gv_currkey->base[0], len);
		}
	} else
		memset(c, 0, sizeof(short));
	return;
}
