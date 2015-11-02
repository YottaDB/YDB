/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "iosp.h"
#include "io.h"
#include "io_params.h"
#include "op.h"
#include "gtm_logicals.h"
#include "logical_truth_value.h"
#include "cenable.h"

GBLREF io_pair		io_std_device;			/* standard device	*/

void cenable(void)
{
	static readonly unsigned char cenable_params_list[2] =
	{
		(unsigned char)iop_cenable,
		(unsigned char)iop_eol
	};
	boolean_t	is_defined;
	mstr		valstr;
	mval		pars, val;

	if (io_std_device.in->type == tt)
	{
		valstr.len = SIZEOF(GTM_NOCENABLE) - 1;
		valstr.addr = GTM_NOCENABLE;
		if (!logical_truth_value(&valstr, FALSE, &is_defined))
		{	/* if they don't ask for nocenable, the default is enable */
			pars.str.len = SIZEOF(cenable_params_list);
			pars.str.addr = (char *)cenable_params_list;
			pars.mvtype = val.mvtype = MV_STR;
			val.str.len = io_std_device.in->trans_name->len;
			val.str.addr = io_std_device.in->trans_name->dollar_io;
			op_use(&val, &pars);
		}
	}
	return;
}
