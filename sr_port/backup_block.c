/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "shmpool.h"
#include "memcoherency.h"
#include "mupipbckup.h"


GBLREF	uint4			process_id;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t	cs_data;

/* requires that cs_addrs and gv_cur_region pointing to the right value */

boolean_t backup_block(block_id blk, cache_rec_ptr_t backup_cr, sm_uc_ptr_t backup_blk_p)
{
	uint4 			bsiz;
	int4			required;
	shmpool_buff_hdr_ptr_t	sbufh_p;
	shmpool_blk_hdr_ptr_t	sblkh_p;
	boolean_t		ret = TRUE;


	/* assuring crit, so only one instance of this module could be running at any time */
	assert(cs_addrs->now_crit);
	/* Should have EITHER backup cr (BG mode) or buffer pointer (MM mode) */
	assert((dba_bg == cs_data->acc_meth && NULL != backup_cr && NULL == backup_blk_p) ||
	       (dba_mm == cs_data->acc_meth && NULL == backup_cr && NULL != backup_blk_p));
	if (dba_bg == cs_data->acc_meth)
	{	/* Get buffer address from the cache record */
		VMS_ONLY(assert(0 == backup_cr->shmpool_blk_off));
		backup_blk_p = GDS_REL2ABS(backup_cr->buffaddr);
	}
	bsiz = ((blk_hdr_ptr_t)(backup_blk_p))->bsiz;
	sbufh_p = cs_addrs->shmpool_buffer;
	assert(bsiz <= sbufh_p->blk_size);

	/* Obtain block from shared memory pool. If we can't get the block, then backup will be effectively terminated. */
	sblkh_p = shmpool_blk_alloc(gv_cur_region, SHMBLK_BACKUP);
	if (((shmpool_blk_hdr_ptr_t)-1L) == sblkh_p)
		return FALSE;	/* Backup died for whatever reason. Backup failure already dealt with in shmpool_blk_alloc() */

	/* Fill the block we have been assigned in before marking it valid */
	sblkh_p->blkid = blk;
	if (dba_bg == cs_data->acc_meth)
	{
		assert(NULL != backup_cr);
		sblkh_p->use.bkup.ondsk_blkver = backup_cr->ondsk_blkver;
	} else DEBUG_ONLY(if (dba_mm == cs_data->acc_meth))
	        /* For MM version, no dynamic conversions take place so just record block as we know it is */
		sblkh_p->use.bkup.ondsk_blkver = cs_data->desired_db_format;
	DEBUG_ONLY(else assert(FALSE));
	/* Copy blokc information to data portion of shmpool block just following header */
	memcpy((sblkh_p + 1), backup_blk_p, bsiz);
	/* Need a write coherency fence here as we want to make sure the above info is stored and
	   reflected to other processors before we mark the block valid.
	*/
	SHM_WRITE_MEMORY_BARRIER;
	sblkh_p->valid_data = TRUE;
	/* And another write barrier to advertise its cleanliness to other processors */
	SHM_WRITE_MEMORY_BARRIER;

	return TRUE;
}
