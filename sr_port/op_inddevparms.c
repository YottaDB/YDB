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

/* Handle indirect device parameters for open, use and close commands */
#include "mdef.h"
#include "hashdef.h"
#include "lv_val.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "io_params.h"
#include "cache.h"
#include "deviceparameters.h"
#include "op.h"

GBLREF mval	**ind_result_sp, **ind_result_top;
GBLREF char	window_token;

LITREF mval literal_null;

void	op_inddevparms(mval *devpsrc, int4 ok_iop_parms,  mval *devpiopl)
{
	bool		rval;
	mstr		*obj, object;
	oprtype		devpopr;
	triple		*indref;
	oprtype		plist;
	error_def(ERR_INDMAXNEST);
	error_def(ERR_INDEXTRACHARS);

	MV_FORCE_STR(devpsrc);
	if (!(obj = cache_get(indir_devparms, &devpsrc->str)))
	{	/* No cached version, compile it now */
		comp_init(&devpsrc->str);
		if (TK_ATSIGN == window_token)
		{	/* For the indirection-obsessive */
			if (!indirection(&devpopr))
				rts_error(VARLSTCNT(1) ERR_INDEXTRACHARS);
			indref = newtriple(OC_INDDEVPARMS);
			indref->operand[0] = devpopr;
			indref->operand[1] = put_ilit(ok_iop_parms);
			plist = put_tref(indref);
			rval = TRUE;
		} else	/* We have the parm string to process now */
			rval = (bool)deviceparameters(&plist, ok_iop_parms);
		if (!comp_fini(rval, &object, OC_IRETMVAL, &plist, devpsrc->str.len))
			return;
		cache_put(indir_devparms, &devpsrc->str, &object);
		obj = &object;
		/* Fall into code activation below */
	}
	*ind_result_sp++ = devpiopl;		/* Where to store return value */
	if (ind_result_sp >= ind_result_top)
		rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
	comp_indr(obj);
}
