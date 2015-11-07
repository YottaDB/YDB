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
#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_limits.h"

#include "relinkctl.h"
#include "util.h"
#include <rtnhdr.h>		/* needed for zroutines.h */
#include "zroutines.h"
#include "cli.h"
#include "cliif.h"
#include "cli_parse.h"
#include "eintr_wrappers.h"
#include "gtmmsg.h"
#endif
#include "mu_rndwn_rlnkctl.h"	/* for mupip_rctldump prototype */

#ifdef AUTORELINK_SUPPORTED
error_def(ERR_FILEPARSE);
error_def(ERR_MUPCLIERR);
error_def(ERR_RLNKCTLRNDWNSUC);
#endif

/* Implements MUPIP RUNDOWN -RELINKCTL */
void mu_rndwn_rlnkctl(void)
{
#	ifdef AUTORELINK_SUPPORTED
	open_relinkctl_sgm	*linkctl;
	relinkshm_hdr_t		*shm_hdr;
	relinkrec_t		*linkrec;
	relinkctl_data		*hdr;
	rtnobjshm_hdr_t		*rtnobj_shm_hdr;
	char			objdir[GTM_PATH_MAX];
	int			i, j, recnum, n_records, shmid, shm_stat, save_errno, objcnt, stat_res;
	unsigned short		param_len;
	mstr			dir;
	zro_ent			*op;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TREF(is_mu_rndwn_rlnkctl) = TRUE;	/* relinkctl_open and relinkctl_rundown check this flag to act differently.
						 * No need to have condition handler to reset this in case of error because
						 * this is invoked from MUPIP RUNDOWN -RELINKCTL which is the only action
						 * that the calling process will do in its lifetime.
						 */
	if (TREF(parms_cnt))
	{
		assert(1 == TREF(parms_cnt));
		param_len = SIZEOF(objdir) - 1;
		if (!cli_get_str("WHAT", objdir, &param_len))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUPCLIERR);
		objdir[param_len] = '\0';
		dir.addr = objdir;
		dir.len = param_len;
		linkctl = relinkctl_attach(&dir, NULL, SIZEOF(objdir));
		assert((dir.len <= SIZEOF(objdir) - 1) && (('\0' == dir.addr[dir.len]) || (0 == dir.len)));
		assert(linkctl == TREF(open_relinkctl_list));
		assert((NULL == linkctl) || (NULL == linkctl->next));
		if (NULL == linkctl)
		{
			if (0 == dir.len)
			{	/* If relinkctl_attach() did not find the object directory corresponding to the user's argument,
				 * dir.len is set to 0; in that case we want to print an error containing the argument, and so the
				 * proper length needs to be restored.
				 */
				dir.len = param_len;
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, RTS_ERROR_MSTR(&dir), errno);
			} else
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RLNKCTLRNDWNSUC, 2, RTS_ERROR_MSTR(&dir));
		} else
			relinkctl_rundown(TRUE, FALSE);	/* Runs down the relinkctl file opened above. */
	} else
	{	/* Unlike other callers of zro_init(), MUPIP RUNDOWN -RELINKCTL does not want it to do relinkctl_attach() on all
		 * relinkctl files at once because we leave the attach logic holding the linkctl lock, which might potentially cause
		 * a deadlock if multiple processes are run concurrently with different $gtmroutines. So, zro_init() / zro_load()
		 * set the count field of each autorelink-enabled object directory to a negative number based on the
		 * TREF(is_mu_rndwn_rlnkctl) global, and we look at the count to decide whether to attach to an individual segment.
		 */
		zro_init();
		objcnt = (TREF(zro_root))->count;
		assert(0 < objcnt);
		for (op = TREF(zro_root) + 1; (0 < objcnt--);)
		{	/* Go through each object directory in our array to run down its relinkctl file. */
			assert((ZRO_TYPE_OBJECT == op->type) || (ZRO_TYPE_OBJLIB == op->type));
			if (ZRO_TYPE_OBJLIB == op->type)
				continue;			/* We only deal with object directories in this loop */
			/* Currently the count field of object directory entries is either unused (with the value of 0) or marked as
			 * autorelink-enabled (only in case of MUPIP RUNDOWN -RELINKCTL). Assert that.
			 */
			assert(0 >= op->count);
			if (0 > op->count)
			{	/* Third parameter is 0 because the object directories are stored in fully resolved format in the
				 * gtmroutines object, so there is no need to update the string.
				 */
				linkctl = relinkctl_attach(&op->str, NULL, 0);
				assert(linkctl == TREF(open_relinkctl_list));
				assert((NULL == linkctl) || (NULL == linkctl->next));
				if (NULL == linkctl)
				{	/* We do not do a check of whether the object directory exists, unlike with an argument,
					 * because zro_init() would have errored out on a non-existent path.
					 */
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RLNKCTLRNDWNSUC, 2,
						       RTS_ERROR_MSTR(&op->str));
				} else
					relinkctl_rundown(TRUE, FALSE);	/* Runs down the relinkctl file opened above. */
			}
			/* Bump past source entries to next object entry */
			op++;
			assert(ZRO_TYPE_COUNT == op->type);
			op += op->count;
			op++;
		}
	}
	TREF(is_mu_rndwn_rlnkctl) = FALSE;
#	endif	/* AUTORELINK_SUPPORTED */
}
