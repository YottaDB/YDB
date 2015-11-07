/****************************************************************
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
#include <sys/wait.h>

#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"

#include "io.h"
#include "iormdef.h"
#include "io_params.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "iosp.h"
#include "string.h"
#include "stringpool.h"

GBLREF io_pair		io_curr_device;
GBLREF io_pair		io_std_device;
GBLREF	boolean_t	gtm_pipe_child;

error_def(ERR_CLOSEFAIL);

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
	int  		wait_status, rc;
	unsigned int	*dollarx_ptr;
	unsigned int	*dollary_ptr;
	char 		*savepath2 = 0;
	int		path2len;
	boolean_t	rm_destroy = TRUE;
	boolean_t	rm_rundown = FALSE;

	assert (iod->type == rm);
	if (iod->state != dev_open)
	{
		remove_rms(iod);
		return;
	}

	rm_ptr = (d_rm_struct *)iod->dev_sp;

#ifdef __MVS__
	/* on zos if it is a fifo device then point to the pair.out for $X and $Y */
	if (rm_ptr->fifo)
	{
		dollarx_ptr = &(iod->pair.out->dollar.x);
		dollary_ptr = &(iod->pair.out->dollar.y);
	} else
#endif
	{
		dollarx_ptr = &(iod->dollar.x);
		dollary_ptr = &(iod->dollar.y);
	}

	iorm_use(iod,pp);
	if (*dollarx_ptr && rm_ptr->lastop == RM_WRITE && !iod->dollar.za)
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
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
				STAT_FILE(path, &statbuf, stat_res);
				if (-1 == stat_res)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
				if (CYGWIN_ONLY(rm_ptr->fifo ||) fstatbuf.st_ino == statbuf.st_ino)
					if (UNLINK(path) == -1)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
				break;
			case iop_rename:
				path = iod->trans_name->dollar_io;
				path2 = (char*)(pp->str.addr + p_offset + 1);
				FSTAT_FILE(rm_ptr->fildes, &fstatbuf, fstat_res);
				if (-1 == fstat_res)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
				STAT_FILE(path, &statbuf, stat_res);
				if (-1 == stat_res)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
				if (CYGWIN_ONLY(rm_ptr->fifo ||) fstatbuf.st_ino == statbuf.st_ino)
				{
					/* make a copy of path2 so we can null terminate it */
					path2len = (int)*((unsigned char *)(pp->str.addr + p_offset));
					assert(stringpool.free >= stringpool.base);
					ENSURE_STP_FREE_SPACE(path2len + 1);
					savepath2 = (char *)stringpool.free;
					memcpy(savepath2, path2, path2len);
					savepath2[path2len] = '\0';
					stringpool.free += path2len + 1;
					assert(stringpool.free >= stringpool.base);
					assert(stringpool.free <= stringpool.top);
					if (LINK(path, savepath2) == -1)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
					if (UNLINK(path) == -1)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
				}
				break;
			case iop_destroy:
				rm_destroy = TRUE;
				break;
			case iop_nodestroy:
				rm_destroy = FALSE;
				break;
			case iop_rundown:
				rm_rundown = TRUE;
				break;
			default:
				break;
		}
		p_offset += ( io_params_size[c]==IOP_VAR_SIZE ?
			(unsigned char)(*(pp->str.addr + p_offset) + 1) : io_params_size[c] );
	}

	if (iod->pair.in != iod)
		assert(iod->pair.out == iod);
	if (iod->pair.out != iod)
		assert(iod->pair.in == iod);

	iod->state = dev_closed;
	iod->dollar.zeof = FALSE;
	*dollarx_ptr = 0;
	*dollary_ptr = 0;
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

	/* Do the close first. If the fclose is done first and we are being called from io_rundown just prior to the execv
	 * in a newly JOBbed off process, the fclose does an implied FFLUSH which is known to do an lseek which resets
	 * the file pointers of any open (flat) files in the parent due to an archane interaction between child and parent
	 * processes prior to an execv call. The fclose (for stream files) will fail but it will clean up structures orphaned
	 * by the CLOSEFILE_RESET.
	 */
	/* Close the fildes unless this is a direct close of the stderr device */
	if (!rm_ptr->stderr_parent)
	{
		int save_fd;
		/* Before closing a pipe device file descriptor, check if the fd was already closed as part of a "write /eof".
		 * If so, do not attempt the close now. Only need to free up the device structures.
		 */
		if (FD_INVALID != rm_ptr->fildes)
		{
			save_fd = rm_ptr->fildes;
			CLOSEFILE_RESET(rm_ptr->fildes, rc);	/* resets "rm_ptr->fildes" to FD_INVALID */
			if (0 != rc)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CLOSEFAIL, 1, save_fd, rc);
		}
		if (rm_ptr->filstr != NULL)
		{
			FCLOSE(rm_ptr->filstr, fclose_res);
			rm_ptr->filstr = NULL;
		}
		/* if this is a pipe and read_fildes and read_filstr are set then close them also */
		if (0 < rm_ptr->read_fildes)
		{
			save_fd = rm_ptr->read_fildes;
			CLOSEFILE_RESET(rm_ptr->read_fildes, rc);	/* resets "rm_ptr->read_fildes" to FD_INVALID */
			if (0 != rc)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CLOSEFAIL, 1, save_fd, rc);
		}
		if (rm_ptr->read_filstr != NULL)
		{
			FCLOSE(rm_ptr->read_filstr, fclose_res);
			rm_ptr->read_filstr = NULL;
		}
	}

	/* reap the forked shell process if a pipe - it will be a zombie, otherwise*/
	if (rm_ptr->pipe_pid > 0)
	{
		/* Don't reap if in a child process creating a new pipe or if in parent and independent is set */
		if (FALSE == gtm_pipe_child)
		{
			if (!rm_ptr->independent)
			{
				WAITPID(rm_ptr->pipe_pid, &wait_status, 0, done_pid);
				assert(done_pid == rm_ptr->pipe_pid);
			}
		}

		if (rm_ptr->stderr_child)
		{
			/* If the stderr device is the current device then set it to the principal device.  This
			 is handled in op_close for a direct close of the stderr device.  */
			if (io_curr_device.in == rm_ptr->stderr_child)
			{
				io_curr_device.in = io_std_device.in;
				io_curr_device.out = io_std_device.out;
			}
			/* have to make it look open in case it was closed directly */
			/* reset the stderr_parent field so the fildes will be closed */
			rm_ptr->stderr_child->state = dev_open;
			stderr_rm_ptr = (d_rm_struct *)rm_ptr->stderr_child->dev_sp;
			stderr_rm_ptr->stderr_parent = 0;
			iorm_close(rm_ptr->stderr_child,pp);
		}
	}
	if ((rm_destroy || rm_ptr->pipe) && !rm_rundown)
	        remove_rms (iod);
	return;
}
