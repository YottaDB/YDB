/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

boolean_t backup_block(sgmnt_addrs *csa, block_id blk, cache_rec_ptr_t backup_cr, sm_uc_ptr_t backup_blk_p)
{
	uint4 			bsiz;
	int4			required;
	shmpool_buff_hdr_ptr_t	sbufh_p;
	shmpool_blk_hdr_ptr_t	sblkh_p;
	boolean_t		ret = TRUE, is_bg;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		bkp_src_blk;


	csd = csa->hdr;
	is_bg = (dba_bg == csd->acc_meth);
	assert(is_bg || (dba_mm == csd->acc_meth));
	/* Should have EITHER backup cr (BG mode) or buffer pointer (MM mode) */
	assert((is_bg && (NULL != backup_cr) && (NULL == backup_blk_p)) ||
	       (!is_bg && (NULL == backup_cr) && (NULL != backup_blk_p)));
	if (is_bg)
	{	/* Get buffer address from the cache record */
		VMS_ONLY(assert(0 == backup_cr->shmpool_blk_off));
		assert(backup_cr->in_cw_set);/* ensure the buffer has been pinned (from preemption in db_csh_getn) */
		backup_blk_p = GDS_ANY_REL2ABS(csa, backup_cr->buffaddr);
	}
	bsiz = ((blk_hdr_ptr_t)(backup_blk_p))->bsiz;
	sbufh_p = csa->shmpool_buffer;
	assert(bsiz <= sbufh_p->blk_size);

	/* Obtain block from shared memory pool. If we can't get the block, then backup will be effectively terminated. */
	sblkh_p = shmpool_blk_alloc(csa->region, SHMBLK_BACKUP);
	if (((shmpool_blk_hdr_ptr_t)-1L) == sblkh_p)
		return FALSE;	/* Backup died for whatever reason. Backup failure already dealt with in shmpool_blk_alloc() */

	/* Fill the block we have been assigned in before marking it valid */
	sblkh_p->blkid = blk;
	if (is_bg)
	{
		assert(NULL != backup_cr);
		sblkh_p->use.bkup.ondsk_blkver = backup_cr->ondsk_blkver;
		assert(((blk_hdr_ptr_t)backup_blk_p)->bsiz >= SIZEOF(blk_hdr));
	} else /* For MM version, no dynamic conversions take place so just record block as we know it is */
		sblkh_p->use.bkup.ondsk_blkver = csd->desired_db_format;
	bkp_src_blk = backup_blk_p;
#	ifdef GTM_CRYPT
	/* If the database is encrypted, the old_block will be in the encrypted twin buffer. Fetch it from the encrypted
	 * twin counter part and write that to the backup buffer instead.
	 */
	if (csd->is_encrypted)
	{
		DBG_ENSURE_PTR_IS_VALID_GLOBUFF(csa, csd, backup_blk_p);
		bkp_src_blk = GDS_ANY_ENCRYPTGLOBUF(backup_blk_p, csa);
		DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(csa, csd, bkp_src_blk);
	}
#	endif
	/* Adjust bsiz to be within database block size range. Asserts above will ensure this IS the case for DBG. */
	if (bsiz < SIZEOF(blk_hdr))
		bsiz = SIZEOF(blk_hdr);
	else if (bsiz > sbufh_p->blk_size)
		bsiz = sbufh_p->blk_size;
	/* Copy block information to data portion of shmpool block just following header */
	memcpy((sblkh_p + 1), bkp_src_blk, bsiz);
	/* Need a write coherency fence here as we want to make sure the above info is stored and
	 * reflected to other processors before we mark the block valid.
	 */
	SHM_WRITE_MEMORY_BARRIER;
	sblkh_p->valid_data = TRUE;
	/* And another write barrier to advertise its cleanliness to other processors */
	SHM_WRITE_MEMORY_BARRIER;

	return TRUE;
}
