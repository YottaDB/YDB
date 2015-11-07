/****************************************************************
 *								*
 * Copyright (c) 2014-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef AUTORELINK_SUPPORTED

#include "gtm_string.h"

#include "relinkctl.h"
#include "util.h"
#include <rtnhdr.h>		/* needed for zroutines.h */
#include "zroutines.h"
#include "cli.h"
#include "cliif.h"
#include "cli_parse.h"
#include "zshow.h"
#endif
#include "mupip_rctldump.h"	/* for mupip_rctldump prototype */

#ifdef AUTORELINK_SUPPORTED
error_def(ERR_MUPCLIERR);
#endif

/* Implements MUPIP RCTLDUMP */
void mupip_rctldump(void)
{
#	ifdef AUTORELINK_SUPPORTED
	unsigned short		max_len;
	mstr			dir;
	char			objdir[GTM_PATH_MAX];
	open_relinkctl_sgm	*linkctl;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(parms_cnt))
	{
		assert(1 == TREF(parms_cnt));
		max_len = SIZEOF(objdir);
		if (!cli_get_str("DIRECTORY", objdir, &max_len))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUPCLIERR);
		dir.addr = objdir;
		dir.len = max_len;
		linkctl = relinkctl_attach(&dir, NULL, 0);
		assert(linkctl == TREF(open_relinkctl_list));
		assert((NULL == linkctl) || (NULL == linkctl->next));
	} else
		zro_init();
	util_out_print("", RESET);	/* Reset output buffer */
	zshow_rctldump(NULL);		/* callee knows caller is mupip_rctldump type based on the NULL parameter */
#	endif	/* AUTORELINK_SUPPORTED */
}

