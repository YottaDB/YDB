/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "io.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "io_params.h"
#include "stringpool.h"
#include "copy.h"

LITREF unsigned char io_params_size[];

void iomt_close(io_desc *dv, mval *pp)
{
	unsigned char   ch;
	d_mt_struct    *mt_ptr;
	int		p_offset;
	int4		skips;

	error_def(ERR_UNIMPLOP);

	p_offset = 0;
	mt_ptr = (d_mt_struct *)dv->dev_sp;
#ifdef DP
	FPRINTF(stderr, ">> iomt_close\n");
#endif
	if (dv->state == dev_open)
	{
		iomt_flush(dv);
		while (*(pp->str.addr + p_offset) != iop_eol)
		{
			switch (ch = *(pp->str.addr + p_offset++))
			{
				case iop_exception:
					dv->error_handler.len = *(pp->str.addr + p_offset);
					dv->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
					s2pool(&dv->error_handler);
					break;
				case iop_skipfile:
					GET_LONG(skips, (pp->str.addr + p_offset));
					iomt_skipfile(dv, skips);
					break;
				case iop_unload:
#ifdef UNIX
					rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#else
					assert(FALSE);
#endif
					break;
				case iop_rewind:
					iomt_rewind(dv);
					break;
				case iop_erasetape:
					iomt_erase(dv);
					break;
				case iop_space:
					GET_LONG(skips, (pp->str.addr + p_offset));
					iomt_skiprecord(dv, skips);
					break;
				case iop_writeof:
					iomt_eof(dv);
					break;
				default:
					break;
			}
			p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
				     (unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
		}
		if (mt_ptr->labeled == MTLAB_ANSI)
		{
			if (mt_ptr->last_op == mt_write)
			{
				iomt_tm(dv);
				iomt_wtansilab(dv, MTL_EOF1 | MTL_EOF2);
				iomt_tm(dv);
				iomt_tm(dv);
			}
		} else
		{
			if (mt_ptr->last_op == mt_write)
				iomt_eof(dv);

#ifdef UNIX
			if (mt_ptr->cap.req_extra_filemark
			    && mt_ptr->last_op == mt_eof)
#else
				if (mt_ptr->last_op == mt_eof)
#endif
				{
					iomt_eof(dv);
					iomt_skipfile(dv, -1);
				}
		}
		if (mt_ptr->buffer)
		{
			/*
			 * If bufftoggle is zero, then there is one buffer.
			 * Otherwise, there are two buffers.  If bufftoggle is
			 * less than zero, then the buffer pointer points at
			 * the second buffer, and we must adjust the pointer so
			 * that we get to the beginning of the data which has
			 * been malloc'ed.
			 */
			if (mt_ptr->bufftoggle < 0)
				free(mt_ptr->buffer + mt_ptr->bufftoggle);
			else
				free(mt_ptr->buffer);
		}
#ifdef DP
		FPRINTF(stderr, "<< iomt_close\n");
#endif
		iomt_closesp(mt_ptr->access_id);
		dv->state = dev_closed;
	}
	return;
}
