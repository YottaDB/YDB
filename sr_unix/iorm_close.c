/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"
#include <errno.h>

#include "gtm_stat.h"
#include "gtm_stdio.h"

#include "io.h"
#include "iormdef.h"
#include "io_params.h"
#include "eintr_wrappers.h"
#include <sys/wait.h>

LITREF unsigned char	io_params_size[];

void iorm_close(io_desc *iod, mval *pp)
{
	d_rm_struct	*rm_ptr;
	d_rm_struct	*in_rm_ptr;
	d_rm_struct	*stderr_rm_ptr;
	unsigned char	c;
	char		*path, *path2;
	int		fclose_res;
	int		stat_res;
	int		fstat_res;
	struct stat	statbuf, fstatbuf;
	int		p_offset;
	pid_t  		done_pid;
	int  		wait_status;

	assert (iod->type == rm);
	if (iod->state != dev_open)
	    return;

	rm_ptr = (d_rm_struct *)iod->dev_sp;
	iorm_use(iod,pp);
	if (iod->dollar.x && rm_ptr->lastop == RM_WRITE && !iod->dollar.za)
		iorm_flush(iod);

	p_offset = 0;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		switch (c = *(pp->str.addr + p_offset++))
		{
			case iop_delete:
				path = iod->trans_name->dollar_io;
				FSTAT_FILE(rm_ptr->fildes, &fstatbuf, fstat_res);
				if (-1 == fstat_res)
					rts_error(VARLSTCNT(1) errno);
				STAT_FILE(path, &statbuf, stat_res);
				if (-1 == stat_res)
					rts_error(VARLSTCNT(1) errno);
				if (CYGWIN_ONLY(rm_ptr->fifo ||) fstatbuf.st_ino == statbuf.st_ino)
					if (UNLINK(path) == -1)
						rts_error(VARLSTCNT(1) errno);
				break;
			case iop_rename:
				path = iod->trans_name->dollar_io;
				path2 = (char*)(pp->str.addr + p_offset + 1);
				FSTAT_FILE(rm_ptr->fildes, &fstatbuf, fstat_res);
				if (-1 == fstat_res)
					rts_error(VARLSTCNT(1) errno);
				STAT_FILE(path, &statbuf, stat_res);
				if (-1 == stat_res)
					rts_error(VARLSTCNT(1) errno);
				if (CYGWIN_ONLY(rm_ptr->fifo ||) fstatbuf.st_ino == statbuf.st_ino)
				{	if (LINK(path, path2) == -1)
						rts_error(VARLSTCNT(1) errno);
					if (UNLINK(path) == -1)
						rts_error(VARLSTCNT(1) errno);
				}
				break;
			default:
				break;
		}
		p_offset += ( io_params_size[c]==IOP_VAR_SIZE ?
			(unsigned char)(*(pp->str.addr + p_offset) + 1) : io_params_size[c] );
	}

	if (iod->pair.in != iod)
		assert (iod->pair.out == iod);
	if (iod->pair.out != iod)
		assert (iod->pair.in == iod);

	iod->state = dev_closed;
	iod->dollar.zeof = FALSE;
	iod->dollar.x = 0;
	iod->dollar.y = 0;
	rm_ptr->lastop = RM_NOOP;
	if (rm_ptr->inbuf)
	{
		free(rm_ptr->inbuf);
		rm_ptr->inbuf = NULL;
	}
	if (rm_ptr->outbuf)
	{
		free(rm_ptr->outbuf);
		rm_ptr->outbuf = NULL;
	}

	/* Do the close first. If the fclose() is done first and we are being called from io_rundown just prior to the execv
	   in a newly JOBbed off process, the fclose() does an implied fflush() which is known to do an lseek() which resets
	   the file pointers of any open (flat) files in the parent due to an archane interaction between child and parent
	   processes prior to an execv() call. The fclose (for stream files) will fail but it will clean up structures orphaned
	   by the close().
	*/
	/* Close the fildes unless this is a direct close of the stderr device */
	if (!rm_ptr->stderr_parent)
	{
		close(rm_ptr->fildes);
		if (rm_ptr->filstr != NULL)
			FCLOSE(rm_ptr->filstr, fclose_res);
		/* if this is a pipe and read_fildes and read_filstr are set then close them also */
		if (rm_ptr->read_fildes)
			close(rm_ptr->read_fildes);
		if (rm_ptr->read_filstr != NULL)
			FCLOSE(rm_ptr->read_filstr, fclose_res);
	}

	/* reap the forked shell process if a pipe - it will be a zombie, otherwise*/
	if (rm_ptr->pipe_pid > 0)
	{
		/* if child process will not go away then have to kill shell in order to reap */
		if (rm_ptr->independent)
			kill(rm_ptr->pipe_pid,SIGKILL);
		WAITPID(rm_ptr->pipe_pid, &wait_status, 0, done_pid);
		assert(done_pid == rm_ptr->pipe_pid);

		if (rm_ptr->stderr_child)
		{
			/* have to make it look open in case it was closed directly */
			/* reset the stderr_parent field so the fildes will be closed */
			rm_ptr->stderr_child->state = dev_open;
			stderr_rm_ptr = (d_rm_struct *)rm_ptr->stderr_child->dev_sp;
			stderr_rm_ptr->stderr_parent = 0;
			iorm_close(rm_ptr->stderr_child,pp);
			remove_rms(rm_ptr->stderr_child);
		}
	}
#ifdef __MVS__
	if (rm_ptr->fifo)
	{
		if (rm_ptr != (iod->pair.out)->dev_sp || rm_ptr != (iod->pair.in)->dev_sp)
		{
			if (rm_ptr != (iod->pair.out)->dev_sp)
			{
				rm_ptr = (iod->pair.out)->dev_sp;
				iod = iod->pair.out;
			}
			else
			{
				rm_ptr = (iod->pair.in)->dev_sp;
				iod = iod->pair.in;
			}
			assert(NULL != rm_ptr);
			if(dev_closed != iod->state)
			{
				iod->state = dev_closed;
				iod->dollar.zeof = FALSE;
				iod->dollar.x = 0;
				iod->dollar.y = 0;
				rm_ptr->lastop = RM_NOOP;
				assert(rm_ptr->fildes>=0);
				close(rm_ptr->fildes);
				if (rm_ptr->filstr != NULL)
					fclose(rm_ptr->filstr);
			}
		}
	}
#endif
	return;
}
