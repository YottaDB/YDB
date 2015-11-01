/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <unistd.h>

#include "gtmio.h"	/* this has to come in before gdsfhead.h, for all "open" to be defined
				to "open64", including the open in header files */
#include "gtm_unistd.h"	/* fsync() needs this */
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

GBLREF	volatile int4	db_fsync_in_prog;
GBLREF	volatile int4	jnl_qio_in_prog;
GBLREF	uint4		process_id;
uint4 jnl_sub_qio_start(jnl_private_control *jpc, boolean_t aligned_write);
void jnl_mm_timer_write(void);

/* If the second argument is TRUE, then the jnl write is done only upto the previous aligned boundary.
 * else the write is done upto the freeaddr */

uint4 jnl_sub_qio_start(jnl_private_control *jpc, boolean_t aligned_write)
{
	boolean_t		was_wrapped;
	int			tsz;
	jnl_buffer_ptr_t	jb;
	int4			free;
	sgmnt_addrs		*csa;
	sm_uc_ptr_t		base;
	unix_db_info		*udi;
	unsigned int		status;

	error_def(ERR_JNLACCESS);
	error_def(ERR_JNLWRTDEFER);
	error_def(ERR_JNLWRTNOWWRTR);

	assert(NULL != jpc);
	udi = FILE_INFO(jpc->region);
	csa = &udi->s_addrs;
	jb = jpc->jnl_buff;
	if (jb->io_in_prog_latch.latch_pid == process_id)	/* We already have the lock? */
		return ERR_JNLWRTNOWWRTR;			/* timer driven io in progress */
	jnl_qio_in_prog++;
	if (!GET_SWAPLOCK(&jb->io_in_prog_latch))
	{
		jnl_qio_in_prog--;
		assert(0 <= jnl_qio_in_prog);
		return ERR_JNLWRTDEFER;
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
		DB_FSYNC(jpc->region, udi, csa, db_fsync_in_prog);
		jb->need_db_fsync = FALSE;
	}
	free = jb->free;
	was_wrapped = free < jb->dsk;
	if (aligned_write)
		free = ROUND_DOWN(free, IO_BLOCK_SIZE);
	assert(!(jb->size % IO_BLOCK_SIZE));
	tsz = (free < jb->dsk ? jb->size : free) - jb->dsk;
	if (aligned_write && !was_wrapped && free <= jb->dsk)
		tsz = 0;
	assert(0 <= tsz);
	/* Note that this assert relies on the fact that freeaddr is updated before free in jnl_write() [jnl_output.c] */
	assert(jb->dskaddr + tsz <= jb->freeaddr);
	if (tsz)
	{	/* ensure that dsk and free are never equal and we have left space for JNL_WRT_START_MASK */
		assert((free > jb->dsk) || (free < (jb->dsk & JNL_WRT_START_MASK)) || (jb->dsk != (jb->dsk & JNL_WRT_START_MASK)));
		jb->wrtsize = tsz;
		jb->qiocnt++;
		base = &jb->buff[jb->dsk];
		assert((base + tsz) <= (jb->buff + jb->size));
		if (NOJNL == jpc->channel)
		{
			jb->wrtsize = 0;
			jpc->status = SS_NORMAL;
		} else
		{
			LSEEKWRITE(jpc->channel, (off_t)jb->dskaddr, (sm_uc_ptr_t)base, tsz, jpc->status);
			if (SS_NORMAL != jpc->status)
				jpc->channel = NOJNL;
		}
	} else
	{
		jb->wrtsize = 0;
		jpc->status = SS_NORMAL;
	}
	status = jpc->status;
	if (SS_NORMAL != jpc->status)
	{
		jb->errcnt++;
		jnl_send_oper(jpc, ERR_JNLACCESS);
		assert(FALSE);
	}
	jpc->status = SS_NORMAL;
	assert(jb->dsk <= jb->size);
	assert(jb->io_in_prog_latch.latch_pid == process_id);
	jpc->new_dsk = jb->dsk + jb->wrtsize;
	if (jpc->new_dsk >= jb->size)
	{
		assert(jpc->new_dsk == jb->size);
		jpc->new_dsk = 0;
	}
	jpc->new_dskaddr = jb->dskaddr + jb->wrtsize;
	assert(jpc->new_dsk == jpc->new_dskaddr % jb->size);
	assert(jb->freeaddr >= jpc->new_dskaddr);

	jpc->dsk_update_inprog = TRUE;	/* for secshr_db_clnup to clean it up (when it becomes feasible in Unix) */
	jb->dsk = jpc->new_dsk;
	jb->dskaddr = jpc->new_dskaddr;
	jpc->dsk_update_inprog = FALSE;

	RELEASE_SWAPLOCK(&jb->io_in_prog_latch);
	if ((jnl_closed == csa->hdr->jnl_state) && NOJNL != csa->jnl->channel)
	{
		close(csa->jnl->channel);
		csa->jnl->channel = NOJNL;
		csa->jnl->lastwrite = 0;
		csa->jnl->regnum = 0;
		csa->jnl->pini_addr = 0;
	}
	jnl_qio_in_prog--;
	assert(0 <= jnl_qio_in_prog);
	return status;
}

/* This is a wrapper for jnl_sub_qio_start that tries to divide the writes into optimal chunks.
 * It calls jnl_sub_qio_start() with appropriate arguments in two stages, the first one with
 * optimal IO_BLOCK_SIZE boundary and the other suboptimal tail end of the write. The latter
 * call is made only if no other process has finished the jnl write upto the required point
 * during the time this process yields */

uint4 jnl_qio_start(jnl_private_control *jpc)
{
	unsigned int		yield_cnt, status;
	uint4			target_freeaddr, lcl_dskaddr, old_freeaddr;
	jnl_buffer_ptr_t	jb;
	sgmnt_addrs		*csa;
	unix_db_info		*udi;

	assert(NULL != jpc);
	udi = FILE_INFO(jpc->region);
	csa = &udi->s_addrs;
	jb = jpc->jnl_buff;

	/* this block of code (till yield()) processes the buffer upto an IO_BLOCK_SIZE alignment boundary
	 * and the next block of code (after the yield()) processes the tail end of the data (if necessary) */

	lcl_dskaddr = jb->dskaddr;
	target_freeaddr = jb->freeaddr;
	if (lcl_dskaddr >= target_freeaddr)
		return SS_NORMAL;

	/* ROUND_DOWN2 macro is used under the assumption that IO_BLOCK_SIZE would be a power of 2 */
	if (ROUND_DOWN2(lcl_dskaddr, IO_BLOCK_SIZE) != ROUND_DOWN2(target_freeaddr, IO_BLOCK_SIZE))
	{	/* data crosses/touches an alignment boundary */
		if (SS_NORMAL != (status = jnl_sub_qio_start(jpc, TRUE)))
			return status;
	} /* else, data does not cross/touch an alignment boundary, yield and see if someone else
	   * does the dirty job more efficiently */

	for (yield_cnt = 0; yield_cnt < csa->hdr->yield_lmt; yield_cnt++)
	{	/* yield() until someone has finished your job or no one else is active on the jnl file */
		old_freeaddr = jb->freeaddr;
		rel_quant();
		if (JNL_FILE_SWITCHED(jpc))
			return SS_NORMAL;
		assert(old_freeaddr <= jb->freeaddr);
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
