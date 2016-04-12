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

#include "gtm_stat.h"
#include <errno.h>
#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmio.h"
#include "util.h"
#include "memcoherency.h"
#include "shmpool.h"
#include "gtmimagename.h"
#include "mupipbckup.h"
#include "send_msg.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

GBLREF	uint4			process_id;

error_def(ERR_BKUPTMPFILOPEN);
error_def(ERR_BKUPTMPFILWRITE);
error_def(ERR_TEXT);
ZOS_ONLY(error_def(ERR_BADTAG);)

boolean_t backup_buffer_flush(gd_region *reg)
{
	int			write_size, fd;
	int4			status;
	sgmnt_addrs		*csa;
	shmpool_buff_hdr_ptr_t	sbufh_p;
	shmpool_blk_hdr_ptr_t	sblkh_p, next_sblkh_p;
	DEBUG_ONLY(int		flush_cnt;)
	ZOS_ONLY(int		realfiletag;)
	int			muinc_adjust;

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
	 * the IO we will be doing. This simplifies the backup logic substantialy. If we released and obtained
	 * the lock for each buffer we dequeue (to allow other processes to proceed while we are doing IO) it
	 * is likely that some of those other processes would get the idea to also run a buffer flush. Then we
	 * would have to manage the task of doing multiple simultaneous IO to the temporary file potentially
	 * resulting in gaps in the file which is something we definitely do not want to do. Besides, if a backup
	 * is going on (and thus causing the flush) we are likely doing this in crit which is holding up all
	 * other processes anyway so we aren't losing much if anything. This is also historically how this
	 * has been done to assure the robustness of the temporary file. SE 1/2005.
	 */
	if (0 < sbufh_p->backup_cnt)
	{	/* open the file, write to it at the address and close the file */
		OPENFILE(sbufh_p->tempfilename, O_RDWR, fd);
		if (FD_INVALID == fd)
		{
			sbufh_p->failed = process_id;
			sbufh_p->backup_errno = errno;
			csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
			send_msg(VARLSTCNT(5) ERR_BKUPTMPFILOPEN, 2, LEN_AND_STR(sbufh_p->tempfilename), sbufh_p->backup_errno);
			assert(EACCES == sbufh_p->backup_errno);
			shmpool_unlock_hdr(reg);
			return FALSE;
		}
#ifdef __MVS__
		if (-1 == gtm_zos_tag_to_policy(fd, TAG_BINARY, &realfiletag))
			TAG_POLICY_SEND_MSG(sbufh_p->tempfilename, errno, realfiletag, TAG_BINARY);
#endif
		/* This adjustment is necessary since the shmpool_blk_hdr had been previously tied to the backup format.
		 * We now have a separate structure for the backup. muinc_adjust is used to translate in a
		 * performance sensitive-way. Currently, the structures only differ by 8 bytes at the front which is
		 * not relevant to the backup and only on 64bit platforms. Asserts are added to assure the design
		 * assumptions are not violated.
		 */
		muinc_adjust = SIZEOF(*sblkh_p) - SIZEOF(muinc_blk_hdr_t);
		NON_GTM64_ONLY(assert(0 == muinc_adjust));
		GTM64_ONLY(assert(8 == muinc_adjust));
		write_size = SIZEOF(muinc_blk_hdr_t) + sbufh_p->blk_size;
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
			/* This block has valid data. Flush it first, then dequeue it. It won't hurt if this
			   process faile between the time that it starts the IO and it dequeues the block. The
			   worst that would happen is the block would be in the temporary file twice which, while
			   a little annoying is not functionally incorrect. If we dequeue it first though, there is
			   a possibility that the IO could be lost and an invalid block written to the temporary file
			   or missed altogether.
			*/
			LSEEKWRITE(fd, sbufh_p->dskaddr, ((char *)sblkh_p + muinc_adjust), write_size, status);
			if (0 != status)
			{
				sbufh_p->failed = process_id;
				sbufh_p->backup_errno = status;
				csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
				assert(FALSE);
				send_msg(VARLSTCNT(8) ERR_BKUPTMPFILWRITE, 2, LEN_AND_STR(sbufh_p->tempfilename), status);
				break;	/* close file, release lock and return now.. */
			}
			/* Update disk addr with record just written */
			sbufh_p->dskaddr += write_size;
			/* Now we can deque this entry from the backup queue safely and release it */
			shmpool_blk_free(reg, sblkh_p);
		}
		CLOSEFILE_RESET(fd, status);	/* resets "fd" to FD_INVALID */
	}
	shmpool_unlock_hdr(reg);
	return TRUE;
}
