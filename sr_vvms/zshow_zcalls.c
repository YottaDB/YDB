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

#include <descrip.h>

#include "mlkdef.h"
#include "zshow.h"
#include "zcall.h"

GBLREF zctabrtn *zctab, *zctab_end;
GBLREF zcpackage *zcpack_start, *zcpack_end;

/********************************************************************
 * the package and routine data structures are as follows:
 * zcpackage structure points to the
 *      name of each package.
 *  and begin, and endpoints of the routines in the package.
 *
 *    +-------------|----------|---------------------+
 * zcpack_start   zcpack ... zcpack...		zcpack_end
 *
 * zctabrtn structure points to the routines
 *
 * zctab        (...routines...)		zctab_end
 * +---------------------------------------------+
 *
 *		 sample
 *		 zctab:	 --routine--	|There might be routines that do
 *			 --routine--	|not belong to a package
 *	zcpack1->begin:	 --routine--
 *			 --routine--
 *	  zcpack1->end:	 --routine--	|Other routines that do not
 *			 --routine--	|belong to a package
 *	zcpack2->begin:	 --routine--
 *			 --routine--
 *zctab_end:zcpack2->end:--routine--
 *
 *
 ********************************************************************/
static readonly char period[] = ".";
void zshow_zcalls(zshow_out *output)
{
	mval			decpt_mv,pack_mv,rtn_mv;
	zctabrtn		*zcrtn;
	zcpackage		*zcpack;
	error_def(ERR_ZCALLTABLE);

	decpt_mv.mvtype = pack_mv.mvtype = rtn_mv.mvtype = MV_STR;
	decpt_mv.str.len = 1;
	decpt_mv.str.addr = &period;
	pack_mv.str.len = 0;
	zcrtn = zctab;
	zcpack = zcpack_start;
	while (zcrtn < zctab_end)
	{
		if (0 == zcrtn->callnamelen)
			rts_error(VARLSTCNT(1) ERR_ZCALLTABLE);
		if (zcpack < zcpack_end)
		{
			if (zcpack->end <= zcrtn)
			{
				zcpack++;
				pack_mv.str.len = 0;
			}
			if ((0 == pack_mv.str.len) && (zcpack->begin <= zcrtn))
			{
				pack_mv.str.addr = zcpack->packname + 1;
				pack_mv.str.len = *zcpack->packname;
			}
		}
		rtn_mv.str.addr = &zcrtn->callname;
		rtn_mv.str.len = zcrtn->callnamelen;
		if (pack_mv.str.len)
		{
			zshow_output(output,&pack_mv.str);
			zshow_output(output,&decpt_mv.str);
		}
		output->flush = TRUE;
		zshow_output(output,&rtn_mv.str);
		zcrtn = (zctabrtn *)((char *)zcrtn + zcrtn->entry_length);
	}
	return;
}
