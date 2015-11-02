/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef UNIX
#include <errno.h>		/* for DSK_WRITE macro */
#elif defined(VMS)
#include "efn.h"		/* for DSK_WRITE macro */
#else
#error UNSUPPORTED PLATFORM
#endif

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;

int4 bml_init(block_id bml)
{
	blk_hdr_ptr_t	ptr;
	uint4		size;
	uint4		status;

	size = BM_SIZE(cs_data->bplmap);
	/* Allocate full block .. bml_newmap will set the write size, dsk_write will write part or all of it as appropriate. */
	ptr = (blk_hdr_ptr_t)malloc(cs_addrs->hdr->blk_size);
	/* We are about to create a local bitmap. Setting its block transaction number to the current database transaction
	 * number gives us a clear history of when this bitmap got created. But with before-image journaling we have an issue.
	 * This is because before-image journaling implies the possibility of using backward recovery/rollback both of which
	 * can take the database to a transaction number MUCH BEFORE the current database transaction number. In that case, the
	 * recovered database will have DBTNTOOLG integrity errors since the bitmap block's transaction number will be greater
	 * than the post-recovery database transaction number. Since we have no control over what transaction number backward
	 * recovery can take the database to, we set the bitmap block transaction number to 0 (the least possible) for the
	 * before-image journaling case.
	 */
	bml_newmap(ptr, size, ((JNL_ENABLED(cs_data) && cs_addrs->jnl && cs_addrs->jnl->jnl_buff &&
				cs_addrs->jnl->jnl_buff->before_images) ?  0 : cs_data->trans_hist.curr_tn));
	/* status holds the status of any error return from dsk_write */
	DSK_WRITE_NOCACHE(gv_cur_region, bml, (sm_uc_ptr_t)ptr, cs_data->desired_db_format, status);
	free(ptr);
	return status;
}
