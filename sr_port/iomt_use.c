/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_iconv.h"
#include "gtm_string.h"

#include "copy.h"
#include "io_params.h"
#include "io.h"
#include "iosp.h"
#include "iottdef.h"
#include "iomtdef.h"
#include "nametabtyp.h"
#include "stringpool.h"
#include "namelook.h"

static readonly nametabent mtlab_names[] =
{
	 {3, "ANS"}, {4,"ANSI"}, {3, "DOS"}, {5, "DOS11"}
};
static readonly unsigned char mtlab_index[27] =
{
	0, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4
	,4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
	,4, 4, 4
};

static readonly char mtlab_type[]={MTLAB_ANSI, MTLAB_ANSI, MTLAB_DOS11, MTLAB_DOS11};

static readonly nametabent mtwtlab_names[] =
{
	{4, "EOF1"}, {4,"EOF2"}, {4, "HDR1"}, {4, "HDR2"}, {4, "VOL1"}
};

static readonly unsigned char mtwtlab_index[27] =
{
	0, 0, 0, 0, 0, 2, 2, 2, 4, 4, 4, 4, 4
	,4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5
	,5, 5, 5
};

static readonly char mtwtlab_type[]={MTL_EOF1, MTL_EOF2, MTL_HDR1, MTL_HDR2, MTL_VOL1};

LITREF unsigned char io_params_size[];

void iomt_use(io_desc *iod, mval *pp)
{
	unsigned char	ch, len;
	int		lab_type;
	int4		length, width;
	int4		skips;
	d_mt_struct	*mt_ptr, *out_ptr;
	io_desc		*d_in, *d_out;
	char		*tab;
	int		p_offset;

	error_def(ERR_MTINVLAB);
	error_def(ERR_DEVPARMNEG);
	error_def(ERR_UNIMPLOP);

	p_offset = 0;
	d_in = iod->pair.in;
	d_out = iod->pair.out;
	mt_ptr = (d_mt_struct *)iod->dev_sp;
	out_ptr = (d_mt_struct *)d_out->dev_sp;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		switch (ch = *(pp->str.addr + p_offset++))
		{
		case iop_ebcdic:
			mt_ptr->ebcdic = TRUE;
			break;
		case iop_noebcdic:
			mt_ptr->ebcdic = FALSE;
			break;
		case iop_newversion:
			mt_ptr->newversion = TRUE;
			break;
		case iop_label:
			len = *(pp->str.addr + p_offset);
			tab = pp->str.addr + p_offset + 1;
			if ((lab_type = namelook(mtlab_index, mtlab_names, tab, len)) < 0)
				rts_error(VARLSTCNT(1) ERR_MTINVLAB);
			mt_ptr->labeled = mtlab_type[lab_type];
			break;
		case iop_nolabel:
			mt_ptr->labeled = FALSE;
			break;
		case iop_rdcheckdata:
			mt_ptr->read_mask |= IO_M_DATACHECK;
			break;
		case iop_nordcheckdata:
			mt_ptr->read_mask &= (~(IO_M_DATACHECK));
			break;
		case iop_wtcheckdata:
			mt_ptr->write_mask |= IO_M_DATACHECK;
			break;
		case iop_nowtcheckdata:
			mt_ptr->write_mask &= (~(IO_M_DATACHECK));
			break;
		case iop_inhretry:
			mt_ptr->write_mask |= IO_M_INHRETRY;
			mt_ptr->read_mask |= IO_M_INHRETRY;
			break;
		case iop_retry:
			mt_ptr->write_mask &= ~IO_M_INHRETRY;
			mt_ptr->read_mask &= ~IO_M_INHRETRY;
			break;
		case iop_inhextgap:
			mt_ptr->write_mask |= IO_M_INHEXTGAP;
			break;
		case iop_extgap:
			mt_ptr->write_mask &= ~IO_M_INHEXTGAP;
			break;
		case iop_length:
			GET_LONG(length, (pp->str.addr + p_offset));
			if (length < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			iod->length = length;
			break;
		case iop_width:
			GET_LONG(width, (pp->str.addr + p_offset));
			if (width < 0)
				rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
			if (width == 0)
			{
				iod->wrap = FALSE;
				iod->width = mt_ptr->record_sz;
			} else  if (width <= mt_ptr->record_sz)
			{
				iomt_flush(iod);
				iod->width = width;
				iod->wrap = TRUE;
			}
			break;
		case iop_wrap:
			out_ptr->wrap = TRUE;
			break;
		case iop_nowrap:
			out_ptr->wrap = FALSE;
			break;
		case iop_skipfile:
			GET_LONG(skips, (pp->str.addr + p_offset));
			iomt_skipfile(iod, skips);
			break;
		case iop_unload:
			assert(FALSE);
			break;
		case iop_rewind:
			iomt_rewind(iod);
			break;
		case iop_erasetape:
			iomt_erase(iod);
			break;
		case iop_space:
			GET_LONG(skips, (pp->str.addr + p_offset));
			iomt_skiprecord(iod, skips);
			break;
		case iop_writeof:
			iomt_eof(iod);
			break;
		case iop_writetm:
			iomt_tm(iod);
			break;
		case iop_writelb:
			len = *(pp->str.addr + p_offset);
			tab = pp->str.addr + p_offset + 1;
			if ((lab_type = namelook(mtwtlab_index, mtwtlab_names, tab, len)) < 0)
				rts_error(VARLSTCNT(1) ERR_MTINVLAB);
			iomt_wtansilab(iod, mtwtlab_type[lab_type]);
			break;
		case iop_next:
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
			break;
		case iop_exception:
			iod->error_handler.len = *(pp->str.addr + p_offset);
			iod->error_handler.addr = (char *)(pp->str.addr + p_offset + 1);
			s2pool(&iod->error_handler);
			break;
		case iop_ipchset:
			{
#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
				if ( (iconv_t)0 != iod->input_conv_cd )
				{
					ICONV_CLOSE_CD(iod->input_conv_cd);
				}
				SET_CODE_SET(iod->in_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->in_code_set)
					ICONV_OPEN_CD(iod->input_conv_cd, (char *)(pp->str.addr + p_offset + 1), INSIDE_CH_SET);
#endif
                        	break;
			}
                case iop_opchset:
			{
#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
				if ( (iconv_t) 0 != iod->output_conv_cd )
				{
					ICONV_CLOSE_CD(iod->output_conv_cd);
				}
				SET_CODE_SET(iod->out_code_set, (char *)(pp->str.addr + p_offset + 1));
				if (DEFAULT_CODE_SET != iod->out_code_set)
					ICONV_OPEN_CD(iod->output_conv_cd, INSIDE_CH_SET, (char *)(pp->str.addr + p_offset + 1));
#endif
                        	break;
			}
		default:
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
}
