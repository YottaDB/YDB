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
#include <sys/sem.h>

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
#include "gtmmsg.h"
#include "error.h"
#include "eintr_wrappers.h"
#include "recover_truncate.h"
#include "interlock.h"
#include "shmpool.h"
#include "gtmio.h"
#include "clear_cache_array.h"
#include "is_proc_alive.h"
#include "do_semop.h"
#include "anticipatory_freeze.h"	/* needed for WRITE_EOF_BLOCK */

error_def(ERR_DBFILERR);
error_def(ERR_MUTRUNCERROR);

GBLREF	uint4			process_id;

void recover_truncate(sgmnt_addrs *csa, sgmnt_data_ptr_t csd, gd_region* reg)
{
	char			*err_msg;
	uint4			old_total, cur_total, new_total;
	off_t			old_size, cur_size, new_size;
	int			ftrunc_status, status;
	unix_db_info    	*udi;
	int			semval;

	if (NULL != csa->nl && csa->nl->trunc_pid && !is_proc_alive(csa->nl->trunc_pid, 0))
		csa->nl->trunc_pid = 0;
	if (!csd->before_trunc_total_blks)
		return;
	assert((GDSVCURR == csd->desired_db_format) && (csd->blks_to_upgrd == 0) && (dba_mm != csd->acc_meth));
	/* If called from db_init, assure we've grabbed the access semaphor and are the only process attached to the database.
	 * Otherwise, we should have crit when called from wcs_recover. */
	udi = FILE_INFO(reg);
	assert((udi->grabbed_access_sem && (1 == (semval = semctl(udi->semid, 1, GETVAL)))) || csa->now_crit);
	/* Interrupted truncate scenario */
	if (NULL != csa->nl)
		csa->nl->root_search_cycle++;
	old_total = csd->before_trunc_total_blks;					/* Pre-truncate total_blks */
	old_size = (off_t)SIZEOF_FILE_HDR(csd)						/* Pre-truncate file size (in bytes) */
			+ (off_t)old_total * csd->blk_size + DISK_BLOCK_SIZE;
	cur_total = csa->ti->total_blks;						/* Actual total_blks right now */
	cur_size = (off_t)gds_file_size(reg->dyn.addr->file_cntl) * DISK_BLOCK_SIZE;	/* Actual file size right now (in bytes) */
	new_total = csd->after_trunc_total_blks;					/* Post-truncate total_blks */
	new_size = old_size - (off_t)(old_total - new_total) * csd->blk_size;		/* Post-truncate file size (in bytes) */
	/* We don't expect FTRUNCATE to leave the file size in an 'in between' state, hence the assert below. */
	assert(old_size == cur_size || new_size == cur_size);
	if (new_total == cur_total && old_size == cur_size)
	{ /* Crash after reducing total_blks, before successful FTRUNCATE. Complete the FTRUNCATE here. */
		DBGEHND((stdout, "DBG:: recover_truncate() -- completing truncate, old_total = [%lu], cur_total = [%lu]\n",
			old_total, new_total));
		assert(csd->before_trunc_free_blocks >= DELTA_FREE_BLOCKS(old_total, new_total));
		csa->ti->free_blocks = csd->before_trunc_free_blocks - DELTA_FREE_BLOCKS(old_total, new_total);
		clear_cache_array(csa, csd, reg, new_total, old_total);
		WRITE_EOF_BLOCK(reg, csd, new_total, status);
		if (status != 0)
		{
			err_msg = (char *)STRERROR(errno);
			rts_error(VARLSTCNT(6) ERR_MUTRUNCERROR, 4, REG_LEN_STR(reg), LEN_AND_STR(err_msg));
			return;
		}
		FTRUNCATE(FILE_INFO(reg)->fd, new_size, ftrunc_status);
		if (ftrunc_status != 0)
		{
			err_msg = (char *)STRERROR(errno);
			rts_error(VARLSTCNT(6) ERR_MUTRUNCERROR, 4, REG_LEN_STR(reg), LEN_AND_STR(err_msg));
			return;
		}
	} else
	{
		/* Crash before even changing csa->ti->total_blks OR after successful FTRUNCATE */
		/* In either case, the db file is in a consistent state, so no need to do anything further */
		assert((old_total == cur_total && old_size == cur_size) || (new_total == cur_total && new_size == cur_size));
		if (!((old_total == cur_total && old_size == cur_size) || (new_total == cur_total && new_size == cur_size)))
		{
			rts_error(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
		}
	}
	csd->before_trunc_total_blks = 0; /* indicate CONSISTENT */
}
