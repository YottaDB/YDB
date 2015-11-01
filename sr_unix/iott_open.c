/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "gtm_stdlib.h"
#include "gtm_termio.h"

#include "io.h"
#include "iottdef.h"
#include "io_params.h"
#include "trmdef.h"
#include "gtmio.h"
#include "stringpool.h"
#include "setterm.h"
#include "getcaps.h"
#include "gtm_isanlp.h"

GBLREF bool		run_time;
GBLREF int		COLUMNS, GTM_LINES, AUTO_RIGHT_MARGIN;
LITREF unsigned char	io_params_size[];

short iott_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, int4 timeout)
{
	unsigned char	ch;
	d_tt_struct	*tt_ptr;
	io_desc		*ioptr;
	int		status;
	int		save_errno;
	int		p_offset;

	error_def(ERR_NOTERMENV);
	error_def(ERR_NOTERMENTRY);
	error_def(ERR_NOTERMINFODB);
	error_def(ERR_TCGETATTR);

	ioptr = dev_name->iod;
	if (ioptr->state == dev_never_opened)
	{
		dev_name->iod->dev_sp = (void *)malloc(sizeof(d_tt_struct) + sizeof(struct termios));
		memset(dev_name->iod->dev_sp, 0, sizeof(d_tt_struct) + sizeof(struct termios));
		tt_ptr = (d_tt_struct *)dev_name->iod->dev_sp;
		tt_ptr->ttio_struct = (struct termios *)((char *)tt_ptr + sizeof(d_tt_struct));
		tt_ptr->in_buf_sz = TTDEF_BUF_SZ;
		tt_ptr->enbld_outofbands.x = 0;
		tt_ptr->term_ctrl &= (~TRM_NOECHO);
		tt_ptr->mask_term.mask[0] = TERM_MSK;
		tt_ptr->ttybuff = (char *)malloc(IOTT_BUFF_LEN);
	}
	tt_ptr = (d_tt_struct *)dev_name->iod->dev_sp;
	p_offset = 0;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		if ((ch = *(pp->str.addr + p_offset++)) == iop_exception)
		{
			ioptr->error_handler.len = *(pp->str.addr + p_offset);
			ioptr->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&ioptr->error_handler);
			break;
		} else  if (ch == iop_canonical)
			tt_ptr->canonical = TRUE;
		else  if (ch == iop_nocanonical)
			tt_ptr->canonical = FALSE;
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
	if (ioptr->state != dev_open)
	{
		int	status;
		char	*env_term;

		assert(fd >= 0);
		tt_ptr->fildes = fd;
		status = tcgetattr(tt_ptr->fildes, tt_ptr->ttio_struct);
		if (0 != status)
		{
			save_errno = errno;
			if (gtm_isanlp(tt_ptr->fildes) == 0)
				rts_error(VARLSTCNT(3) ERR_TCGETATTR, tt_ptr->fildes, save_errno);
		}
		if (run_time)
			setterm(ioptr);
		status = getcaps(tt_ptr->fildes);
		if (1 != status)
		{
			if (status == 0)
			{
				env_term = GETENV("TERM");
				if (!env_term)
				{
					rts_error(VARLSTCNT(1) ERR_NOTERMENV);
					env_term = "unknown";
				}
				rts_error(VARLSTCNT(4) ERR_NOTERMENTRY, 2, LEN_AND_STR(env_term));
			} else
				rts_error(VARLSTCNT(1) ERR_NOTERMINFODB);
		}
		ioptr->width = (unsigned short)COLUMNS;
		ioptr->length = (unsigned short)GTM_LINES;
		ioptr->wrap = (0 == AUTO_RIGHT_MARGIN) ? FALSE : TRUE; /* defensive programming; till we are absolutely, positively
									* certain that there are no uses of wrap == TRUE */
		ioptr->state = dev_open;
		tt_ptr->tbuffp = tt_ptr->ttybuff;	/* Buffer is now empty */
	}
	return TRUE;
}
