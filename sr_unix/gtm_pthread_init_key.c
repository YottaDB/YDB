/****************************************************************
 *								*
 * Copyright (c) 2015-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gdsfhead.h"
#include "gtm_multi_thread.h"
#include "gtm_pthread_init_key.h"

error_def(ERR_SYSCALL);

/* The following function needs to be invoked first thing inside a mur_* thread function.
 * Returns 0 on success. Non-zero otherwise.
 */
uint4	gtm_pthread_init_key(gd_region *reg)
{
	int		status;
#	ifdef GTM_PTHREAD
	unsigned char	*ptr;

	if (!multi_thread_in_use)
		return 0;
	/* Initialize thread_gtm_putmsg_rname_key at start of thread */
	assert('\0' == reg->rname[reg->rname_len]);	/* ensure region name is null terminated */
	/* In rare cases (e.g. "mur_db_files_from_jnllist"), rname is not initialized, but fname is. Use it then. */
	if (!reg->owning_gd->is_dummy_gbldir)
	{
		assert(reg->rname_len);
		ptr = &reg->rname[0];
		assert('\0' == ptr[reg->rname_len]);
	} else
	{
		assert(!memcmp(reg->rname, "DEFAULT", reg->rname_len));
		ptr = &reg->dyn.addr->fname[0];
		assert('\0' == ptr[reg->dyn.addr->fname_len]);
	}
	assert('\0' != *ptr);
	status = pthread_setspecific(thread_gtm_putmsg_rname_key, ptr);
	assert(0 == status);
	if ((0 != status) && !IS_GTM_IMAGE)	/* Display error if mupip/dse etc. but not for mumps */
	{	/* Note: We got an error while setting the rname_key so gtm_putmsg_csa will not print the
		 * region-name prefix for this message like it usually does for gtm_putmsg calls inside threaded code.
		 * Not having the region name in this rare case is considered okay for now.
		 */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("pthread_setspecific()"), CALLFROM,
															status);
	}
#	else
	status = 0;
#	endif
	return status;
}
