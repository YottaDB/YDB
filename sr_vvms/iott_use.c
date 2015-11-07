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

#include <iodef.h>
#include <smgtrmptr.h>
#include <ssdef.h>
#include <trmdef.h>
#include <ttdef.h>
#include <tt2def.h>
#include <efndef.h>

#include "io.h"
#include "io_params.h"
#include "iottdef.h"
#include "nametabtyp.h"
#include "outofband.h"
#include "stringpool.h"
#include "namelook.h"
#include "copy.h"

LITDEF nametabent filter_names[] =
{
	 { 4, "CHAR*"},
	 { 3, "ESC*"},
	 { 6, "NOCHAR*"},
	 { 5, "NOESC*"}
};
LITDEF unsigned char filter_index[27] =
{
	0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2
	,2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4
	,4, 4, 4
};

GBLREF bool		ctrlc_on;
GBLREF uint4		std_dev_outofband_msk;
GBLREF uint4		spc_inp_prc;
GBLREF io_pair		io_std_device;
LITREF unsigned char	io_params_size[];

void iott_use(io_desc *iod, mval *pp)
{
	bool		flush_input;
	char		buf[512];
	int		fil_type;
	unsigned char	ch, len;
	short		field;
	int4		width, length;
	uint4 		mask_in, status;
	unsigned int	bufsz, buflen;
	unsigned int	req_code, args[4];
	d_tt_struct 	*out_ttptr, *in_ttptr, *tt_ptr;
	io_desc		*d_in, *d_out;
	io_termmask	mask_term;
	iosb		stat_blk;
	char		*tab;
	t_cap		s_mode;
	int		p_offset;

	error_def(ERR_DEVPARMNEG);
	error_def(ERR_TTINVFILTER);
	error_def(ERR_TTWIDTHTOOBIG);
	error_def(ERR_TTLENGTHTOOBIG);

	assert(iod->state == dev_open);
	assert(iod->type == tt);
	assert(iod->pair.in == iod || iod->pair.out == iod);
	iott_flush(iod);
	p_offset = 0;
	if (*(pp->str.addr + p_offset) != iop_eol)
	{
		tt_ptr = (d_tt_struct *)iod->dev_sp;
		bufsz = SIZEOF(buf);
		flush_input = FALSE;
		/* WARNING - the inclusion of an IOSB on this qio is NOT spurious!  Values in the IOSB
			are used, nay, required on the subsequent SETMODE call.  RTPAD will fail the
			SETMODE on certain circumstances when the IOSB is not present.
		*/
		status = sys$qiow(EFN$C_ENF, tt_ptr->channel,
			IO$_SENSEMODE, &stat_blk, 0, 0, &s_mode, 12, 0, 0, 0, 0);
		if (status == SS$_NORMAL)
			status = stat_blk.status;
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
		/* although it is only for safety, the following covers for
			read * (iott_rdone) and dm_read use of pasthru et al */
		s_mode.ext_cap &= (~TT2$M_PASTHRU);
		s_mode.ext_cap |= (tt_ptr->ext_cap & TT2$M_PASTHRU);
		s_mode.ext_cap &= (~TT2$M_EDITING);
		s_mode.ext_cap |= (tt_ptr->ext_cap & TT2$M_EDITING);
		s_mode.term_char &= (~TT$M_ESCAPE);
		s_mode.term_char |= (tt_ptr->term_char & TT$M_ESCAPE);
		s_mode.term_char &= (~TT$M_TTSYNC);
		s_mode.term_char |= (tt_ptr->term_char & TT$M_TTSYNC);
		d_in = iod->pair.in;
		d_out = iod->pair.out;
		in_ttptr = d_in->type == tt ? (d_tt_struct *)d_in->dev_sp : (d_tt_struct *)0;
		out_ttptr = d_out->type == tt ? (d_tt_struct *)d_out->dev_sp : (d_tt_struct *)0;
		if (in_ttptr)
		{
			in_ttptr->term_chars_twisted = FALSE; /* they were normalized above */
			mask_in = in_ttptr->item_list[0].addr;
			memcpy(&mask_term, in_ttptr->item_list[2].addr, SIZEOF(io_termmask));
		}
		while (*(pp->str.addr + p_offset) != iop_eol)
		{
			switch (ch = *(pp->str.addr + p_offset++))
			{
			case iop_canctlo:
				if (out_ttptr)
					out_ttptr->write_mask |= IO$M_CANCTRLO;
				break;
			case iop_cenable:
				if (iod == io_std_device.in && !ctrlc_on)
				{
					ctrlc_on = TRUE;
					std_dev_outofband_msk |= CTRLC_MSK;
					iott_resetast(iod);
				}
				break;
			case iop_nocenable:
				if (iod == io_std_device.in && ctrlc_on)
				{
					ctrlc_on = FALSE;
					std_dev_outofband_msk &= (~CTRLC_MSK);
					iott_resetast(iod);
				}
				break;
			case iop_clearscreen:
				if (out_ttptr)
				{
					status = sys$qiow(EFN$C_ENF, out_ttptr->channel, IO$_WRITEVBLK,
						&stat_blk, NULL, 0,
						out_ttptr->clearscreen.addr, out_ttptr->clearscreen.len,
						0, 0, 0, 0);
					if (status == SS$_NORMAL)
						status = stat_blk.status;
					if (status != SS$_NORMAL)
						rts_error(VARLSTCNT(1) status);
				}
				break;
			case iop_convert:
				mask_in |= TRM$M_TM_CVTLOW;
				break;
			case iop_noconvert:
				mask_in &= (~TRM$M_TM_CVTLOW);
				break;
			case iop_ctrap:
				if (in_ttptr)
				{
					in_ttptr->enbld_outofbands.mask = *((uint4 *)(pp->str.addr + p_offset));
					iott_resetast(iod);
				}
				break;
			case iop_downscroll:
				if (out_ttptr && out_ttptr->term_tab_entry)
				{
					args[0] = 2;
					args[1] = d_out->dollar.y;
					args[2] = 1;
					req_code = SMG$K_SET_CURSOR_ABS;
					status = smg$get_term_data (&out_ttptr->term_tab_entry,
						&req_code, &bufsz, &buflen, buf, args);
					if (!(status & 1))
						rts_error(VARLSTCNT(1) status);
					status = sys$qiow(EFN$C_ENF, out_ttptr->channel, IO$_WRITEVBLK,
						&stat_blk, NULL, 0, buf, buflen, 0, 0, 0, 0);
					if (status == SS$_NORMAL)
						status = stat_blk.status;
					if (status != SS$_NORMAL)
						rts_error(VARLSTCNT(1) status);

					if (d_out->dollar.y > 0)
					{
						d_out->dollar.y --;
						if (d_out->length)
							d_out->dollar.y %= d_out->length;
					}
					d_out->dollar.x = 0;
				}
				break;
			case iop_echo:
				if (in_ttptr)
					s_mode.term_char &= (~TT$M_NOECHO);
				break;
			case iop_noecho:
				if (in_ttptr)
					s_mode.term_char |= TT$M_NOECHO;
				break;
			case iop_editing:
				if (in_ttptr)
					s_mode.ext_cap |= TT2$M_EDITING;
				break;
			case iop_noediting:
				if (in_ttptr)
					s_mode.ext_cap &= (~TT2$M_EDITING);
				break;
			case iop_escape:
				if (in_ttptr)
					s_mode.term_char |= TT$M_ESCAPE;
				break;
			case iop_noescape:
				if (in_ttptr)
					s_mode.term_char &= (~TT$M_ESCAPE);
			default:
				break;
			case iop_eraseline:
				if (out_ttptr)
				{
					status = sys$qiow(EFN$C_ENF, out_ttptr->channel, IO$_WRITEVBLK,
						&stat_blk, NULL, 0,
						out_ttptr->erase_to_end_line.addr, out_ttptr->erase_to_end_line.len,
						0, 0, 0, 0);
					if (status == SS$_NORMAL)
						status = stat_blk.status;
					if (status != SS$_NORMAL)
						rts_error(VARLSTCNT(1) status);
				}
				break;
			case iop_exception:
				iod->error_handler.len = *(pp->str.addr + p_offset);
				iod->error_handler.addr = pp->str.addr + p_offset + 1;
				s2pool(&iod->error_handler);
				break;
			case iop_filter:
				len = *(pp->str.addr + p_offset);
				tab = pp->str.addr + p_offset + 1;
				if ((fil_type = namelook(filter_index, filter_names, tab, len)) < 0)
				{
					rts_error(VARLSTCNT(1) ERR_TTINVFILTER);
					return;
				}
				switch (fil_type)
				{
					case 0:
						iod->write_filter |= CHAR_FILTER;
						break;
					case 1:
						iod->write_filter |= ESC1;
						break;
					case 2:
						iod->write_filter &= ~CHAR_FILTER;
						break;
					case 3:
						iod->write_filter &= ~ESC1;
						break;
				}
				break;
			case iop_nofilter:
				iod->write_filter = 0;
				break;
			case iop_field:
				GET_SHORT(field, pp->str.addr + p_offset);
				if (field < 0)
					rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
				if (in_ttptr)
					if (field == 0)
						in_ttptr->in_buf_sz = TTDEF_BUF_SZ;
					else
						in_ttptr->in_buf_sz = field;
				break;
			case iop_flush:
				flush_input = TRUE;
				break;
			case iop_hostsync:
				if (in_ttptr)
					s_mode.term_char |= TT$M_HOSTSYNC;
				break;
			case iop_nohostsync:
				if (in_ttptr)
					s_mode.term_char &= (~TT$M_HOSTSYNC);
				break;
			case iop_insert:
				if (in_ttptr)
					s_mode.ext_cap |= TT2$M_INSERT;
				break;
			case iop_noinsert:
				if (in_ttptr)
					s_mode.ext_cap &= (~TT2$M_INSERT);
				break;
			case iop_length:
				GET_LONG(length, pp->str.addr + p_offset);
				if (length < 0)
					rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
				if (length > TTMAX_PG_LENGTH)
					rts_error(VARLSTCNT(1) ERR_TTLENGTHTOOBIG);
				s_mode.pg_length = length;
				d_out->length = length;
				break;
			case iop_pasthru:
				if (in_ttptr)
				{
					s_mode.ext_cap |= TT2$M_PASTHRU;
					if ((spc_inp_prc & (SHFT_MSK << CTRL_U)) && (s_mode.term_char & TT$M_SCOPE))
					{
						in_ttptr->ctrlu_msk =0;
						iott_resetast(iod);
					}
				}
				break;
			case iop_nopasthru:
				if (in_ttptr)
				{
					s_mode.ext_cap &= (~TT2$M_PASTHRU);
					if ((spc_inp_prc & (SHFT_MSK << CTRL_U)) && (s_mode.term_char & TT$M_SCOPE))
					{
						in_ttptr->ctrlu_msk = (SHFT_MSK << CTRL_U);
						iott_resetast(iod);
					}
				}
				break;
			case iop_readsync:
				if (in_ttptr)
					s_mode.term_char |= TT$M_READSYNC;
				break;
			case iop_noreadsync:
				if (in_ttptr)
					s_mode.term_char &= (~TT$M_READSYNC);
				break;
			case iop_terminator:
				memcpy(&mask_term.mask[0], (pp->str.addr + p_offset), SIZEOF(io_termmask));
				if (mask_term.mask[0] == NUL &&
				    mask_term.mask[1] == NUL &&
				    mask_term.mask[2] == NUL &&
				    mask_term.mask[3] == NUL &&
				    mask_term.mask[4] == NUL &&
				    mask_term.mask[5] == NUL &&
				    mask_term.mask[6] == NUL &&
				    mask_term.mask[7] == NUL)
					mask_term.mask[0] = TERM_MSK;
				break;
			case iop_noterminator:
				memset(&mask_term.mask[0], 0, SIZEOF(io_termmask));
				break;
			case iop_ttsync:
				s_mode.term_char |= TT$M_TTSYNC;
				break;
			case iop_nottsync:
				s_mode.term_char &= (~TT$M_TTSYNC);
				break;
			case iop_typeahead:
				if (in_ttptr)
					s_mode.term_char &= (~TT$M_NOTYPEAHD);
				break;
			case iop_notypeahead:
				if (in_ttptr)
					s_mode.term_char |= TT$M_NOTYPEAHD;
				break;
			case iop_upscroll:
				if (out_ttptr && out_ttptr->term_tab_entry)
				{
					args[0] = 2;
					args[1] = d_out->dollar.y + 2;
					args[2] = 1;
					req_code = SMG$K_SET_CURSOR_ABS;
					status = smg$get_term_data (&out_ttptr->term_tab_entry, &req_code,
						&bufsz, &buflen, buf, args);
					if (!(status & 1))
						rts_error(VARLSTCNT(1) status);
					status = sys$qiow(EFN$C_ENF, out_ttptr->channel, IO$_WRITEVBLK,
						&stat_blk, NULL, 0, buf, buflen, 0, 0, 0, 0);
					if (status == SS$_NORMAL)
						status = stat_blk.status;
					if (status != SS$_NORMAL)
						rts_error(VARLSTCNT(1) status);

					d_out->dollar.y++;
					if (d_out->length)
						d_out->dollar.y %= d_out->length;
					d_out->dollar.x = 0;
				}
				break;
			case iop_width:
				GET_LONG(width, pp->str.addr + p_offset);
				if (width < 0)
					rts_error(VARLSTCNT(1) ERR_DEVPARMNEG);
				if (width > TTMAX_PG_WIDTH)
					rts_error(VARLSTCNT(1) ERR_TTWIDTHTOOBIG);
				if (width == 0)
				{
					s_mode.term_char &= (~TT$M_WRAP);
					d_out->wrap = FALSE;
					s_mode.pg_width = TTDEF_PG_WIDTH;
					d_out->width = TTDEF_PG_WIDTH;
				} else
				{
		/* ******** later with a ring buffer this will change to indicate ******** */
		/* ******** location of a carriage return on an extended write    ******** */
					s_mode.pg_width = width;
					d_out->width = width;
					s_mode.term_char |= TT$M_WRAP;
					d_out->wrap = TRUE;
				}
				break;
			case iop_wrap:
				s_mode.term_char |= TT$M_WRAP;
				d_out->wrap = TRUE;
				break;
			case iop_nowrap:
				s_mode.term_char &= (~TT$M_WRAP);
				d_out->wrap = FALSE;
				break;
			case iop_x:
				if (out_ttptr && out_ttptr->term_tab_entry)
				{
					GET_LONG(d_out->dollar.x, pp->str.addr + p_offset);
					if (d_out->dollar.x < 0)
						d_out->dollar.x = 0;
					if (d_out->dollar.x > d_out->width && d_out->wrap)
					{
						d_out->dollar.y += (d_out->dollar.x / d_out->width);
						if (d_out->length)
							d_out->dollar.y %= d_out->length;
						d_out->dollar.x	%= d_out->width;
					}
					args[0] = 2;
					args[1] = d_out->dollar.y + 1;
					args[2] = d_out->dollar.x + 1;
					req_code = SMG$K_SET_CURSOR_ABS;
					status = smg$get_term_data (&out_ttptr->term_tab_entry,
						&req_code, &bufsz, &buflen, buf, args);
					if (!(status & 1))
						rts_error(VARLSTCNT(1) status);
					status = sys$qiow(EFN$C_ENF, out_ttptr->channel, IO$_WRITEVBLK,
						&stat_blk, NULL, 0, buf, buflen, 0, 0, 0, 0);
					if (status == SS$_NORMAL)
						status = stat_blk.status;
					if (status != SS$_NORMAL)
						rts_error(VARLSTCNT(1) status);
				}
				break;
			case iop_y:
				if (out_ttptr && out_ttptr->term_tab_entry)
				{
					GET_LONG(d_out->dollar.y, pp->str.addr + p_offset);
					if (d_out->dollar.y < 0)
						d_out->dollar.y = 0;
					if (d_out->length)
						d_out->dollar.y %= d_out->length;
					args[0] = 2;
					args[1] = d_out->dollar.y + 1;
					args[2] = d_out->dollar.x + 1;
					req_code = SMG$K_SET_CURSOR_ABS;
					status = smg$get_term_data (&out_ttptr->term_tab_entry,
						&req_code, &bufsz, &buflen, buf, args);
					if (!(status & 1))
						rts_error(VARLSTCNT(1) status);
					status = sys$qiow(EFN$C_ENF, out_ttptr->channel, IO$_WRITEVBLK,
						&stat_blk, NULL, 0, buf, buflen, 0, 0, 0, 0);
					if (status == SS$_NORMAL)
						status = stat_blk.status;
					if (status != SS$_NORMAL)
						rts_error(VARLSTCNT(1) status);
				}
				break;
			}
			p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
				(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
		}
		status = sys$qiow(EFN$C_ENF, tt_ptr->channel, IO$_SETMODE,
			&stat_blk, NULL, 0, &s_mode, 12, 0, 0, 0, 0);
		if (status == SS$_NORMAL)
			status = stat_blk.status;
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
		if (in_ttptr)
		{
			in_ttptr->item_list[0].addr = mask_in;
			in_ttptr->term_char = s_mode.term_char;
			in_ttptr->ext_cap = s_mode.ext_cap;
			memcpy(in_ttptr->item_list[2].addr, &mask_term, SIZEOF(io_termmask));
			if (flush_input)
			{
				status = sys$qiow(EFN$C_ENF, in_ttptr->channel,
			  		IO$_READVBLK | IO$M_PURGE | IO$M_TIMED | IO$M_NOECHO,
					&stat_blk, NULL, 0, &flush_input, 1, 0, 0, 0, 0);
				if (status == SS$_NORMAL)
					status = stat_blk.status;
				if ((status != SS$_NORMAL) && (status != SS$_TIMEOUT))
					rts_error(VARLSTCNT(1) status);
			}
		}
	}
	return;
}
