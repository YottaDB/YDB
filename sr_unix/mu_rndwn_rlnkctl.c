/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
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
#endif
#include "mu_rndwn_rlnkctl.h"	/* for mupip_rctldump prototype */

#ifdef AUTORELINK_SUPPORTED
error_def(ERR_MUPCLIERR);
#endif

void mu_rndwn_rlnkctl(void)
{
#	ifdef AUTORELINK_SUPPORTED
	open_relinkctl_sgm	*linkctl;
	relinkshm_hdr_t		*shm_hdr;
	relinkrec_t		*linkrec;
	relinkctl_data		*hdr;
	rtnobjshm_hdr_t		*rtnobj_shm_hdr;
	char			objdir[GTM_PATH_MAX];
	int			i, j, recnum, n_records, shmid, shm_stat, save_errno;
	unsigned short		max_len;
	mstr			dir;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TREF(is_mu_rndwn_rlnkctl) = TRUE;	/* relinkctl_open and relinkctl_rundown check this flag to act differently */
	if (TREF(parms_cnt))
	{
		assert(1 == TREF(parms_cnt));
		max_len = SIZEOF(objdir);
		if (!cli_get_str("WHAT", objdir, &max_len))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUPCLIERR);
		dir.addr = objdir;
		dir.len = max_len;
		linkctl = relinkctl_attach(&dir);
		assert(linkctl == TREF(open_relinkctl_list));
		assert((NULL == linkctl) || (NULL == linkctl->next));
	} else
		zro_init();
	relinkctl_rundown(TRUE, FALSE);	/* will rundown one or more linkctl files opened above */
	TREF(is_mu_rndwn_rlnkctl) = FALSE;
#	endif	/* AUTORELINK_SUPPORTED */
}

