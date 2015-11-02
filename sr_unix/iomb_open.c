/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "copy.h"
#include "io.h"
#include "iombdef.h"
#include "io_params.h"
#include "stringpool.h"

LITREF unsigned char io_params_size[];

short iomb_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, int4 timeout)
{
	unsigned char	ch;
	io_desc		*iod;
 	d_mb_struct	*d_mb;
	int		p_offset;
	error_def(ERR_DEVPARMNEG);

	p_offset = 0;
	iod = dev_name->iod;
	assert((params) *(pp->str.addr) < (unsigned char)n_iops);
	assert(iod != 0);
	assert(iod->state >= 0 && iod->state < n_io_dev_states);
	assert(iod->type == mb);
	if (iod->state == dev_never_opened)
	{
		iod->dev_sp = (void *)malloc(SIZEOF(d_mb_struct));
		d_mb = (d_mb_struct *)iod->dev_sp;
		d_mb->maxmsg = DEF_MB_MAXMSG;
		d_mb->promsk = 0;
		d_mb->del_on_close = FALSE;
		d_mb->prmflg = 0;
		d_mb->write_mask = 0;
	}
	d_mb = (d_mb_struct *)iod->dev_sp;
	if (iod->state != dev_open)
	{
		while ((ch = *(pp->str.addr + p_offset++)) != iop_eol)
		{
			switch(ch)
			{
			case iop_blocksize:
				GET_LONG(d_mb->maxmsg, (pp->str.addr + p_offset));
				if ((int4)d_mb->maxmsg < 0)
					rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
				break;
			case iop_readonly:
				d_mb->promsk = IO_RD_ONLY;
				break;
			case iop_noreadonly:
				d_mb->promsk &= ~IO_RD_ONLY;
				break;
			case iop_writeonly:
				d_mb->promsk = IO_SEQ_WRT;
				break;
			case iop_nowriteonly:
				d_mb->promsk &= ~IO_SEQ_WRT;
				break;
			case iop_delete:
				d_mb->del_on_close = TRUE;
				break;
			case iop_prmmbx:
				d_mb->prmflg = 1;
				break;
			case iop_tmpmbx:
				d_mb->prmflg = 0;
				break;
			case iop_exception:
				iod->error_handler.len = *(pp->str.addr + p_offset);
				iod->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
				s2pool(&iod->error_handler);
				break;
			default:
				break;
			}
			p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
				(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
		}
		d_mb->in_pos = d_mb->in_top = d_mb->inbuf = (unsigned char *)malloc(DEF_MB_MAXMSG);
		memset(d_mb->inbuf, 0, DEF_MB_MAXMSG);
		assert(fd >= 0);
		d_mb->channel = fd;
		iod->state = dev_open;
	}
	return TRUE;
}
