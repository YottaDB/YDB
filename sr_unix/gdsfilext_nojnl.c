/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include <errno.h>

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblkops.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "min_max.h"
#include "t_qread.h"
#include "dse.h"
#include "gtmmsg.h"
#include "t_begin.h"
#include "t_write_map.h"
#include "t_abort.h"
#include "t_retry.h"
#include "t_end.h"
#include "wbox_test_init.h"
#include "error.h"
#include "t_recycled2free.h"
#include "cdb_sc.h"
#include "eintr_wrappers.h"
#include "gtmimagename.h"
#include "gdsfilext_nojnl.h"
#include "gtmio.h"
#include "anticipatory_freeze.h"

#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "interlock.h"
#include "gdsbgtr.h"
#include "copy.h"
#include "shmpool.h"

error_def(ERR_DBFILERR);
/* Minimal file extend. Called (at the moment) from mur_back_process.c when processing JRT_TRUNC record.
 * We want to avoid jnl and other interferences of gdsfilext.
 */
int gdsfilext_nojnl(gd_region* reg, uint4 new_total, uint4 old_total)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	int 			status;
	off_t			offset;
	char			*newmap;
	uint4			ii;
	unix_db_info		*udi;

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	assert(old_total < new_total);
	assert(new_total <= MAXTOTALBLKS(csd));
	WRITE_EOF_BLOCK(reg, csd, new_total, status);
	if (0 != status)
	{
		send_msg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
		return status;
	}
	/* initialize the bitmap tn's to 0. */
	newmap = (char *)malloc(csd->blk_size);
	bml_newmap((blk_hdr *)newmap, BM_SIZE(BLKS_PER_LMAP), 0);
	/* initialize bitmaps, if any new ones are added */
	for (ii = ROUND_UP(old_total, BLKS_PER_LMAP); ii < new_total; ii += BLKS_PER_LMAP)
	{
		offset = (off_t)(csd->start_vbn - 1) * DISK_BLOCK_SIZE + (off_t)ii * csd->blk_size;
		DB_LSEEKWRITE(csa, udi->fn, udi->fd, offset, newmap, csd->blk_size, status);
		if (0 != status)
		{
			send_msg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
			free(newmap);
			return status;
		}
	}
	csa->ti->free_blocks += DELTA_FREE_BLOCKS(new_total, old_total);
	csa->ti->total_blks = new_total;
	free(newmap);
	return status;
}
