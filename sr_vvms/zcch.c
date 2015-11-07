/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "io.h"
#include "error.h"
#include "setterm.h"

GBLREF io_pair		io_std_device;

CONDITION_HANDLER(zcch)
{
	static struct chf$signal_array *newsig = 0;
	void		free();
	error_def	(ERR_ZCUSRRTN);

	if (SIGNAL == SS$_DEBUG)
		return SS$_RESIGNAL;
	if (SIGNAL == SS$_UNWIND)
	{
		if (newsig)
			free(newsig);
		return SS$_NORMAL;
	}

	if (io_std_device.in->type == tt)
	{	setterm(io_std_device.in);
	}

	/* add GT.M cover message over users error */
	newsig = malloc((sig->chf$l_sig_args + 2 + 1) * SIZEOF(int4));
	memcpy(&newsig->chf$l_sig_name + 2, &sig->chf$l_sig_name, sig->chf$l_sig_args * SIZEOF(int4));
	newsig->chf$l_sig_args = sig->chf$l_sig_args + 2;
	newsig->chf$l_sig_name = ERR_ZCUSRRTN;
	newsig->chf$l_sig_arg1 = 0;
	sig = newsig;
	NEXTCH;
}
