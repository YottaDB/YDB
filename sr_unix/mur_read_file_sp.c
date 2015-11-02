/****************************************************************
 *								*
 *	Copyright 2003, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "min_max.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_string.h"
#include "eintr_wrappers.h"
#include "gtm_aio.h"
#include "gtmio.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "mur_read_file.h"
#include "iosp.h"
#include "copy.h"
#include "gtmmsg.h"

GBLREF	mur_read_desc_t	mur_desc;
GBLREF	jnl_ctl_list	*mur_jctl;
GBLREF	mur_opt_struct	mur_options;
GBLREF 	mur_gbls_t	murgbl;

#ifdef MUR_USE_AIO

/****************************************************************************************
 * Function Name: mur_fread_start
 * Input: struct mur_buffer_desc * buff
 * Output : SS_NORMAL on successful
 *          error status on unsuccessful
 * This function starts an asynchronous read in a given buffer
 ****************************************************************************************/
uint4 mur_fread_start(mur_buff_desc_t *buff)
{
	buff->aiocbp->aio_offset = buff->dskaddr;
	buff->aiocbp->aio_buf = (char *)buff->base;
	buff->blen = MIN(MUR_BUFF_SIZE, mur_jctl->eof_addr - buff->dskaddr);
	buff->aiocbp->aio_nbytes = buff->blen;
	buff->rip_channel = mur_jctl->channel;	/* store channel that issued the AIO in order to use later for aio_cancel() */
	buff->read_in_progress = TRUE;
        AIO_READ(buff->rip_channel, buff->aiocbp, mur_jctl->status, mur_jctl->status2);
	return mur_jctl->status;
}
/************************************************************************************
 * Function name: mur_fread_wait
 * Input : struct mur_buffer_desc *buff
 * Output: SS_NORMAL on success
 *         error status on unsuccessful
 * Purpose: The purpose of this routine is to make sure that a previously issued asysnchronous read
 *          in a given buffer has completed
 **************************************************************************************/
uint4 mur_fread_wait(mur_buff_desc_t *buff)
{
	ssize_t	nbytes;

	error_def(ERR_PREMATEOF);

	assert(buff->read_in_progress);
	buff->read_in_progress = FALSE;
	/* The aio_error function returns the error status associated with the specified aiocbp. If the aio_error function returns
	 * anything but EINPROGRESS, the asynchronous I/O operation has completed. Subsequently, we can fetch the status of the
	 * operation from a call to aio_return.
	 */
	AIO_ERROR(buff->aiocbp, mur_jctl->status);
	if (-1 != mur_jctl->status)
	{
		AIO_RETURN(buff->aiocbp, nbytes); /* if successful mur_jctl->status will contain number of bytes read */
		if (-1 != nbytes)
		{
			if (buff->blen == nbytes)
				return (mur_jctl->status = SS_NORMAL);
			/* AIO_READ didn't fetch the requested size chunk */
			assert(buff->blen > nbytes);
			DO_FILE_READ(buff->rip_channel, buff->dskaddr + nbytes, buff->base + nbytes, buff->blen - nbytes,
					mur_jctl->status, mur_jctl->status2);
			return mur_jctl->status;
		}
	}
	return (mur_jctl->status = errno);
}

/* cancel asynchronous read */
uint4 mur_fread_cancel(void)
{
	int	status, index, save_err;

	/* At most one buffer can have read_in_progress, not both */
	assert(!(mur_desc.seq_buff[0].read_in_progress && mur_desc.seq_buff[1].read_in_progress));
	for (index = 0, save_err = 0; index < sizeof(mur_desc.seq_buff) / sizeof(mur_desc.seq_buff[0]); index++)
	{
		if (mur_desc.seq_buff[index].read_in_progress)
		{
			AIO_CANCEL(mur_desc.seq_buff[index].rip_channel, NULL, status);
			if (-1 == status)
				save_err = errno;
			else if (AIO_NOTCANCELED == status)	/* the OS cannot cancel the aio once it has actually started */
				mur_jctl->status = mur_fread_wait(&mur_desc.seq_buff[index]);	/* wait for it to finish. */
			mur_desc.seq_buff[index].read_in_progress = FALSE;
		}
	}
	/* Note that although the cancellation errored out for rip_channel, we are storing the status in mur_jctl which need not
	 * actually be the jctl corresponding to rip_channel
	 */
	return (mur_jctl->status = ((0 == save_err) ? SS_NORMAL : save_err));
}

#endif /* MUR_USE_AIO */

boolean_t mur_fopen_sp(jnl_ctl_list *jctl)
{
	struct stat		stat_buf;
	int			status, perms;

	error_def(ERR_JNLFILEOPNERR);
	error_def(ERR_SYSCALL);

	perms = O_RDONLY;
	jctl->read_only = TRUE;
	/* Both for recover and rollback open in read/write mode. We do not need to write in journal file
	 * for mupip journal extract/show/verify or recover -forward.  So open it as read-only
	 */
	if (mur_options.update && !mur_options.forward)
	{
		perms = O_RDWR;
		jctl->read_only = FALSE;
	}
	jctl->channel = OPEN((char *)jctl->jnl_fn, perms);
	if (-1 != jctl->channel)
	{
		FSTAT_FILE(jctl->channel, &stat_buf, status);
		if (-1 != status)
		{
			jctl->os_filesize = (off_jnl_t)stat_buf.st_size;
			return TRUE;
		}
		jctl->status = errno;
		CLOSEFILE(jctl->channel, status);
	} else
		jctl->status = errno;
	if (ENOENT == jctl->status)	/* File Not Found is a common error, so no need for SYSCALL */
		gtm_putmsg(VARLSTCNT(5) ERR_JNLFILEOPNERR, 2, jctl->jnl_fn_len, jctl->jnl_fn, jctl->status);
	else
		gtm_putmsg(VARLSTCNT(12) ERR_JNLFILEOPNERR, 2, jctl->jnl_fn_len, jctl->jnl_fn, ERR_SYSCALL, 5,
			LEN_AND_STR((-1 == jctl->channel) ? "open" : "fstat"), CALLFROM, jctl->status);
	jctl->channel = NOJNL;
	return FALSE;
}
