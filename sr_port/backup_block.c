/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <fcntl.h>
#include <unistd.h>

#if defined(VMS)
#include <rms.h>
#elif defined(UNIX)
#include "aswp.h"
#include "lockconst.h"
#else
#error UNSUPPORTED PLATFORM
#endif
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "mupipbckup.h"
#include "interlock.h"
#include "sleep_cnt.h"
#include "copy.h"
#include "wcs_sleep.h"


GBLREF	uint4		process_id;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gd_region	*gv_cur_region;

/* requires that cs_addrs and gv_cur_region pointing to the right value */

bool backup_block(block_id blk, sm_uc_ptr_t blk_ptr)
{
	int 			bsize, available, counter, first, diff;
	int4			required;
	backup_buff_ptr_t	buf_ptr;
	backup_blk_ptr_t	rec_ptr;
	char			local_buffer[sizeof(int4) + sizeof(block_id)];
	boolean_t		ret = TRUE;

	error_def(ERR_BCKUPBUFLUSH);

	/* assuring crit, so only one instance of this module could be running at any time */
	assert(cs_addrs->now_crit);

	bsize = ((blk_hdr_ptr_t)(blk_ptr))->bsiz;
	if ((bsize > cs_addrs->hdr->blk_size) || (bsize <= 0)) /* database probably sick, try to save more info */
		bsize = cs_addrs->hdr->blk_size;
	required = bsize + sizeof(int4) + sizeof(block_id);
	buf_ptr = cs_addrs->backup_buffer;
	rec_ptr = (backup_blk_ptr_t)(&(buf_ptr->buff[buf_ptr->free]));

	/* make sure we have enough space first, if not, flush backup buffer now (in crit) */

	for (counter = 1; ; counter++)
	{
		available = buf_ptr->disk - buf_ptr->free - 1;
     		if (0 > available)
     			available += buf_ptr->size;

		if (required <= available)
			break;

		if (buf_ptr->backup_errno)
		{
			assert(cs_addrs->nl->nbb == BACKUP_NOT_IN_PROGRESS);
			return FALSE;
		}

		if (counter > MAX_BACKUP_FLUSH_TRY)
		{
			/* having problem flushing, give it up */
#if defined(UNIX)
			if (GET_SWAPLOCK(&buf_ptr->backup_ioinprog_latch))
			{
				if ((0 == buf_ptr->failed))
				{
					buf_ptr->failed = process_id;
					buf_ptr->backup_errno = ERR_BCKUPBUFLUSH;
				}
				RELEASE_SWAPLOCK(&buf_ptr->backup_ioinprog_latch);
			}
#elif defined(VMS)
			if (1 != bsi(&buf_ptr->backup_ioinprog_latch.latch_word))
			{
				if (0 == buf_ptr->failed)
				{
					buf_ptr->failed = process_id;
					buf_ptr->backup_errno = ERR_BCKUPBUFLUSH;
				}
				bci(&buf_ptr->backup_ioinprog_latch.latch_word);
			}
#else
#error UNSUPPORTED PLATFORM
#endif
			else
			{
				/* out of design situation -- couldn't get lock */
				assert(FALSE);
			}
			cs_addrs->nl->nbb = BACKUP_NOT_IN_PROGRESS;
			return FALSE;

		}

		backup_buffer_flush(gv_cur_region);

		available = buf_ptr->disk - buf_ptr->free - 1;
     		if (0 > available)
     			available += buf_ptr->size;

    		if (required <= available)
    			break;

		wcs_sleep(counter);
	}

	/* save the block and modify the pointer free */

	first = buf_ptr->size - buf_ptr->free;

	if (0 > (diff = first - sizeof(int4) - sizeof(block_id)))
	{
		diff = -diff;
		memcpy(&(local_buffer[0]), &required, sizeof(int4));
		memcpy(&(local_buffer[sizeof(int4)]), &blk, sizeof(block_id));
		memcpy(&(buf_ptr->buff[buf_ptr->free]), &(local_buffer[0]), first);
		memcpy(&(buf_ptr->buff[0]), &(local_buffer[first]), diff);
		memcpy(&(buf_ptr->buff[diff]), blk_ptr, bsize);
	}
	else
	{
		GET_LONG(rec_ptr->rsize, &required);
		GET_LONG(rec_ptr->id, &blk);
		if (first < required)
		{
			if (diff != 0)
				memcpy(&(rec_ptr->bptr[0]), blk_ptr, diff);
			memcpy(&(buf_ptr->buff[0]), blk_ptr + diff, required - first);
		}
		else
		{
			memcpy(&(rec_ptr->bptr[0]), blk_ptr, bsize);
		}
	}

	if (first > required)
		buf_ptr->free += required;
	else
		buf_ptr->free = required - first;

	return TRUE;
}
