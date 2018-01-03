/****************************************************************
 *								*
 * Copyright (c) 2017 Fidelity National Information		*
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

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "repl_msg.h"		/* needed for jnlpool_addrs_ptr_t */
#include "gtmsource.h"		/* needed for jnlpool_addrs_ptr_t */
#include "secshr_db_clnup.h"
#include "send_msg.h"	/* for send_msg prototype */

error_def(ERR_DBCLNUPINFO);

void	secshr_send_DBCLNUPINFO_msg(sgmnt_addrs *csa, int numargs, gtm_uint64_t *argarray)
{
	int	i;
	char	secshr_string[2048];
	char	secshr_string_delta[256];

	secshr_string[0] = '\0';
	for (i = 0; i < numargs; i += 2)
	{
		if (0 != i)
			strcat(secshr_string, " : ");
		SPRINTF(secshr_string_delta, "%s = [0x%08lx]", argarray[i], argarray[i+1]);
		strcat(secshr_string, secshr_string_delta);
	}
	assert(ARRAYSIZE(secshr_string) > strlen(secshr_string));
	send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_DBCLNUPINFO, 4, DB_LEN_STR(csa->region), RTS_ERROR_TEXT(secshr_string));
}
