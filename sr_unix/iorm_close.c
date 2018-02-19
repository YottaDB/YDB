/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_string.h"

#include "io.h"
#include "iormdef.h"
#include "io_params.h"
#include "gtm_signal.h"
#include "gtmio.h"
#include "iosp.h"
#include "stringpool.h"
#include "iotimer.h"
#include "gt_timer.h"
#include "wake_alarm.h"
#include "copy.h"
#include "error.h"

GBLREF io_pair		io_curr_device;
GBLREF io_pair		io_std_device;
GBLREF boolean_t	gtm_pipe_child;
GBLREF volatile bool	out_of_time;
GBLREF int		process_exiting;

error_def(ERR_SYSCALL);
error_def(ERR_DEVPARMTOOSMALL);
error_def(ERR_IOERROR);

LITREF unsigned char	io_params_size[];

void iorm_close(io_desc *iod, mval *pp)
{
	d_rm_struct	*rm_ptr;
	d_rm_struct	*in_rm_ptr;
	d_rm_struct	*stderr_rm_ptr;
	unsigned char	c;
	char		*path, *path2;
	int		stat_res;
	int		save_errno;
	int		fstat_res;
	struct stat	statbuf, fstatbuf;
	int		p_offset;
	pid_t  		done_pid;
#ifdef _BSD
        union wait      wait_status;
#else
        int4            wait_status;
#endif

	int  		rc, status;
	unsigned int	*dollarx_ptr;
	unsigned int	*dollary_ptr;
	char 		*savepath2 = 0;
	int		path2len;
	boolean_t	rm_destroy = TRUE;
	boolean_t	rm_rundown = FALSE;
	TID		timer_id;
	int4		pipe_timeout = 2;	/* default timeout in sec waiting for waitpid */
	off_t		cur_position;
	boolean_t	ch_set;
	sigset_t	empty_set, old_set;
	boolean_t	use_timer;

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	assert (iod->type == rm);
	if (iod->state != dev_open)
	{
		remove_rms(iod);
		return;
	}

	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
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
	/* We do not want a NEWLINE to be issued by the middle process. */
	if (!gtm_pipe_child)
		iorm_cond_wteol(iod);

	p_offset = 0;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		switch (c = *(pp->str.addr + p_offset++))
		{
			case iop_delete:
				path = iod->trans_name->dollar_io;
				FSTAT_FILE(rm_ptr->fildes, &fstatbuf, fstat_res);
				if (-1 == fstat_res)
				{
					save_errno = errno;
					SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) save_errno);
				}
				STAT_FILE(path, &statbuf, stat_res);
				if (-1 == stat_res)
				{
					save_errno = errno;
					SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) save_errno);
				}
				if (CYGWIN_ONLY(rm_ptr->fifo ||) fstatbuf.st_ino == statbuf.st_ino)
					if (UNLINK(path) == -1)
					{
						save_errno = errno;
						SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) save_errno);
					}
				break;
			case iop_rename:
				path = iod->trans_name->dollar_io;
				path2 = (char*)(pp->str.addr + p_offset + 1);
				FSTAT_FILE(rm_ptr->fildes, &fstatbuf, fstat_res);
				if (-1 == fstat_res)
				{
					save_errno = errno;
					SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) save_errno);
				}
				STAT_FILE(path, &statbuf, stat_res);
				if (-1 == stat_res)
				{
					save_errno = errno;
					SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) save_errno);
				}
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
					{
						save_errno = errno;
						SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) save_errno);
					}
					if (UNLINK(path) == -1)
					{
						save_errno = errno;
						SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) save_errno);
					}
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
			case iop_timeout:
				GET_LONG(pipe_timeout, (pp->str.addr + p_offset));
				if (1 > pipe_timeout)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVPARMTOOSMALL);
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
	/* save no_destroy for disk device */
	if ((FALSE == rm_destroy) && !rm_ptr->fifo && !rm_ptr->is_pipe && (2 < rm_ptr->fildes))
	{
		rm_ptr->no_destroy = TRUE;
		/* We can write anywhere in the file so need to save current file pointer for re-open
		 if last operation was a write*/
		/* if last operation was a write then set file_pos to position after write */
		if (RM_WRITE == rm_ptr->lastop)
		{
			/* need to do an lseek to get current location in file */
			cur_position = lseek(rm_ptr->fildes, (off_t)0, SEEK_CUR);
			if ((off_t)-1 == cur_position)
			{
				save_errno = errno;
				SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_IOERROR, 7,
					      RTS_ERROR_LITERAL("lseek"),
					      RTS_ERROR_LITERAL("iorm_close()"), CALLFROM, save_errno);
			} else
				rm_ptr->file_pos = cur_position;
		}

	} else
	{
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
		if (rm_ptr->tmp_buffer)
		{
			free(rm_ptr->tmp_buffer);
			rm_ptr->tmp_buffer = NULL;
		}
		if (iod->dollar.devicebuffer)
		{
			free(iod->dollar.devicebuffer);
			iod->dollar.devicebuffer = NULL;
		}
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
		/* Before closing a pipe device file descriptor, check if the fd was already closed as part of a "write /eof".
		 * If so, do not attempt the close now. Only need to free up the device structures.
		 */
		IORM_FCLOSE(rm_ptr, fildes, filstr);
		assert(FD_INVALID == rm_ptr->fildes);
		assert(NULL == rm_ptr->filstr);
		/* if this is a pipe and read_fildes and read_filstr are set then close them also */
		IORM_FCLOSE(rm_ptr, read_fildes, read_filstr);
		assert(FD_INVALID == rm_ptr->read_fildes);
		assert(NULL == rm_ptr->read_filstr);
	}
	/* Reap the forked shell process if a pipe - it will be a zombie otherwise. */
	if (rm_ptr->pipe_pid > 0)
	{	/* Do not reap if in a child process creating a new pipe or if in parent and independent is set. */
		if (FALSE == gtm_pipe_child)
		{
			if (!rm_ptr->independent)
			{
				if (!process_exiting)
				{	/* Find out whether timers are available. If not, instead of scheduling one to interrupt
					 * waitpid, simply invoke it with WNOHANG flag instead. Note that SIGPROCMASK below is
					 * operating on an empty signal set and so is not blocking any signals.
					 */
					sigemptyset(&empty_set);
					SIGPROCMASK(SIG_BLOCK, &empty_set, &old_set, rc);
					use_timer = !sigismember(&old_set, SIGALRM);
				} else
					use_timer = FALSE;
				out_of_time = FALSE;
				if (use_timer)
				{	/* Start timer for pipe_timeout seconds to wait for reap of close via waitpid. If this times
					 * out, set dollar_zclose = -99.
					 */
					timer_id = (TID)iorm_close;
					start_timer(timer_id, pipe_timeout * 1000, wake_alarm, 0, NULL);
				}
				do
				{
					status = 0;
					done_pid = waitpid(rm_ptr->pipe_pid, &status, use_timer ? 0 : WNOHANG); /* BYPASSOK */
				} while (((pid_t)-1 == done_pid) && (EINTR == errno) && (!out_of_time));
				if (((pid_t)-1 == done_pid) && (!out_of_time))
				{
					save_errno = errno;
					if (use_timer)
						cancel_timer(timer_id);
					SET_DOLLARDEVICE_ONECOMMA_STRERROR(iod, save_errno);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
						      RTS_ERROR_LITERAL("waitpid"), CALLFROM, save_errno);
				}
				if (out_of_time || (!use_timer && (0 == done_pid)))
				{
					TREF(dollar_zclose) = -99;
				} else
				{	/* Not an error and did not timeout. */
					assert(!use_timer || (done_pid == rm_ptr->pipe_pid));
					/* Cancel timer since it did not timeout. */
					if (use_timer)
						cancel_timer(timer_id);
#					ifdef _BSD
					assert(SIZEOF(wait_status) == SIZEOF(int4));
					/* The WIF* macros expect a union wait_stat as an argument on BSD. */
					wait_status.w_status = status;
#					else
					wait_status = status;
#					endif
					if (WIFEXITED(wait_status))
					{	/* Normal exit. */
						TREF(dollar_zclose) = WEXITSTATUS(wait_status);
					} else if (WIFSIGNALED(wait_status))
					{	/* Change signal to negative for distinguishing from normal exit codes. */
						TREF(dollar_zclose) = -WTERMSIG(wait_status);
					} else
					{	/* Set any other non-normal status to -98. */
						TREF(dollar_zclose) = -98;
					}
				}
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
	if ((rm_destroy || rm_ptr->is_pipe || rm_ptr->fifo) && !rm_rundown)
	        remove_rms (iod);
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}
