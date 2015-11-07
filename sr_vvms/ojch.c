/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"
#include <iodef.h>
#include <stsdef.h>
#include "job.h"

#define FATAL(error)	(error & STS$M_COND_ID | STS$K_SEVERE)
GBLREF pchan;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);

CONDITION_HANDLER(ojch)
{
	mstr		stsdsc;
	pmsg_type	stsmsg;
	mbx_iosb	iosb;

	switch(SIGNAL)
	{
		case SS$_ACCVIO:
		case SS$_ASTFLT:
		case SS$_OPCCUS:
		case SS$_OPCDEC:
		case SS$_PAGRDERR:
		case SS$_RADRMOD:
		case SS$_ROPRAND:
			gtm_dump();
			break;
		default:
			if ((SIGNAL == ERR_ASSERT) || (SIGNAL == ERR_GTMCHECK) || (SIGNAL == ERR_GTMASSERT)	/* BYPASSOK */
			    || (SIGNAL == ERR_GTMASSERT))
				gtm_dump();
			break;
	}
	stsdsc.addr = &stsmsg;
	stsdsc.len = SIZEOF(stsmsg);
	stsmsg.unused = 0;
	stsmsg.finalsts = SIGNAL;
	ojmbxio(IO$_WRITEVBLK, pchan, &stsdsc, &iosb, TRUE);
	EXIT(FATAL(SIGNAL));
	NEXTCH;
}
