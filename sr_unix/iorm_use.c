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

#include "gtm_string.h"

#include <errno.h>
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_iconv.h"

#include "copy.h"
#include "io.h"
#include "iormdef.h"
#include "io_params.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "stringpool.h"

typedef struct
{
	unsigned short mem;
	unsigned short grp;
} uic_struct;

LITREF unsigned char	io_params_size[];

void	iorm_use(io_desc *iod, mval *pp)
{
	unsigned char	c;
	short		mode, mode1;
	unsigned short	length, width;
	long		size;
	int		fstat_res;
	d_rm_struct	*rm_ptr;
	struct stat	statbuf;
	int		p_offset;

	error_def(ERR_DEVPARMNEG);
	error_def(ERR_RMWIDTHPOS);

	p_offset = 0;
	rm_ptr = (d_rm_struct *)iod->dev_sp;
	FSTAT_FILE(rm_ptr->fildes, &statbuf, fstat_res);
	if (-1 == fstat_res)
		rts_error(VARLSTCNT(1) errno);
	mode = mode1 = statbuf.st_mode;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		assert((params) *(pp->str.addr + p_offset) < (params)n_iops);
		switch (c = *(pp->str.addr + p_offset++))
		{
		case iop_exception:
			iod->error_handler.len = *(pp->str.addr + p_offset);
			iod->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&iod->error_handler);
			break;
		case iop_fixed:
			if (iod->state != dev_open)
				rm_ptr->fixed = TRUE;
			break;
		case iop_nofixed:
			if (iod->state != dev_open)
				rm_ptr->fixed = FALSE;
			break;
		case iop_length:
			GET_USHORT(length, (pp->str.addr + p_offset));
			if (length < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			iod->length = length;
			break;
		case iop_w_protection:
			mode &= ~(0x07);
			mode |= *(pp->str.addr + p_offset);
			break;
		case iop_g_protection:
			mode &= ~(0x07 << 3);
			mode |= *(pp->str.addr + p_offset) << 3;
			break;
		case iop_s_protection:
		case iop_o_protection:
			mode &= ~(0x07 << 6);
			mode |= *(pp->str.addr + p_offset) << 6;
			break;
		case iop_readonly:
			rm_ptr->noread = TRUE;
			break;
		case iop_noreadonly:
			rm_ptr->noread = FALSE;
			break;
		case iop_recordsize:
			GET_USHORT(width, (pp->str.addr + p_offset));
			if (width <= 0)
				rts_error(VARLSTCNT(1) ERR_RMWIDTHPOS);
			iod->width = width;
			break;
		case iop_rewind:
			if (iod->state == dev_open && !rm_ptr->fifo)
			{
				iorm_flush(iod);
				if (lseek(rm_ptr->fildes, (off_t)0, SEEK_SET) == -1)
					rts_error(VARLSTCNT(1) errno);
				if (fseek(rm_ptr->filstr, (long)0, SEEK_SET) == -1)	/* Rewind the input stream */
					rts_error(VARLSTCNT(1) errno);
				iod->dollar.zeof = FALSE;
				iod->dollar.y = 0;
				iod->dollar.x = 0;
				rm_ptr->lastop = RM_NOOP;
			}
			break;
		case iop_stream:
			rm_ptr->stream = TRUE;
			break;
		case iop_truncate:
			if (!rm_ptr->fifo)
			{
				/* Warning! ftell() returns a long and fseek only accepts a long
				 * as its second argument.  this may cause problems for files longer
				 * the 2Gb.
				 */
				if ((size = ftell(rm_ptr->filstr)) != -1)
				{
					int ftruncate_res;

					if (lseek(rm_ptr->fildes, (off_t)size, SEEK_SET) == -1)
						rts_error(VARLSTCNT(1) errno);
					FTRUNCATE(rm_ptr->fildes, (off_t)size, ftruncate_res);
					if (fseek(rm_ptr->filstr, size, SEEK_SET) == -1)
						rts_error(VARLSTCNT(1) errno);
					iod->dollar.zeof = TRUE;
				}
			}
			break;
		case iop_uic:
			{
				unsigned char	*ch, ct, *end;
				int		chown_res;
				uic_struct	uic;

				ch = (unsigned char *)pp->str.addr + p_offset;
				ct = *ch++;
				end = ch + ct;
				uic.grp = uic.mem = 0;
				while ((*ch != ',') && (ch < end))
					uic.mem = (10 * uic.mem) + (*ch++ - '0');
				if (*ch == ',')
				{
					while (++ch < end)
						uic.grp = (10 * uic.grp) + (*ch - '0');
				}
				CHG_OWNER(iod->trans_name->dollar_io, uic.mem, uic.grp, chown_res);
				if (-1 == chown_res)
					rts_error(VARLSTCNT(1) errno);
				break;
			}
		case iop_width:
			assert(iod->state == dev_open);
			GET_USHORT(width, (pp->str.addr + p_offset));
			if (width <= 0)
				rts_error(VARLSTCNT(1) ERR_RMWIDTHPOS);
			iod->width = width;
			iod->wrap = TRUE;
			break;
		case iop_wrap:
			iod->wrap = TRUE;
			break;
		case iop_nowrap:
			iod->wrap = FALSE;
			break;
		case iop_ipchset:
			{
				if ( (iconv_t)0 != iod->input_conv_cd )
				{
					ICONV_CLOSE_CD(iod->input_conv_cd);
				}
				SET_CODE_SET(iod->in_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->in_code_set)
					ICONV_OPEN_CD(iod->input_conv_cd, (char *)(pp->str.addr + p_offset + 1),
												INSIDE_CH_SET);
                        	break;
			}
                case iop_opchset:
			{
				if ( (iconv_t) 0 != iod->output_conv_cd )
				{
					ICONV_CLOSE_CD(iod->output_conv_cd);
				}
				SET_CODE_SET(iod->out_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->out_code_set)
					ICONV_OPEN_CD(iod->output_conv_cd, INSIDE_CH_SET,
							(char *)(pp->str.addr + p_offset + 1));
                        	break;
			}
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[c]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[c]);
	}
	if (mode != mode1)
	{	/* if the mode has been changed by the qualifiers, reset it */
		if (-1 == CHMOD(iod->trans_name->dollar_io, mode))
			rts_error(VARLSTCNT(1) errno);
	}
	return;
}
