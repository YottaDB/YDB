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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "error.h"
#include "jnl.h"
#include "mupipset.h"
#include "mupip_exit.h"
#include "ipcrmid.h"
#include "util.h"

GBLREF	uint4		mupip_set_jnl_exit_error;

CONDITION_HANDLER(mupip_set_jnl_ch)
{

	START_CH
	mupip_set_jnl_cleanup();
	if (arg != 0)
	{
		PRN_ERROR

		if (SEVERITY == ERROR  ||  SEVERITY == SEVERE)
			/* NOTE:  mupip_set_jnl_exit_error == 0 (== SS_NORMAL)
				  if we did not come here via mupip_set_journal() */
			mupip_exit(mupip_set_jnl_exit_error);
	}
	CONTINUE
}
