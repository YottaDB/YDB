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

#include <iodef.h>
#include <psldef.h>
#include <efndef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "efn.h"
#include "locks.h"
#include "crit_wake.h"
#include "memcoherency.h"
#include "wbox_test_init.h"

GBLREF short	astq_dyn_avail;
GBLREF uint4	image_count;
GBLREF uint4	process_id;

error_def(ERR_JNLACCESS);
error_def(ERR_JNLACCESS);
error_def(ERR_JNLWRTDEFER);
error_def(ERR_JNLWRTNOWWRTR);

uint4 jnl_qio_start(jnl_private_control *jpc)
{
	uint4		new_dskaddr, status;

	assert(!lib$ast_in_prog());
	/* mainline entry can't go to the ast directly to avoid being recursed upon by wcs_wtstart */
	if (!jpc->qio_active)
	{	/* only try the ast if this process doesn't have a write in progress */
		new_dskaddr = jpc->new_dskaddr;
		status = jnl_permit_ast(jpc);
		if (SS$_NORMAL != status)
			jnl_send_oper(jpc, ERR_JNLACCESS);
		else if (!jpc->qio_active && jpc->jnl_buff->io_in_prog)
			status = ERR_JNLWRTDEFER;
	} else
		status = ERR_JNLWRTNOWWRTR;
	return (status);
}

uint4 jnl_permit_ast(jnl_private_control *jpc)
{

	/* this extra level exists to provide a return that may permit the ast to process, thus making status info available */
	assert(!lib$ast_in_prog());
	return (sys$dclast(jnl_start_ast, jpc, PSL$C_USER));
}

void jnl_start_ast(jnl_private_control *jpc)
{
	void			jnl_qio_end();
	bool			bsi(), bci();
	int			tsz;
	int4			free;
	uint4			status;
	sm_uc_ptr_t		base;
	jnl_buffer_ptr_t	jb;

	assert(lib$ast_in_prog());
	if (!jpc->qio_active && (NOJNL != jpc->channel))
	{	/* no jnl io inprogress by this process and haven't lost the file through an operator close */
		jb = jpc->jnl_buff;
		if (FALSE == bsi(&jb->io_in_prog))	/* jnl writes are single threaded */
		{	/* got jb write exclusive ownership */
			if (jb->blocked && jb->blocked != process_id)
			{	/* Blocking requires special handling to prevent deadlocks since ASTs can't be
				 * nested. The process that is currently blocked knows to issue the right jnl-qio
				 * commands (taking care that the qio gets cleaned up without deadlocking). But
				 * if non-blocked processes are allowed to take up io_in_prog ownership, deadlocks
				 * can occur. e.g. P1 takes up io_in_prog of region R1 and enters into a
				 * wcs_wipchk_ast() for region R2. P2 in the meantime takes up io_in_prog for
				 * region R2 and enters into wcs_wipchk_ast() for region R1.
				 */
				bci(&jb->io_in_prog);
				return;
			}
			if (JNL_FILE_SWITCHED(jpc))
			{	/* don't know how we can have an old journal file open if we have dirty data in the journal buffer,
				 * but it is safer to handle this case by not doing the qio (D9C12-002266) so we do not corrupt
				 * both old (because we overwrite) and current journal file (because we miss out on writing some
				 * data). if we do not have any data left in the journal buffer to write, it is possible that we
				 * come here if we get interrupted in jnl_file_close() (by wcs_stale() which results in a call to
				 * jnl_qio_start()) after the increment of jb->cycle but before jpc->channel got set to NOJNL. In
				 * this case, doing nothing is the right thing to do as all jnl data flushing has already happened.
				 */
				assert((jb->dskaddr == jb->freeaddr)
					|| (gtm_white_box_test_case_enabled
						&& (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number)));
				jpc->fd_mismatch = TRUE;
				bci(&jb->io_in_prog);	/* release lock and return */
				return;
			} else
				jpc->fd_mismatch = FALSE;
			/* Take a copy of the shared dsk* fields while holding the io_in_prog lock. This is used by
			 * jnl_write_attempt to determine if there is a JNLCNTRL error with dskaddr/dsk.
			 */
			jpc->new_dskaddr = jb->dskaddr;
			jpc->new_dsk = jb->dsk;
			if ((jb->dskaddr % jb->size) != jb->dsk)
			{	/* This is a JNLCNTRL error but since we are inside an AST we cannot return any non-zero value.
				 * We expect caller (jnl_write_attempt) to detect this from the copy of new_dskaddr/new_dsk.
				 */
				bci(&jb->io_in_prog);	/* release lock and return */
				return;
			}
			/* Start jnl qio */
			jpc->qio_active = TRUE;
			jb->image_count = image_count;
			jb->now_writer = process_id;
			free = jb->free;
			/* The following barrier is to make sure that for the value of "free" that we extract (which may be
			   slightly stale but that is not a correctness issue) we make sure we dont write out a stale version of
			   the journal buffer contents. While it is possible that we see journal buffer contents that are more
			   uptodate than "free", this would only mean writing out a less than optimal number of bytes but again,
			   not a correctness issue. Secondary effect is that it also enforces a corresponding non-stale value of
			   freeaddr is read and this is relied upon by asserts below.
			*/
 			SHM_READ_MEMORY_BARRIER;
			tsz = (free < jb->dsk ? jb->size : free) - jb->dsk;
			assert(0 <= tsz);
			assert(jb->dskaddr + tsz <= jb->freeaddr);
			if (tsz)
			{	/* ensure that dsk and free are never equal and we have left space for JNL_WRT_START_MASK */
				assert((free > jb->dsk) || (free < (jb->dsk & JNL_WRT_START_MASK(jb)))
						|| (jb->dsk != (jb->dsk & JNL_WRT_START_MASK(jb))));
				assert(DISK_BLOCKS_SUM(jb->dskaddr, tsz) <= jb->filesize);
				jb->qiocnt++;
				jb->wrtsize = tsz;
				tsz += (jb->dsk - (jb->dsk & JNL_WRT_START_MASK(jb)));	/* back up to block boundary */
				assert(0 == jb->buff_off);	/* buff_off should have been set to 0 in jnl_file_open_common.c */
				base = &jb->buff[jb->dsk & JNL_WRT_START_MASK(jb)];
				assert(tsz >= jb->wrtsize);
				assert(base >= jb->buff);
				assert(base < (jb->buff + jb->size));
				tsz = (tsz + ~JNL_WRT_END_MASK) & JNL_WRT_END_MASK;	/* round-up to quad-word boundary */
				if (tsz > jb->max_write_size)
				{
					tsz = jb->max_write_size;
					jb->wrtsize = tsz - ((jb->buff + jb->dsk) - base);
				}
				assert((base + tsz) <= (jb->buff + jb->size));
				if (jb->blocked != process_id)
				{
					status = sys$qio(efn_jnl, jpc->channel, IO$_WRITEVBLK, &jb->iosb, jnl_qio_end, jpc,
						base, tsz, jb->dskaddr/DISK_BLOCK_SIZE + 1, 0, 0, 0);/* 1st block is designated 1 */
					if (0 == (status & 1))
					{	/*modify iosb so that qio_end will process the error*/
						assert(FALSE);
						jb->iosb.cond = status;
						jb->iosb.length = 0;
						jb->iosb.dev_specific = 0;
						jnl_qio_end(jpc);
					}
				} else
				{	/* Do qiow */
					status = sys$qiow(EFN$C_ENF, jpc->channel, IO$_WRITEVBLK, &jb->iosb, NULL, 0,
						base, tsz, jb->dskaddr/DISK_BLOCK_SIZE + 1, 0, 0, 0);
					if (0 == (status & 1))
					{
						assert(FALSE);
						jb->iosb.cond = status;
						jb->iosb.length = 0;
						jb->iosb.dev_specific = 0;
					}
					jnl_qio_end(jpc);
				}
			} else
			{	/* nothing left to write */
				jb->wrtsize = 0;
				jb->iosb.cond = 1;
				jnl_qio_end(jpc);		/* qio_active semaphore prevents need for dclast */
			}
		}	/* if got jb write exclusive ownersip */
	}	/* if no io in progress and have a channel */
	return;
}

void jnl_qio_end(jnl_private_control *jpc)
{
	bool			bci();
	uint4			wake_pid;
	jnl_buffer		*jb;
	sgmnt_addrs		*csa;
	node_local_ptr_t	cnl;

	if (FALSE == jpc->qio_active)
		return;			/* during exi_rundown, secshr_db_clnup may have effectively "cancelled" the io */
	jb = jpc->jnl_buff;
	assert(jb->io_in_prog);
	csa = &FILE_INFO(jpc->region)->s_addrs;
	if (0 == (jb->iosb.cond & 1))
	{
		jb->errcnt++;
		jnl_send_oper(jpc, ERR_JNLACCESS);
	} else
	{
		assert(jb->dsk <= jb->size);
		assert(jb->freeaddr >= jb->dskaddr);
		jpc->new_dsk = jb->dsk + jb->wrtsize;
		if (jpc->new_dsk >= jb->size)
			jpc->new_dsk = 0;
		jpc->new_dskaddr = jb->dskaddr + jb->wrtsize;
		assert(jb->freeaddr >= jpc->new_dskaddr);
		jpc->dsk_update_inprog = TRUE;
		jb->dsk = jpc->new_dsk;
		jb->dskaddr = jpc->new_dskaddr;
		jpc->dsk_update_inprog = FALSE;
		assert(jb->freeaddr >= jb->dskaddr);
		cnl = csa->nl;
		INCR_GVSTATS_COUNTER(csa, cnl, n_jfile_bytes, jb->wrtsize);
		INCR_GVSTATS_COUNTER(csa, cnl, n_jfile_writes, 1);
	}
	jb->iosb.cond = -2;		/* don't leave success set */
	jb->now_writer = 0;		/* NOTE: The order of these lines is necessary for concurrency control	*/
	jpc->qio_active = FALSE;
	bci(&jb->io_in_prog);
	wake_pid = jb->blocked;
	if (0 != wake_pid)
		crit_wake(&wake_pid);
	/* If we dont have crit and journaling state has been turned to OFF concurrently, need to free up our journal
	 * resources for the crit holding process (currently either switching journals or turning journaling OFF) to
	 * proceed. If we hold crit though and free up the journal resources as part of this interrupt routine (jnl_qio_end),
	 * it is possible that mainline code (one that is switching journals or turning journaling OFF e.g. jnl_file_lost)
	 * will be confused about whether to do the journal resource free up depending on where in the execution flow
	 * the jnl_qio_end AST got delivered. Therefore do not do any freeup while holding crit and in interrupt code.
	 */
	if (!csa->now_crit && (jnl_closed == csa->hdr->jnl_state))
	{	/* operator close that this process needs to recognize */
		jpc->old_channel = jpc->channel;
		jpc->channel = NOJNL;
		jpc->status = gtm_deq(jpc->jnllsb->lockid, NULL, PSL$C_USER, 0);
		assert(SS$_NORMAL == jpc->status);
		if (SS$_NORMAL == jpc->status)
			jpc->jnllsb->lockid = 0;
		jnl_oper_user_ast(jpc->region);		/* which should deq the lock */
	}
	return;
}

void jnl_mm_timer_write(gd_region *reg)
{
	sgmnt_addrs	*csa;

	assert(lib$ast_in_prog());
	assert(reg->open);	/* gds_rundown() should have cancelled timers for this region before setting reg->open to FALSE */
	if (!reg->open)
		return;
	csa = &FILE_INFO(reg)->s_addrs;
	if (csa->jnl)	/* cover trip during rundown */
		jnl_start_ast(csa->jnl);
	adawi(-1, &csa->nl->wcs_timers);
	csa->timer = FALSE;
	astq_dyn_avail++;
	return;
}

void jnl_mm_timer(sgmnt_addrs *csa, gd_region *reg)
{
	uint4	status;

	assert(reg->open);
	if (!csa->timer && csa->nl->wcs_timers < 1)
	{
		if (astq_dyn_avail > 0)
		{
			astq_dyn_avail--;
			csa->timer = TRUE;
			adawi(1, &csa->nl->wcs_timers);
			status = sys$setimr(efn_ignore, &csa->hdr->flush_time[0], jnl_mm_timer_write, reg, 0);
			if (0 == (status & 1))
			{
				adawi(-1, &csa->nl->wcs_timers);
				csa->timer = FALSE;
				astq_dyn_avail++;
			}
		}
	}
	return;
}
