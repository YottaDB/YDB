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

/* Handle indirect device parameters for open, use and close commands */
#include "mdef.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "io_params.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "deviceparameters.h"
#include "op.h"

error_def(ERR_INDEXTRACHARS);

void	op_inddevparms(mval *devpsrc, int4 ok_iop_parms,  mval *devpiopl)
{
	int	rval;
	icode_str	indir_src;
	mstr		*obj, object;
	oprtype		devpopr, plist, getdst;
	triple		*indref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(devpsrc);
	indir_src.str = devpsrc->str;
	indir_src.code = indir_devparms;
	if (NULL == (obj = cache_get(&indir_src)))				/* NOTE assignment */
	{	/* No cached version, compile it now */
		obj = &object;
		comp_init(&devpsrc->str, &getdst);
		if (TK_ATSIGN == TREF(window_token))
		{	/* For the indirection-obsessive */
			if (EXPR_FAIL != (rval = indirection(&devpopr)))	/* NOTE assignment */
			{
				indref = newtriple(OC_INDDEVPARMS);
				indref->operand[0] = devpopr;
				indref->operand[1] = put_ilit(ok_iop_parms);
				plist = put_tref(indref);
			}
		} else	/* We have the parm string to process now */
			rval = deviceparameters(&plist, ok_iop_parms);
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAL, &plist, &getdst, devpsrc->str.len))
			return;
		indir_src.str.addr = devpsrc->str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	TREF(ind_result) = devpiopl;						/* Where to store return value */
	comp_indr(obj);
	return;
}
