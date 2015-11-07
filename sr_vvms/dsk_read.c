/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <descrip.h>
#include <iodef.h>
#include <rms.h>
#include <ssdef.h>
#include "gtm_string.h"
#ifdef DEBUG
#include "gtm_stdio.h"
#endif

#include "cdb_sc.h"
#include "efn.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtm_malloc.h"		/* for CHECK_CHANNEL_STATUS macro */
#include "iosb_disk.h"
#include "iosp.h"
#include "gds_blk_upgrade.h"
#include "gdsbml.h"
#include "gtmimagename.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			update_trans;
GBLREF	volatile int4		fast_lock_count;
GBLREF	boolean_t		dse_running;

error_def(ERR_DYNUPGRDFAIL);

int4 dsk_read(block_id blk, sm_uc_ptr_t buff, enum db_ver *ondsk_blkver, boolean_t blk_free)
{
	int4			status, size;
	io_status_block_disk	iosb;
	enum db_ver		tmp_ondskblkver;
	sm_uc_ptr_t		save_buff = NULL;
	uint4			channel_id;
	boolean_t		fully_upgraded;
	DEBUG_ONLY(
		blk_hdr_ptr_t	blk_hdr;
		static int	in_dsk_read;
	)
	/* It is possible that the block that we read in from disk is a V4 format block.  The database block scanning routines
	 * (gvcst_*search*.c) that might be concurrently running currently assume all global buffers (particularly the block
	 * headers) are V5 format.  They are not robust enough to handle a V4 format block. Therefore we do not want to
	 * risk reading a potential V4 format block directly into the cache and then upgrading it. Instead we read it into
	 * a private buffer, upgrade it there and then copy it over to the cache in V5 format. This is the static variable
	 * read_reformat_buffer. We could have as well used the global variable "reformat_buffer" for this purpose. But
	 * that would then prevent dsk_reads and concurrent dsk_writes from proceeding. We dont want that loss of asynchronocity.
	 * Hence we keep them separate. Note that while "reformat_buffer" is used by a lot of routines, "read_reformat_buffer"
	 * is used only by this routine and hence is a static instead of a GBLDEF.
	 */
	static sm_uc_ptr_t	read_reformat_buffer;
	static int		read_reformat_buffer_len;

	assert(!blk_free); /* VMS should never try to read a FREE block from the disk */
	assert(0 == in_dsk_read);	/* dsk_read should never be nested. the read_reformat_buffer logic below relies on this */
	DEBUG_ONLY(in_dsk_read++;)
	assert(cs_addrs->hdr == cs_data);
	size = cs_data->blk_size;
	assert(cs_data->acc_meth == dba_bg);
	/* Since cs_data->fully_upgraded is referenced more than once in this module (once explicitly and once in
	 * GDS_BLK_UPGRADE_IF_NEEDED macro used below), take a copy of it and use that so all usages see the same value.
	 * Not doing this, for example, can cause us to see the database as fully upgraded in the first check causing us
	 * not to allocate save_buff (a temporary buffer to hold a V4 format block) at all but later in the macro
	 * we might see the database as NOT fully upgraded so we might choose to call the function gds_blk_upgrade which
	 * does expect a temporary buffer to have been pre-allocated. It is ok if the value of cs_data->fully_upgraded
	 * changes after we took a copy of it since we have a buffer locked for this particular block (at least in BG)
	 * so no concurrent process could be changing the format of this block. For MM there might be an issue.
	 */
	fully_upgraded = cs_data->fully_upgraded;
	if (!fully_upgraded)
	{
		save_buff = buff;
		if (size > read_reformat_buffer_len)
		{	/* do the same for the reformat_buffer used by dsk_read */
			assert(0 == fast_lock_count);	/* this is mainline (non-interrupt) code */
			++fast_lock_count;		/* No interrupts in free/malloc across this change */
			if (NULL != read_reformat_buffer)
				free(read_reformat_buffer);
			read_reformat_buffer = malloc(size);
			read_reformat_buffer_len = size;
			--fast_lock_count;
		}
		buff = read_reformat_buffer;
	}
	if (NULL != cs_addrs->nl)	/* could be NULL in case of MUPIP CREATE */
		INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_dsk_read, 1);
	channel_id = ((vms_gds_info*)(gv_cur_region->dyn.addr->file_cntl->file_info))->fab->fab$l_stv;
	status = sys$qiow(efn_bg_qio_read, channel_id, IO$_READVBLK, &iosb, 0, 0,
				buff, size, (size / DISK_BLOCK_SIZE) * blk + cs_data->start_vbn, 0, 0, 0);
	if (SS$_NORMAL != status)
	{
		DEBUG_ONLY(in_dsk_read--;)
		assert(FALSE);
		CHECK_CHANNEL_STATUS(status, channel_id);
		return status;
	}
	status = iosb.cond;
	if (SS$_NORMAL != status)
	{
		DEBUG_ONLY(in_dsk_read--;)
		assert(FALSE);
		return status;
	}
	assert(0 == (long)buff % 2);
	/* GDSV4 (0) version uses "buff->bver" as a block length so should always be > 0 when M code is running.
	 * The only exception is if the block has not been initialized (possible if it is BLK_FREE status in the
	 * bitmap). This is possible due to concurrency issues while traversing down the tree. But if we have
	 * crit on this region, we should not see these either. Assert accordingly.
	 */
	assert(!IS_MCODE_RUNNING || !cs_addrs->now_crit || ((blk_hdr_ptr_t)buff)->bver);
	/* Block must be converted to current version (if necessary) for use by internals.
	 * By definition, all blocks are converted from/to their on-disk version at the IO point.
	 */
	GDS_BLK_UPGRADE_IF_NEEDED(blk, buff, save_buff, cs_data, &tmp_ondskblkver, status, fully_upgraded);
	DEBUG_DYNGRD_ONLY(
		if (GDSVCURR != tmp_ondskblkver)
			PRINTF("DSK_READ: Block %d being dynamically upgraded on read\n", blk);
	)
	assert((GDSV6 == tmp_ondskblkver) || (NULL != save_buff));	/* never read a V4 block directly into cache */
	if (NULL != ondsk_blkver)
		*ondsk_blkver = tmp_ondskblkver;
	/* a bitmap block should never be short of space for a dynamic upgrade. assert that. */
	assert((NULL == ondsk_blkver) || !IS_BITMAP_BLK(blk) || (ERR_DYNUPGRDFAIL != status));
	/* If we didn't run gds_blk_upgrade which would move the block into the cache, we need to do
	 * it ourselves. Note that buff will be cleared by the GDS_BLK_UPGRADE_IF_NEEDED macro if
	 * buff and save_buff are different and gds_blk_upgrade was called.
	 */
	if ((NULL != save_buff) && (NULL != buff))	/* Buffer not moved by upgrade, we must move */
		memcpy(save_buff, buff, size);
	DEBUG_ONLY(
		in_dsk_read--;
		if (cs_addrs->now_crit && !dse_running)
		{	/* Do basic checks on GDS block that was just read. Do it only if holding crit as we could read
			 * uninitialized blocks otherwise. Also DSE might read bad blocks even inside crit so skip checks.
			 */
			blk_hdr = (NULL != save_buff) ? (blk_hdr_ptr_t)save_buff : (blk_hdr_ptr_t)buff;
			GDS_BLK_HDR_CHECK(cs_data, blk_hdr, fully_upgraded);
		}
	)
	if (cs_data->clustered && !cs_addrs->now_crit && ((blk_hdr_ptr_t)buff)->tn > cs_addrs->ti->curr_tn)
	{	/* a future read */
		/* Note !! This clustering code is currently dead as no clustering product currently exists. Should
		   it be resurrected, the test above needs some work as "buff" can be zeroed at this point (even though
		   it is unlikely we would ever support the headache of auto-upgrade in a clustered environment).
		*/
		assert(FALSE);	/* update_trans is relied upon by t_end/tp_tend currently and it is not clear
				 * if setting it to TRUE here is ok. since this is clustering code and is not
				 * currently supported, an assert is added to revisit this once clustering is enabled.
				 */
		update_trans = UPDTRNS_DB_UPDATED_MASK;
		return FUTURE_READ;
	}
	return status;
}
