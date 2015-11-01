/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include "gdsbt.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "is_file_identical.h"

void jnl_wait(gd_region *reg)
{
	jnl_private_control	*jpc;
	sgmnt_addrs		*csa;
	uint4			status;

	if ((NULL != reg) && (TRUE == reg->open))
	{
		csa = &FILE_INFO(reg)->s_addrs;
		jpc = csa->jnl;
		if ((TRUE == JNL_ENABLED(csa->hdr)) && (NULL != jpc))
		{	/* wait on jnl writes for region */
			status = jnl_write_attempt(jpc, jpc->new_freeaddr);
			UNIX_ONLY(
				  if (SS_NORMAL == status && !JNL_FILE_SWITCHED(jpc))
					jnl_fsync(reg, jpc->new_freeaddr);
				 );
		}
	}
	return;
}
