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

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "aswp.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gt_timer.h"
#include "jnl.h"
#include "lockconst.h"
#include "interlock.h"
#include "gtmio.h"
#include "util.h"
#include "mupipbckup.h"

GBLREF	uint4	process_id;
GBLREF	bool	run_time;

void backup_buffer_flush(gd_region *reg)
{
	int			n, fd;
	int4			free;
	uint4			status;
	sm_uc_ptr_t		base;
	sgmnt_addrs		*csa;
	backup_buff_ptr_t	bptr;

	csa = &FILE_INFO(reg)->s_addrs;
	bptr = csa->backup_buffer;

	if (!GET_SWAPLOCK(&bptr->backup_ioinprog_latch))
	{
#ifdef DEBUG
		/* someone else is flushing it right now */
		if (!run_time)
			util_out_print("Process !12UL is flushing the backup buffers right now.",
				TRUE, bptr->backup_ioinprog_latch.latch_pid);
#endif
		return;
	}

	/* save a copy of free, because it might be modified while we are here
	 * and use bptr->disk because this is the only place it could be modified
	 */
	free = bptr->free;
	n = (free < bptr->disk ? bptr->size : free) - bptr->disk;
	if (n)
	{
		base = &bptr->buff[bptr->disk];
		assert((free > bptr->disk) || (free < bptr->disk));

		/* open the file, write to it at the address and close the file */
		OPENFILE3(bptr->tempfilename, O_RDWR, 0660, fd);
		if (-1 == fd)
		{
			bptr->failed = process_id;
			bptr->backup_errno = errno;
			csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
			RELEASE_SWAPLOCK(&bptr->backup_ioinprog_latch);
			return;
		}
		/* modify bptr->dskaddr according to the return of write	*/
		LSEEKWRITE(fd, (off_t)bptr->dskaddr, (sm_uc_ptr_t)base, n, status);
		if (0 != status)
		{
			bptr->failed = process_id;
			bptr->backup_errno = status;
			csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
			RELEASE_SWAPLOCK(&bptr->backup_ioinprog_latch);
			return;
		}
		CLOSEFILE(fd, status);
		bptr->dskaddr += n;
		bptr->disk += n;
		assert(bptr->disk <= bptr->size);
        	if (bptr->disk == bptr->size)
                	bptr->disk = 0;
	}
	assert(bptr->disk < bptr->size);
	assert(process_id == bptr->backup_ioinprog_latch.latch_pid);
	RELEASE_SWAPLOCK(&bptr->backup_ioinprog_latch);
	return;
}
