/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "aio_shim.h"
#include "gtmio.h"
#include "anticipatory_freeze.h"
#include "wcs_wt.h"
#include "gdsbgtr.h"

GBLREF	uint4		process_id;

/* Reissues a pending qio from the WIP queue. In case of EAGAIN and holding crit, it issues a SYNCIO instead.
 * And if the SYNCIO succeeds, a special value of SYNCIO_MORPH_SUCCESS is returned so caller can handle this
 * situation appropriately (by moving the cr from wip queue to active queue or out of all queues).
 */
int	wcs_wt_restart(unix_db_info *udi, cache_state_rec_ptr_t csr)
{
	int			save_errno;
	blk_hdr_ptr_t		bp, save_bp;
	sgmnt_data_ptr_t	csd;
	cache_que_head_ptr_t	ahead;
	sgmnt_addrs		*csa;

	assert(0 > SYNCIO_MORPH_SUCCESS); /* save_errno should be positive in all cases except when == SYNCIO_MORPH_SUCCESS */
	csa = &udi->s_addrs;
	BG_TRACE_PRO_ANY(csa, wcs_wt_restart_invoked);
	csd = csa->hdr;
	bp = (blk_hdr_ptr_t)(GDS_ANY_REL2ABS(csa, csr->buffaddr));
	if (!csr->wip_is_encr_buf)
		save_bp = bp;
	else
		save_bp = (blk_hdr_ptr_t)GDS_ANY_ENCRYPTGLOBUF(bp, csa);
	DB_LSEEKWRITEASYNCRESTART(csa, udi, udi->fn, udi->fd, save_bp, csr, save_errno);
	assert(0 == save_errno IF_LIBAIO(|| EAGAIN  == save_errno));
	if (0 == save_errno)
		csr->epid = process_id;
	else if (EAGAIN == save_errno)
	{	/* ASYNC IO could not be started */
		BG_TRACE_PRO_ANY(csa, wcs_wt_restart_eagain);
		if (csa->now_crit && !csa->region->read_only)
		{	/* Holding crit and have read-write access to the database.
			 * Do synchronous IO given OS does not have enough memory temporarily.
			 */
			DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd,
				csr->aiocb.aio_offset, save_bp, csr->aiocb.aio_nbytes, save_errno);
			assert(0 <= save_errno);
			if (0 == save_errno)
			{	/* SYNCIO succeeded. Return special status */
				save_errno = SYNCIO_MORPH_SUCCESS;
			}
		} else
		{	/* Need to reinsert this back into the active queue.
			 * Clearing csr->epid and returning 0 indicates this to caller.
			 */
			BG_TRACE_PRO_ANY(csa, wcs_wt_restart_reinsert);
			csr->epid = 0;
			save_errno = 0;
		}
	}
	return save_errno;
}

