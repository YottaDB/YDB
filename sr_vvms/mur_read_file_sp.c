/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <fab.h>
#include <rmsdef.h>
#include <xab.h>
#include <iodef.h>
#include <ssdef.h>
#include <efndef.h>

#include "min_max.h"
#include "gtmio.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "iosb_disk.h"
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "mur_read_file.h"
#include "util.h"
#include "gtmmsg.h"
#include "iosp.h"	/* for SS_NORMAL */


error_def(ERR_PREMATEOF);

GBLREF	mur_opt_struct	mur_options;

static	void	mur_fread_fini(void)
{
	sys$wake(NULL, NULL);
	return;
}
/****************************************************************************************
 * Function Name: mur_fread_start
 * Input: struct mur_buffer_desc * buff
 * Output : SS_NORMAL on successful
 *          error status on unsuccessful
 *This function starts an asynchrounous read in a given buffer
 ****************************************************************************************/

uint4	mur_fread_start(jnl_ctl_list *jctl, mur_buff_desc_t *buff)
{
	assert(0 == buff->dskaddr % DISK_BLOCK_SIZE);
	buff->blen = MIN(MUR_BUFF_SIZE, jctl->eof_addr - buff->dskaddr);
	buff->rip_channel = jctl->channel;	/* store channel that issued the AIO in order to use later for sys$_cancel() */
	assert(!buff->read_in_progress);
	buff->read_in_progress = TRUE;
	jctl->status = sys$qio(EFN$C_ENF, jctl->channel, IO$_READVBLK, &buff->iosb, mur_fread_fini, buff, buff->base,
					buff->blen, (buff->dskaddr >> LOG2_DISK_BLOCK_SIZE) + 1, 0, 0, 0);
	return jctl->status;
}

/************************************************************************************
 * Function name: mur_fread_wait
 * Input : struct mur_buffer_desc *buff
 * Output: SS_NORMAL on success
 *         error status on unsuccessful
 * Purpose: The purpose of this routine is to make sure that a previously issued asynchrounous
 *          read in a given  buffer has completed
 **************************************************************************************/
uint4	mur_fread_wait(jnl_ctl_list *jctl, mur_buff_desc_t *buff)
{
	uint4	status;

	assert(buff->read_in_progress);
	buff->read_in_progress = FALSE;
	/* sys$qio clears iosb when it begins execution */
	while ((status = buff->iosb.cond) == 0)
		sys$hiber();
	return ((SS$_NORMAL == status || SS$_CANCEL == status || SS$_ABORT == status) ? SS_NORMAL : status);
}
/************************************************************************************************
 * Function Name: mur_fread_cancel
 * Input: buffer index
 * Output: SS_NORMAL on successful
 *         error status on unsuccessful
 * This function is used for cancelling asynchronous I/O issued for seq buffers
 * ************************************************************************************************/
uint4 mur_fread_cancel(jnl_ctl_list *jctl)
{
	uint4			status, save_err;
	int			index;
	reg_ctl_list		*rctl;
	mur_read_desc_t		*mur_desc;
	mur_buff_desc_t		*seq_buff;

	rctl = jctl->reg_ctl;
	mur_desc = rctl->mur_desc;
	/* At most one buffer can have read_in_progress, not both */
	assert(!(mur_desc->seq_buff[0].read_in_progress && mur_desc->seq_buff[1].read_in_progress));
	for (index = 0, save_err = SS_NORMAL; index < ARRAYSIZE(mur_desc->seq_buff); index++)
	{
		seq_buff = &mur_desc->seq_buff[index];
		if (seq_buff->read_in_progress)
		{
			status = sys$cancel(seq_buff->rip_channel);
			if (1 & status)
			{
				if (SS_NORMAL != (status = mur_fread_wait(jctl, seq_buff)))
					save_err = status;
			} else
				save_err = status;
			seq_buff->read_in_progress = FALSE;
		}
	}
	/* Note that although the cancellation errored out for rip_channel, we are storing the status in jctl which need not
	 * actually be the jctl corresponding to rip_channel
	 */
	return (jctl->status = ((SS_NORMAL == save_err) ? SS_NORMAL : save_err));
}
/**************************************************************************************
 * Function name: mur_fopen
 * Input: jnl_ctl_list *
 * Return value : TRUE or False
 * *************************************************************************************/
boolean_t mur_fopen_sp(jnl_ctl_list *jctl)
{
	struct FAB	*fab;
	struct XABFHC	*xab;
	uint4		status;

	error_def(ERR_JNLFILEOPNERR);

	fab = jctl->fab = malloc(SIZEOF(*fab));
	*fab = cc$rms_fab;
	fab->fab$l_fna = jctl->jnl_fn;
	fab->fab$b_fns = jctl->jnl_fn_len;
	fab->fab$b_fac = FAB$M_BIO | FAB$M_GET;
	fab->fab$l_fop = FAB$M_UFO;
	fab->fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
	xab = fab->fab$l_xab = malloc(SIZEOF(*xab));
	*xab = cc$rms_xabfhc;
	jctl->read_only = TRUE;
	/* Both for recover and rollback open in read/write mode. We do not need to write in journal file
	 * for mupip journal extract/show/verify or recover -forward.  So open it as read-only
	 */
	if (mur_options.update && !mur_options.forward)
	{
		fab->fab$b_fac |= FAB$M_PUT;
		jctl->read_only = FALSE;
	}
	status = sys$open(fab);
	if (SYSCALL_SUCCESS(status))
	{
		jctl->channel = fab->fab$l_stv; /* if $open() is succedded, fab$l_stv contains the I/O channel */
		jctl->os_filesize = (xab->xab$l_ebk - 1) * DISK_BLOCK_SIZE;
		return TRUE;
	}
	jctl->status = status;
	jctl->status2 = fab->fab$l_stv;
	jctl->channel = NOJNL;
	gtm_putmsg(VARLSTCNT(6) ERR_JNLFILEOPNERR, 2, jctl->jnl_fn_len, jctl->jnl_fn, jctl->status, jctl->status2);
	return FALSE;
}
