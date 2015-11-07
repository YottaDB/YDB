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

#include <rms.h>
#include <iodef.h>
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "efn.h"
#include "util.h"
#include "memcoherency.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "iormdef.h"
#include "shmpool.h"
#include "gtmimagename.h"
#include "mupipbckup.h"
#include "send_msg.h"

GBLREF	int			process_id;

error_def(ERR_BKUPTMPFILOPEN);
error_def(ERR_BKUPTMPFILWRITE);

/* Return true if flush attempted, false if lock not obtained or other error */
boolean_t backup_buffer_flush(gd_region *reg)
{
	int4			status, write_size, write_len, lcnt;
	sgmnt_addrs		*csa;
	shmpool_buff_hdr_ptr_t	sbufh_p;
	shmpool_blk_hdr_ptr_t	sblkh_p, next_sblkh_p;
	boolean_t		multiple_writes;
	sm_uc_ptr_t		write_ptr;

	struct FAB		fab;
	struct NAM		nam;
	struct RAB		rab;
	VMS_ONLY(int		flush_cnt);

	csa = &FILE_INFO(reg)->s_addrs;
	sbufh_p = csa->shmpool_buffer;

	if (!shmpool_lock_hdr_nowait(reg))
	{
#ifdef DEBUG
		/* someone else is flushing it right now */
		if (!IS_GTM_IMAGE)
			util_out_print("Process !12UL has the shmpool lock preventing backup buffer flush.",
				TRUE, sbufh_p->shmpool_crit_latch.u.parts.latch_pid);
#endif
		return FALSE;
	}


	if (0 != sbufh_p->backup_errno)
	{	/* Since this is signal/mupip initiated, the proper message will be (or already has been) output on exit. */
		shmpool_unlock_hdr(reg);
		return FALSE;		/* Async error state change (perhaps mupip stop) -- nothing to do if backup is dying */
	}

	/* See if there are any buffers needing flushing. Note that we are holding the shmpool lock across
	   the IO we will be doing. This simplifies the backup logic substantialy. If we released and obtained
	   the lock for each buffer we dequeue (to allow other processes to proceed while we are doing IO) it
	   is likely that some of those other processes would get the idea to also run a buffer flush. Then we
	   would have to manage the task of doing multiple simultaneous IO to the temporary file potentially
	   resulting in gaps in the file which is something we definitely do not want to do. Besides, if a backup
	   is going on (and thus causing the flush) we are likely doing this in crit which is holding up all
	   other processes anyway so we aren't losing much if anything. This is also historically how this
	   has been done to assure the robustness of the temporary file. SE 1/2005.
	*/
	if (0 < sbufh_p->backup_cnt)
	{	/* open the file, write to it at the address and close the file */
		fab = cc$rms_fab;
		fab.fab$b_fac = FAB$M_PUT;
		fab.fab$l_fna = sbufh_p->tempfilename;
		fab.fab$b_fns = strlen(sbufh_p->tempfilename);
		rab = cc$rms_rab;
                rab.rab$l_fab = &fab;
                rab.rab$l_rop = RAB$M_WBH | RAB$M_EOF;

		for (lcnt = 1;  MAX_OPEN_RETRY >= lcnt;  lcnt++)
		{
			if (RMS$_FLK != (status = sys$open(&fab, NULL, NULL)))
				break;
			wcs_sleep(lcnt);
		}

		if ((RMS$_NORMAL != status) || (RMS$_NORMAL != (status = sys$connect(&rab))))
		{	/* Unable to open temporary file */
			sbufh_p->backup_errno = status;
			sbufh_p->failed = process_id;
			csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
			send_msg(VARLSTCNT(5) ERR_BKUPTMPFILOPEN, 2, LEN_AND_STR(sbufh_p->tempfilename), sbufh_p->backup_errno);
			shmpool_unlock_hdr(reg);
			return FALSE;
		}

		DEBUG_ONLY(flush_cnt = 0);
		for (sblkh_p = SBLKP_REL2ABS(&sbufh_p->que_backup, fl);
		     sblkh_p != (shmpool_blk_hdr_ptr_t)&sbufh_p->que_backup;
		     sblkh_p = next_sblkh_p)
		{	/* Loop through the queued backup blocks */
			DEBUG_ONLY(++flush_cnt);
			VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_free);
			VERIFY_QUEUE((que_head_ptr_t)&sbufh_p->que_backup);
			next_sblkh_p = SBLKP_REL2ABS(sblkh_p, fl);	/* Get next offset now in case remove entry */
			/* Need read fence for checking if block has valid data or not since these
			   fields are not set under lock */
			SHM_READ_MEMORY_BARRIER;
			assert(SHMBLK_BACKUP == sblkh_p->blktype);
			if (!sblkh_p->valid_data)
				continue;
			assert(sbufh_p->blk_size >= ((blk_hdr_ptr_t)(sblkh_p + 1))->bsiz);	/* Still validly sized blk? */
			/* This block has valid data. Flush it first, then dequeue it. It won't hurt if this
			   process fails between the time that it starts the IO and it dequeues the block. The
			   worst that would happen is the block would be in the temporary file twice which, while
			   a little annoying is not functionally incorrect. If we dequeue it first though, there is
			   a possibility that the IO could be lost and an invalid block written to the temporary file
			   or missed altogether.
			*/
			write_size = SIZEOF(*sblkh_p) + sbufh_p->blk_size;	/* Assume write hdr/data in one block */
			write_ptr = (sm_uc_ptr_t)sblkh_p;
			while (write_size)
			{	/* Our block + hdr would exceed the 32K max. Write it in two writes as necessary. Since this
				   is standard buffered IO, this is not as big a deal as it could be hence we don't go
				   crazy with our own buffering scheme.
				*/
				write_len = MIN(MAX_RMS_RECORDSIZE, write_size);
				rab.rab$l_rbf = write_ptr;
				rab.rab$w_rsz = write_len;
				if (RMS$_NORMAL != (status = sys$put(&rab)))
				{
					sbufh_p->backup_errno = status;
					sbufh_p->failed = process_id;
					csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
					send_msg(VARLSTCNT(5) ERR_BKUPTMPFILWRITE, 2, LEN_AND_STR(sbufh_p->tempfilename),
						 sbufh_p->backup_errno);
					break;
				}
				write_ptr += write_len;
				write_size -= write_len;
			}
			if (sbufh_p->backup_errno)
				break;
			/* Update disk addr with record just written */
			sbufh_p->dskaddr += (SIZEOF(*sblkh_p) + sbufh_p->blk_size);
			/* Now we can deque this entry from the backup queue safely and release it */
			shmpool_blk_free(reg, sblkh_p);
		}
		sys$close(&fab);
	}
	shmpool_unlock_hdr(reg);
	return TRUE;
}
