/****************************************************************
 *								*
 * Copyright (c) 2009-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_signal.h" /* for SIGPROCMASK used inside Tcsetattr */

#include "eintr_wrappers.h"
#include "iosp.h"
#include "io.h"
#include "io_params.h"
#include "iottdef.h"			/* include termios.h */
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
	boolean_t	is_defined, dosetattr = FALSE;
	mstr		valstr;
	mval		pars, val;
	int		status, local_errno;
	d_tt_struct	*tt_ptr;
	struct termios	ttio;

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
		valstr.len = SIZEOF(GTM_NOZENABLE) - 1;
		valstr.addr = GTM_NOZENABLE;
		if (logical_truth_value(&valstr, FALSE, &is_defined))
		{	/* default is leave VSUSP alone */
		        tt_ptr = (d_tt_struct *)io_std_device.in->dev_sp;
			if (tt_ptr && tt_ptr->ttio_modified && (!tcgetattr(tt_ptr->fildes, &ttio)))
			{
				if (_POSIX_VDISABLE != ttio.c_cc[VSUSP])
				{
					ttio.c_cc[VSUSP] = _POSIX_VDISABLE;
					dosetattr = TRUE;
				}
#				ifdef VDSUSP
				if (_POSIX_VDISABLE != ttio.c_cc[VDSUSP])
				{
					ttio.c_cc[VDSUSP] = _POSIX_VDISABLE;
					dosetattr = TRUE;
				}
#				endif
				if (dosetattr)
				{
					Tcsetattr(tt_ptr->fildes, TCSANOW, &ttio, status, local_errno);
					assert(!status);	/* ignore error if pro */
					if (!status)
					{	/* use for iott_setterm and iott_resetterm */
						tt_ptr->nozenable = TRUE;
						tt_ptr->ttio_struct->c_cc[VSUSP] = ttio.c_cc[VSUSP];
#						ifdef VDSUSP
						tt_ptr->ttio_struct->c_cc[VDSUSP] = ttio.c_cc[VDSUSP];
#						endif
					}
				}
			}
		}
	}
	return;
}
