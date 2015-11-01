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

#include <unistd.h>

#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "io.h"
#include "iormdef.h"
#include "io_params.h"
#include "eintr_wrappers.h"

GBLREF io_pair		io_curr_device;

LITREF unsigned char	io_params_size[];


/* WARNING, this routine is called from ioff_open as well as from the dispatch table. */

short	iorm_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, int4 timeout)
{
	io_desc		*iod;
 	d_rm_struct	*d_rm;
	off_t		size;
	unsigned char	ch;
	int		fstat_res;
	struct stat	statbuf;
	int 		p_offset;

	error_def(ERR_DEVOPENFAIL);
	error_def(ERR_TEXT);

	iod = dev_name->iod;
	size = 0;
	p_offset = 0;
	assert((params) *(pp->str.addr + p_offset) < (unsigned char)n_iops);
	assert(NULL != iod);
	assert(0 <= iod->state && n_io_dev_states > iod->state);
	assert(rm == iod->type);
	if (dev_never_opened == iod->state)
	{
		iod->dev_sp = (void *)malloc(sizeof(d_rm_struct));
		d_rm = (d_rm_struct *)iod->dev_sp;
		iod->state = dev_closed;
		d_rm->stream = FALSE;
		iod->width = DEF_RM_WIDTH;
		iod->length = DEF_RM_LENGTH;
		d_rm->fixed = FALSE;
		d_rm->noread = FALSE;
		d_rm->fifo = FALSE;
	}
	d_rm = (d_rm_struct *)iod->dev_sp;
	if (dev_closed == iod->state)
	{
		d_rm->lastop = RM_NOOP;
		assert(0 <= fd);
		d_rm->fildes = fd;
		for (p_offset = 0; iop_eol != *(pp->str.addr + p_offset); )
		{
			if (iop_append == (ch = *(pp->str.addr + p_offset++)))
			{
				if (!d_rm->fifo && (off_t)-1 == (size = lseek(fd, (off_t)0, SEEK_END)))
					rts_error(VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len, dev_name->dollar_io,
						  ERR_TEXT, 2, LEN_AND_LIT("Error setting file pointer to end of file"), errno);
				break;
			}
			p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ? (unsigned char)*(pp->str.addr + p_offset) + 1 :
					io_params_size[ch]);
		}
		FSTAT_FILE(fd, &statbuf, fstat_res);
		if (-1 == fstat_res)
			rts_error(VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len, dev_name->dollar_io, ERR_TEXT, 2,
					LEN_AND_LIT("Error in fstat"), errno);
		if (!d_rm->fifo)
		{
			if ((off_t)-1 == (size = lseek(fd, (off_t)0, SEEK_CUR)))
				rts_error(VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len, dev_name->dollar_io,
				  ERR_TEXT, 2, LEN_AND_LIT("Error setting file pointer to the current position"), errno);
			if (size == statbuf.st_size)
				iod->dollar.zeof = TRUE;
		}
		if (1 == fd)
			d_rm->filstr = NULL;
		else if (NULL == (d_rm->filstr = FDOPEN(fd, "r")) && NULL == (d_rm->filstr = FDOPEN(fd, "w")))
			rts_error(VARLSTCNT(9) ERR_DEVOPENFAIL, 2, dev_name->len, dev_name->dollar_io,
				  ERR_TEXT, 2, LEN_AND_LIT("Error in stream open"), errno);
	}
	iorm_use(iod, pp);
	iod->state = dev_open;
	return TRUE;
}
