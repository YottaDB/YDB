/***************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_unistd.h"	/* DB_FSYNC macro needs this */
#include "gtm_string.h"

#include "gtmio.h"	/* this has to come in before gdsfhead.h, for all "open" to be defined
				to "open64", including the open in header files */
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
#include "iosp.h"
#include "gdsbgtr.h"
#include "is_file_identical.h"
#include "dpgbldir.h"
#include "rel_quant.h"
#include "repl_sp.h"	/* for F_CLOSE used by the JNL_FD_CLOSE macro */
#include "memcoherency.h"
#include "gtm_dbjnl_dupfd_check.h"
#include "anticipatory_freeze.h"

GBLREF	volatile int4	db_fsync_in_prog;
GBLREF	volatile int4	jnl_qio_in_prog;
GBLREF	uint4		process_id;

error_def(ERR_DBFSYNCERR);
error_def(ERR_ENOSPCQIODEFER);
error_def(ERR_JNLACCESS);
error_def(ERR_JNLCNTRL);
error_def(ERR_JNLRDERR);
error_def(ERR_JNLWRTDEFER);
error_def(ERR_JNLWRTNOWWRTR);
error_def(ERR_PREMATEOF);

uint4 jnl_sub_qio_start(jnl_private_control *jpc, boolean_t aligned_write);
void jnl_mm_timer_write(void);

/* If the second argument is TRUE, then the jnl write is done only upto the previous aligned boundary.
 * else the write is done upto the freeaddr */

uint4 jnl_sub_qio_start(jnl_private_control *jpc, boolean_t aligned_write)
{
	boolean_t		was_wrapped;
	int			tsz, close_res;
	jnl_buffer_ptr_t	jb;
	int4			free_ptr;
	sgmnt_addrs		*csa;
	node_local_ptr_t	cnl;
	sm_uc_ptr_t		base;
	unix_db_info		*udi;
	unsigned int		status;
	int			save_errno;
	uint4			aligned_dskaddr, dskaddr;
	int4			aligned_dsk, dsk;
	int			aligned_tsz;
	sm_uc_ptr_t		aligned_base;
	uint4			jnl_fs_block_size;
	gd_region		*reg;

	assert(NULL != jpc);
	reg = jpc->region;
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	jb = jpc->jnl_buff;
	if (jb->io_in_prog_latch.u.parts.latch_pid == process_id)	/* We already have the lock? */
		return ERR_JNLWRTNOWWRTR;			/* timer driven io in progress */
	jnl_qio_in_prog++;
	if (!GET_SWAPLOCK(&jb->io_in_prog_latch))
	{
		jnl_qio_in_prog--;
		assert(0 <= jnl_qio_in_prog);
		return ERR_JNLWRTDEFER;
	}
#	ifdef DEBUG
	/* When jnl_sub_qio_start() is called as part of WBTEST_SIGTSTP_IN_JNL_OUTPUT_SP white-box test case,
	 * aligned_write should always be FALSE. But depending upon the filesystem block size, it is possible that
	 * the function could also be called with aligned_write being TRUE. This could lead to sending SIGTSTP
	 * twice. Hence ensure that SIGTSTP is sent only for the unaligned write.
	 */
	if (gtm_white_box_test_case_enabled && (WBTEST_SIGTSTP_IN_JNL_OUTPUT_SP == gtm_white_box_test_case_number)
				&& !aligned_write)
		kill(process_id, SIGTSTP);
#	endif
	if (jb->dsk != (jb->dskaddr % jb->size))
	{
		RELEASE_SWAPLOCK(&jb->io_in_prog_latch);
		jnl_qio_in_prog--;
		assert(0 <= jnl_qio_in_prog);
		return ERR_JNLCNTRL;
	}
	if (!JNL_FILE_SWITCHED(jpc))
		jpc->fd_mismatch = FALSE;
	else
	{	/* journal file has been switched; release io_in_prog lock and return */
		jpc->fd_mismatch = TRUE;
		RELEASE_SWAPLOCK(&jb->io_in_prog_latch);
		jnl_qio_in_prog--;
		assert(0 <= jnl_qio_in_prog);
		return SS_NORMAL;
	}
	/* Currently we overload io_in_prog_latch to perform the db fsync too. Anyone trying to do a
	 *   jnl_qio_start will first check if a db_fsync is needed and if so sync that before doing any jnl qio.
	 * Note that since an epoch record is written when need_db_fsync is set to TRUE, we are guaranteed that
	 *   (dskaddr < freeaddr) which is necessary for the jnl_wait --> jnl_write_attempt mechanism (triggered
	 *   by wcs_flu) to actually initiate a call to jnl_qio_start().
	 */
	if (jb->need_db_fsync)
	{
		DB_FSYNC(reg, udi, csa, db_fsync_in_prog, save_errno);
		GTM_WHITE_BOX_TEST(WBTEST_ANTIFREEZE_DBFSYNCERR, save_errno, EIO);
		if (0 != save_errno)
		{
			RELEASE_SWAPLOCK(&jb->io_in_prog_latch);
			jnl_qio_in_prog--;
			assert(0 <= jnl_qio_in_prog);
			/* DBFSYNCERR can potentially cause syslog flooding. Remove the following line if we it becomes an issue. */
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFSYNCERR, 2, DB_LEN_STR(reg), save_errno);
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFSYNCERR, 2, DB_LEN_STR(reg), save_errno);
			assert(FALSE);	/* should not come here as the rts_error above should not return */
			return ERR_DBFSYNCERR;	/* ensure we do not fall through to the code below as we no longer have the lock */
		}
		jb->need_db_fsync = FALSE;
	}
	free_ptr = jb->free;
        /* The following barrier is to make sure that for the value of "free" that we extract (which may be
         * slightly stale but that is not a correctness issue) we make sure we dont write out a stale version of
         * the journal buffer contents. While it is possible that we see journal buffer contents that are more
         * uptodate than "free", this would only mean writing out a less than optimal number of bytes but again,
         * not a correctness issue. Secondary effect is that it also enforces a corresponding non-stale value of
         * freeaddr is read and this is relied upon by asserts below.
	 */
	SHM_READ_MEMORY_BARRIER;
	dsk = jb->dsk;
	dskaddr = jb->dskaddr;
	was_wrapped = (free_ptr < dsk);
	jnl_fs_block_size = jb->fs_block_size;
	if (aligned_write)
		free_ptr = ROUND_DOWN2(free_ptr, jnl_fs_block_size);
	assert(!(jb->size % jnl_fs_block_size));
	tsz = (free_ptr < dsk ? jb->size : free_ptr) - dsk;
	if ((aligned_write && !was_wrapped && (free_ptr <= dsk)) || (NOJNL == jpc->channel))
		tsz = 0;
	assert(0 <= tsz);
	assert(dskaddr + tsz <= jb->freeaddr);
	status = SS_NORMAL;
	if (tsz)
	{	/* ensure that dsk and free are never equal and we have left space for JNL_WRT_START_MASK */
		assert(SS_NORMAL == status);
		assert((free_ptr > dsk) || (free_ptr < (dsk & JNL_WRT_START_MASK(jb)))
			|| (dsk != (dsk & JNL_WRT_START_MASK(jb))));
		jb->wrtsize = tsz;
		jb->qiocnt++;
		base = &jb->buff[dsk + jb->buff_off];
		assert((base + tsz) <= (jb->buff + jb->size + jnl_fs_block_size));
		assert(NOJNL != jpc->channel);
		/* If sync_io is turned on, we would have turned on the O_DIRECT flag on some platforms. That will
		 * require us to do aligned writes. Both the source buffer and the size of the write need to be aligned
		 * for this to work on some platforms. The alignment needs to be on a filesystem-block-size granularity.
		 * If sync_io is not turned on, doing aligned writes saves us from the OS doing a read of the block
		 * under the covers in case we write only a part of the filesystem block.
		 * Therefore we do aligned writes no matter what. This means we could be writing some garbage padding
		 * data out after the last valid journal record jut to fit in the alignment requirements. But that is
		 * considered okay because as part of writing the EOF record out (for a clean termination), jnl_write
		 * would have 0-padded the journal buffer for us. So a cleanly shutdown journal file will have 0-padding
		 * following the EOF record but an actively used journal file might have garbage padding following the
		 * last valid record. This is considered okay as journal recovery has logic to scan past the garbage and
		 * locate the last valid record in case of a crash before writing the EOF.
		 */
		aligned_dsk = ROUND_DOWN2(dsk, jnl_fs_block_size);
		aligned_dskaddr = ROUND_DOWN2(dskaddr, jnl_fs_block_size);
		aligned_tsz = ROUND_UP2((tsz + (dskaddr - aligned_dskaddr)), jnl_fs_block_size);
		aligned_base = (sm_uc_ptr_t)ROUND_DOWN2((uintszofptr_t)base, jnl_fs_block_size);
		/* Assert that aligned_dsk never backs up to a point BEFORE where the free pointer is */
		assert((aligned_dsk > free_ptr) || (dsk <= free_ptr));
		/* Assert that aligned_dskaddr never backs up to a point inside journal file header territory.
		 * This is because those fields are always updated inside crit and therefore we should
		 * never touch those while we hold only the jnl qio lock.
		 */
		assert(JNL_HDR_LEN <= aligned_dskaddr);
		/* Assert that both ends of the source buffer for the write falls within journal buffer limits */
		assert(aligned_base >= &jb->buff[jb->buff_off]);
		assert(aligned_base + aligned_tsz <= &jb->buff[jb->buff_off + jb->size]);
		JNL_LSEEKWRITE(csa, csa->hdr->jnl_file_name, jpc->channel,
			(off_t)aligned_dskaddr, aligned_base, (size_t)aligned_tsz, jpc->status);
		status = jpc->status;
		if (SS_NORMAL == status)
		{	/* update jnl_buff pointers to reflect the successful write to the journal file */
			assert(dsk <= jb->size);
			assert(jb->io_in_prog_latch.u.parts.latch_pid == process_id);
			jpc->new_dsk = dsk + tsz;
			if (jpc->new_dsk >= jb->size)
			{
				assert(jpc->new_dsk == jb->size);
				jpc->new_dsk = 0;
			}
			jpc->new_dskaddr = dskaddr + tsz;
			assert(jpc->new_dsk == jpc->new_dskaddr % jb->size);
			assert(jb->freeaddr >= jpc->new_dskaddr);
			jpc->dsk_update_inprog = TRUE;	/* for secshr_db_clnup to clean it up (when it becomes feasible in Unix) */
			jb->dsk = jpc->new_dsk;
			jb->dskaddr = jpc->new_dskaddr;
			jpc->dsk_update_inprog = FALSE;
			cnl = csa->nl;
			INCR_GVSTATS_COUNTER(csa, cnl, n_jfile_bytes, aligned_tsz);
			INCR_GVSTATS_COUNTER(csa, cnl, n_jfile_writes, 1);
		} else
		{
			assert((ENOSPC == status) || (ERR_ENOSPCQIODEFER == status));
			jb->errcnt++;
			if (ENOSPC == status)
				jb->enospc_errcnt++;
			else
				jb->enospc_errcnt = 0;

			if (ERR_ENOSPCQIODEFER != status)
			{
				jnl_send_oper(jpc, ERR_JNLACCESS);
				jpc->status = status;	/* set jpc->status back to original error as jnl_send_oper resets
							 * jpc->status to SS_NORMAL. We need it in callers of this function
							 * (e.g. jnl_write_attempt). */
			}
#			ifdef GTM_FD_TRACE
			if ((EBADF == status) || (ESPIPE == status))
			{	/* likely case of D9I11-002714. check if fd is valid */
				gtm_dbjnl_dupfd_check();
				/* If fd of this journal points to some other database or journal file opened by this process
				 * the above call would have reset jpc->channel. If it did not get reset, then check
				 * if the fd in itself is valid and points back to the journal file. If not reset it to NOJNL.
				 */
				if (NOJNL != jpc->channel)
					gtm_check_fd_is_valid(reg, FALSE, jpc->channel);
				/* If jpc->channel still did not get reset to NOJNL, it means the file descriptor is valid but
				 * not sure why we are getting EBADF/ESPIPE errors. No further recovery attempted at this point.
				 */
			}
#			endif
			if (ERR_ENOSPCQIODEFER == status)
				status = ERR_JNLWRTDEFER;
			else
				status = ERR_JNLACCESS;
		}
	}
	RELEASE_SWAPLOCK(&jb->io_in_prog_latch);
	if ((jnl_closed == csa->hdr->jnl_state) && (NOJNL != jpc->channel))
	{
		JNL_FD_CLOSE(jpc->channel, close_res);	/* sets jpc->channel to NOJNL */
		jpc->pini_addr = 0;
	}
	jnl_qio_in_prog--;
	assert(0 <= jnl_qio_in_prog);
	return status;
}

/* This is a wrapper for jnl_sub_qio_start that tries to divide the writes into optimal chunks.
 * It calls jnl_sub_qio_start() with appropriate arguments in two stages, the first one with
 * optimal "jnl_fs_block_size" boundary and the other suboptimal tail end of the write. The latter
 * call is made only if no other process has finished the jnl write upto the required point
 * during the time this process yields
 */
uint4 jnl_qio_start(jnl_private_control *jpc)
{
	unsigned int		yield_cnt, status;
	uint4			target_freeaddr, lcl_dskaddr, old_freeaddr;
	jnl_buffer_ptr_t	jb;
	sgmnt_addrs		*csa;
	unix_db_info		*udi;
	uint4			jnl_fs_block_size;

	assert(NULL != jpc);
	udi = FILE_INFO(jpc->region);
	csa = &udi->s_addrs;
	jb = jpc->jnl_buff;
	/* this block of code (till yield()) processes the buffer upto an "jnl_fs_block_size" alignment boundary
	 * and the next block of code (after the yield()) processes the tail end of the data (if necessary)
	 */
	lcl_dskaddr = jb->dskaddr;
	target_freeaddr = jb->freeaddr;
	if (lcl_dskaddr >= target_freeaddr)
		return SS_NORMAL;
	/* ROUND_DOWN2 macro is used under the assumption that "jnl_fs_block_size" would be a power of 2 */
	jnl_fs_block_size = jb->fs_block_size;
	if (ROUND_DOWN2(lcl_dskaddr, jnl_fs_block_size) != ROUND_DOWN2(target_freeaddr, jnl_fs_block_size))
	{	/* data crosses/touches an alignment boundary */
		if (SS_NORMAL != (status = jnl_sub_qio_start(jpc, TRUE)))
			return status;
	} /* else, data does not cross/touch an alignment boundary, yield and see if someone else
	   * does the dirty job more efficiently
	   */
	for (yield_cnt = 0; yield_cnt < csa->hdr->yield_lmt; yield_cnt++)
	{	/* yield() until someone has finished your job or no one else is active on the jnl file */
		old_freeaddr = jb->freeaddr;
		rel_quant();
		/* Purpose of this memory barrier is to get a current view of asyncrhonously changed fields
		 * like whether the jnl file was switched, the write position in the journal file and the
		 * write address in the journal buffer for all the remaining statements in this loop because
		 * the rel_quant call above allows any and all of them to change and we aren't under any
		 * locks while in this loop. This is not a correctness issue as we would either eventually
		 * see the updates or it means we are writing what has already been written. It is a performance
		 * issue keeping more current with state changes done by other processes on other processors.
		 */
		SHM_READ_MEMORY_BARRIER;
		if (JNL_FILE_SWITCHED(jpc))
			return SS_NORMAL;
		/* assert(old_freeaddr <= jb->freeaddr) ** Potential race condition with jnl file switch could
		 * make this assert fail so it is removed
		 */
		if (old_freeaddr == jb->freeaddr || target_freeaddr <= jb->dskaddr)
			break;
	}
	status = SS_NORMAL;
	if (target_freeaddr > jb->dskaddr)
		status = jnl_sub_qio_start(jpc, FALSE);
	return status;
}

static boolean_t	jnl_timer;
void jnl_mm_timer_write(void)
{	/* While this should work by region and use baton passing to more accurately and efficiently perform its task,
	 * it is currently a blunt instrument
	 */
	gd_region	*reg, *r_top;
	gd_addr		*addr_ptr;
	sgmnt_addrs	*csa;

	for (addr_ptr = get_next_gdr(NULL);  NULL != addr_ptr;  addr_ptr = get_next_gdr(addr_ptr))
	{	/* since the unix timers don't provide an argument, for now write all regions */
		for (reg = addr_ptr->regions, r_top = reg + addr_ptr->n_regions;  reg < r_top; reg++)
		{
			if ((dba_mm == reg->dyn.addr->acc_meth) && reg->open)
			{
				csa = &FILE_INFO(reg)->s_addrs;
				if ((NULL != csa->jnl) && (NOJNL != csa->jnl->channel))
					jnl_qio_start(csa->jnl);
			}
		}
	}
	jnl_timer = FALSE;
	return;
}

void jnl_mm_timer(sgmnt_addrs *csa, gd_region *reg)
{	/* While this should work by region and use baton passing to more accurately and efficiently perform its task,
	 * it is currently a blunt instrument.
	 */
	assert(reg->open);
	if (FALSE == jnl_timer)
	{
		jnl_timer = TRUE;
		start_timer((TID)jnl_mm_timer, csa->hdr->flush_time[0], &jnl_mm_timer_write, 0, NULL);
	}
	return;
}
