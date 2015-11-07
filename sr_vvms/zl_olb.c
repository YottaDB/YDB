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
#include "lbrdef.h"
#include <descrip.h>

globalvalue int4 LBR$_NORMAL;
globalvalue int4 LBR$_LIBOPN;
globalvalue int4 LBR$_OLDLIBRARY;
globalvalue int4 LBR$_KEYNOTFND;

zl_olb(mstr *olblib, mstr *module, int4 *libindex)
{
	int4			txtrfa[2];
	uint4			pos, status;
	struct dsc$descriptor_s	libnamdes, modnamdes;
	static readonly		$DESCRIPTOR(period, ".");

	modnamdes.dsc$b_dtype   = DSC$K_DTYPE_T;
	modnamdes.dsc$b_class   = DSC$K_CLASS_S;
	modnamdes.dsc$a_pointer = module->addr;
	modnamdes.dsc$w_length  = module->len;
	pos = lib$locc(&period, &modnamdes);
	if (pos != 0 && pos != module->len)
		modnamdes.dsc$w_length = pos - 1;
	else
		GTMASSERT;
	libnamdes.dsc$b_dtype   = DSC$K_DTYPE_T;
	libnamdes.dsc$b_class   = DSC$K_CLASS_S;
	libnamdes.dsc$a_pointer = olblib->addr;
	libnamdes.dsc$w_length  = olblib->len;
	status = lbr$ini_control(libindex, &LBR$C_READ, &LBR$C_TYP_OBJ, 0);
	if (LBR$_NORMAL == status)
	{
		status = lbr$open(libindex, &libnamdes, 0, 0, 0, 0, 0);
		if ((LBR$_OLDLIBRARY | LBR$_LIBOPN | LBR$_NORMAL) & status)
		{
			status = lbr$set_locate(libindex);
			if (LBR$_NORMAL == status)
				status = lbr$lookup_key(libindex, &modnamdes, txtrfa);
		}
	}
	return status;
}
