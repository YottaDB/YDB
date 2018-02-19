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

#include <sys/ioctl.h>
#include <errno.h>

#include "gtm_fcntl.h"
#include "gtm_string.h"
#include "gtm_iconv.h"
#include "gtm_termios.h"
#include "gtm_unistd.h"
#include "gtm_signal.h"	/* for SIGPROCMASK used inside Tcsetattr */

#include "io_params.h"
#include "io.h"
#include "iottdef.h"
#include "iosp.h"
#include "trmdef.h"
#include "nametabtyp.h"
#include "copy.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "stringpool.h"
#include "send_msg.h"
#include "namelook.h"
#include "gtm_conv.h"
#include "error.h"
#include "gtm_tputs.h"
#include "gtm_tparm.h"
#include "outofband.h"
#include "restrict.h"
#include "op.h"
#include "indir_enum.h"

LITDEF nametabent filter_names[] =
{
	{4, "CHAR*"},
	{3, "ESC*"},
	{6, "NOCHAR*"},
	{5, "NOESC*"}
};
LITDEF unsigned char filter_index[27] =
{
	0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2
	,2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4
	,4, 4, 4
};

GBLREF boolean_t		ctrlc_on, dollar_zininterrupt;
GBLREF char			*CURSOR_ADDRESS, *CLR_EOL, *CLR_EOS;
GBLREF io_pair			io_std_device;
GBLREF io_pair			io_curr_device;
GBLREF void 			(*ctrlc_handler_ptr)();

LITREF unsigned char	io_params_size[];

error_def(ERR_DEVPARMNEG);
error_def(ERR_NOPRINCIO);
error_def(ERR_SYSCALL);
error_def(ERR_TCGETATTR);
error_def(ERR_TCSETATTR);
error_def(ERR_TTINVFILTER);
error_def(ERR_WIDTHTOOSMALL);
error_def(ERR_ZINTRECURSEIO);

void iott_use(io_desc *iod, mval *pp)
{
	boolean_t		flush_input, terminator_specified = FALSE;
	char			dc1, *ttab;
	d_tt_struct		*temp_ptr, *tt_ptr;
	int			p_offset, fil_type, save_errno, status;
	int4			length, width;
	io_desc			*d_in, *d_out;
	io_termmask		mask_term;
	struct sigaction	act;
	struct termios		t;
	mstr			chset_mstr;
	gtm_chset_t		temp_chset, old_ochset, old_ichset;
	uint4			mask_in;
	unsigned char		ch, len;
	boolean_t		ch_set;
	mval			mv;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	p_offset = 0;
	assert(iod->state == dev_open);
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
	iott_flush(iod);
	tt_ptr = (d_tt_struct *)iod->dev_sp;
	if (*(pp->str.addr + p_offset) != iop_eol)
	{
		if (tt_ptr->mupintr)
			if (dollar_zininterrupt)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
			else
			{	/* The interrupted read was not properly resumed so clear it now */
				tt_ptr->mupintr = FALSE;
				tt_ptr->tt_state_save.who_saved = ttwhichinvalid;
				io_find_mvstent(iod, TRUE);
			}
		status = tcgetattr(tt_ptr->fildes, &t);
		if (0 != status)
		{
			save_errno = errno;
			ISSUE_NOPRINCIO_IF_NEEDED_TT(io_curr_device.out);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TCGETATTR, 1, tt_ptr->fildes, save_errno);
		}
		flush_input = FALSE;
		d_in = iod->pair.in;
		d_out = iod->pair.out;
		temp_ptr = (d_tt_struct *)d_in->dev_sp;
		mask_in = temp_ptr->term_ctrl;
		mask_term = temp_ptr->mask_term;
		old_ochset = iod->ochset;
		old_ichset = iod->ichset;
		while (*(pp->str.addr + p_offset) != iop_eol)
		{
			switch (ch = *(pp->str.addr + p_offset++))
			{
				case iop_canonical:
					tt_ptr->canonical = TRUE;
					t.c_lflag |= ICANON;
					break;
				case iop_nocanonical:
					tt_ptr->canonical = FALSE;
					t.c_lflag &= ~(ICANON);
					break;
				case iop_empterm:
					tt_ptr->ext_cap |= TT_EMPTERM;
					break;
				case iop_noempterm:
					tt_ptr->ext_cap &= ~TT_EMPTERM;
					break;
				case iop_cenable:
					if (!ctrlc_on && !RESTRICTED(cenable))
					{	/* if it's already cenable, no need to change */
						temp_ptr = (d_tt_struct *)io_std_device.in->dev_sp;
						if (tt_ptr->fildes == temp_ptr->fildes)
						{	/* if this is $PRINCIPAL make sure the ctrlc_handler is enabled */
							sigemptyset(&act.sa_mask);
							act.sa_flags = 0;
							act.sa_handler = ctrlc_handler_ptr;
							sigaction(SIGINT, &act, 0);
							ctrlc_on = TRUE;
						}
					}
					break;
				case iop_nocenable:
					if (ctrlc_on && !RESTRICTED(cenable))
					{	/* if it's already nocenable, no need to change */
						temp_ptr = (d_tt_struct *)io_std_device.in->dev_sp;
						if (tt_ptr->fildes == temp_ptr->fildes)
						{	/* if this is $PRINCIPAL may disable the ctrlc_handler */
							if (0 == (CTRLC_MSK & tt_ptr->enbld_outofbands.mask))
							{	/* but only if ctrap=$c(3) is not active */
								sigemptyset(&act.sa_mask);
								act.sa_flags = 0;
								act.sa_handler = SIG_IGN;
								sigaction(SIGINT, &act, 0);
							}
							ctrlc_on = FALSE;
						}
					}
					break;
				case iop_clearscreen:
					if (NULL != CLR_EOS)
						gtm_tputs(CLR_EOS, 1, outc);
					break;
				case iop_convert:
					mask_in |= TRM_CONVERT;
					break;
				case iop_noconvert:
					mask_in &= ~TRM_CONVERT;
					break;
				case iop_ctrap:
					GET_LONG(tt_ptr->enbld_outofbands.mask, pp->str.addr + p_offset);
					if (!ctrlc_on)
					{	/* if cenable, ctrlc_handler active anyway, otherwise, depends on ctrap=$c(3) */
						sigemptyset(&act.sa_mask);
						act.sa_flags = 0;
						act.sa_handler = (CTRLC_MSK & tt_ptr->enbld_outofbands.mask)
							? ctrlc_handler_ptr : SIG_IGN;
						sigaction(SIGINT, &act, 0);
					}
					break;
				case iop_downscroll:
					if (d_out->dollar.y > 0)
					{
						d_out->dollar.y--;
						if (NULL != CURSOR_ADDRESS)
							gtm_tputs(gtm_tparm(CURSOR_ADDRESS, d_out->dollar.y, d_out->dollar.x), 1,
								  outc);
					}
					break;
				case iop_echo:
					mask_in &= (~TRM_NOECHO);
					break;
				case iop_noecho:
					mask_in |= TRM_NOECHO;
					break;
				case iop_editing:
					if (io_curr_device.in == io_std_device.in)
					{	/* $PRINCIPAL only */
						tt_ptr->ext_cap |= TT_EDITING;
						if (!tt_ptr->recall_buff.addr)
						{
							assert(tt_ptr->in_buf_sz);
							tt_ptr->recall_buff.addr = malloc(tt_ptr->in_buf_sz);
							tt_ptr->recall_size = tt_ptr->in_buf_sz;
							tt_ptr->recall_buff.len = 0;    /* nothing in buffer */
						}
					}
					break;
				case iop_noediting:
					if (io_curr_device.in == io_std_device.in)
						tt_ptr->ext_cap &= ~TT_EDITING;	/* $PRINCIPAL only */
					break;
				case iop_escape:
					mask_in |= TRM_ESCAPE;
					break;
				case iop_noescape:
					mask_in &= (~TRM_ESCAPE);
					default:
					break;
				case iop_eraseline:
					if (NULL != CLR_EOL)
						gtm_tputs(CLR_EOL, 1, outc);
					break;
				case iop_exception:
					DEF_EXCEPTION(pp, p_offset, iod);
					break;
				case iop_filter:
					len = *(pp->str.addr + p_offset);
					ttab = pp->str.addr + p_offset + 1;
					if ((fil_type = namelook(filter_index, filter_names, ttab, len)) < 0)
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TTINVFILTER);
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
				case iop_flush:
					flush_input = TRUE;
					break;
				case iop_hostsync:
					t.c_iflag |= IXOFF;
					break;
				case iop_nohostsync:
					t.c_iflag &= ~IXOFF;
					break;
				case iop_insert:
					if (io_curr_device.in == io_std_device.in)
						tt_ptr->ext_cap &= ~TT_NOINSERT;	/* $PRINCIPAL only */
					break;
				case iop_noinsert:
					if (io_curr_device.in == io_std_device.in)
						tt_ptr->ext_cap |= TT_NOINSERT;	/* $PRINCIPAL only */
					break;
				case iop_length:
					GET_LONG(length, pp->str.addr + p_offset);
					if (0 > length)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVPARMNEG);
					d_out->length = length;
					break;
				case iop_pasthru:
					mask_in |= TRM_PASTHRU;
					break;
				case iop_nopasthru:
					mask_in &= (~TRM_PASTHRU);
					break;
				case iop_readsync:
					mask_in |= TRM_READSYNC;
					break;
				case iop_noreadsync:
					dc1 = (char)17;
					temp_ptr = (d_tt_struct *)io_std_device.in->dev_sp;
					DOWRITERC(temp_ptr->fildes, &dc1, 1, status);
					if (0 != status)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);
					mask_in &= (~TRM_READSYNC);
					break;
				case iop_terminator:
					memcpy(&mask_term.mask[0], (pp->str.addr + p_offset), SIZEOF(io_termmask));
					terminator_specified = TRUE;
					temp_ptr = (d_tt_struct *)d_in->dev_sp;
					if (mask_term.mask[0] == NUL &&
						mask_term.mask[1] == NUL &&
						mask_term.mask[2] == NUL &&
						mask_term.mask[3] == NUL &&
						mask_term.mask[4] == NUL &&
						mask_term.mask[5] == NUL &&
						mask_term.mask[6] == NUL &&
						mask_term.mask[7] == NUL)
						temp_ptr->default_mask_term = TRUE;
					else
						temp_ptr->default_mask_term = FALSE;
					break;
				case iop_noterminator:
					temp_ptr = (d_tt_struct *)d_in->dev_sp;
					temp_ptr->default_mask_term = FALSE;
					memset(&mask_term.mask[0], 0, SIZEOF(io_termmask));
					break;
				case iop_ttsync:
					t.c_iflag |= IXON;
					break;
				case iop_nottsync:
					t.c_iflag &= ~IXON;
					break;
				case iop_typeahead:
					mask_in &= (~TRM_NOTYPEAHD);
					break;
				case iop_notypeahead:
					mask_in |= TRM_NOTYPEAHD;
					break;
				case iop_upscroll:
					d_out->dollar.y++;
					if (d_out->length)
						d_out->dollar.y %= d_out->length;
					if (NULL != CURSOR_ADDRESS)
						gtm_tputs(gtm_tparm(CURSOR_ADDRESS, d_out->dollar.y, d_out->dollar.x), 1, outc);
					break;
				case iop_width:
					GET_LONG(width, pp->str.addr + p_offset);
					if (0 > width)
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DEVPARMNEG);
					/* Do not allow a WIDTH of 1 if UTF mode (ICHSET or OCHSET is not M) */
					if ((1 == width) && ((CHSET_M != d_in->ochset) || (CHSET_M != d_in->ichset)))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_WIDTHTOOSMALL);
					if (0 == width)
					{
						d_out->wrap = FALSE;
						d_out->width = TTDEF_PG_WIDTH;
					} else
					{
						d_out->width = width;
						d_out->wrap = TRUE;
					}
					break;
				case iop_wrap:
					d_out->wrap = TRUE;
					break;
				case iop_nowrap:
					d_out->wrap = FALSE;
					break;
				case iop_x:
					GET_LONG(d_out->dollar.x, pp->str.addr + p_offset);
					if (0 > (int4)d_out->dollar.x)
						d_out->dollar.x = 0;
					if (d_out->dollar.x > d_out->width && d_out->wrap)
					{
						d_out->dollar.y += (d_out->dollar.x / d_out->width);
						if (d_out->length)
							d_out->dollar.y %= d_out->length;
						d_out->dollar.x	%= d_out->width;
					}
					if (NULL != CURSOR_ADDRESS)
						gtm_tputs(gtm_tparm(CURSOR_ADDRESS, d_out->dollar.y, d_out->dollar.x), 1, outc);
					break;
				case iop_y:
					GET_LONG(d_out->dollar.y, pp->str.addr + p_offset);
					if (0 > (int4)d_out->dollar.y)
						d_out->dollar.y = 0;
					if (d_out->length)
						d_out->dollar.y %= d_out->length;
					if (NULL != CURSOR_ADDRESS)
						gtm_tputs(gtm_tparm(CURSOR_ADDRESS, d_out->dollar.y, d_out->dollar.x), 1, outc);
					break;
				case iop_ipchset:
					{
#						ifdef KEEP_zOS_EBCDIC
						if ((iconv_t)0 != iod->input_conv_cd)
						{
							ICONV_CLOSE_CD(iod->input_conv_cd);
						}
						SET_CODE_SET(iod->in_code_set, (char *)(pp->str.addr + p_offset + 1));
						if (DEFAULT_CODE_SET != iod->in_code_set)
							ICONV_OPEN_CD(iod->input_conv_cd,
								(char *)(pp->str.addr + p_offset + 1), INSIDE_CH_SET);
#						endif
						GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
						SET_ENCODING(temp_chset, &chset_mstr);
						if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
							break;	/* ignore UTF chsets if not utf8_mode. */
						if (IS_UTF16_CHSET(temp_chset))		/* UTF16 is not valid for TTY */
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADCHSET, 2,
								 chset_mstr.len, chset_mstr.addr);
						iod->ichset = temp_chset;
						break;
					}
				case iop_opchset:
					{
#						ifdef KEEP_zOS_EBCDIC
						if ((iconv_t)0 != iod->output_conv_cd)
						{
							ICONV_CLOSE_CD(iod->output_conv_cd);
						}
						SET_CODE_SET(iod->out_code_set, (char *)(pp->str.addr + p_offset + 1));
						if (DEFAULT_CODE_SET != iod->out_code_set)
							ICONV_OPEN_CD(iod->output_conv_cd, INSIDE_CH_SET,
								(char *)(pp->str.addr + p_offset + 1));
#						endif
						GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
						SET_ENCODING(temp_chset, &chset_mstr);
						if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
							break;	/* ignore UTF chsets if not utf8_mode. */
						if (IS_UTF16_CHSET(temp_chset))		/* UTF16 is not valid for TTY */
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADCHSET, 2,
								 chset_mstr.len, chset_mstr.addr);
						iod->ochset = temp_chset;
						break;
					}
				case iop_chset:
					{
						GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
						SET_ENCODING(temp_chset, &chset_mstr);
						if (!gtm_utf8_mode && IS_UTF_CHSET(temp_chset))
							break;	/* ignore UTF chsets if not utf8_mode. */
						if (IS_UTF16_CHSET(temp_chset))		/* UTF16 is not valid for TTY */
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADCHSET, 2,
								 chset_mstr.len, chset_mstr.addr);
						iod->ichset = iod->ochset = temp_chset;
						break;
					}
			}
			p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
				(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
		}
		temp_ptr = (d_tt_struct *)d_in->dev_sp;
		Tcsetattr(tt_ptr->fildes, TCSANOW, &t, status, save_errno);
		if (0 != status)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TCSETATTR, 1, tt_ptr->fildes, save_errno);
		if (tt == d_in->type)
		{
			temp_ptr->term_ctrl = mask_in;
			/* reset the mask to default if chset was changed without specifying new terminators or Default */
			if ((!terminator_specified && (old_ichset != iod->ichset)) ||
				(terminator_specified && temp_ptr->default_mask_term))
			{
				memset(&mask_term.mask[0], 0, SIZEOF(io_termmask));
				if (CHSET_M != iod->ichset)
				{
					mask_term.mask[0] = TERM_MSK_UTF8_0;
					mask_term.mask[4] = TERM_MSK_UTF8_4;
				} else
					mask_term.mask[0] = TERM_MSK;
				temp_ptr->default_mask_term = TRUE;
			}
			memcpy(&temp_ptr->mask_term, &mask_term, SIZEOF(io_termmask));
		}
		if (flush_input)
		{
			TCFLUSH(tt_ptr->fildes, TCIFLUSH, status);
			if (0 != status)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LIT_AND_LEN("tcflush input"),
					CALLFROM, errno);
		}
	} else if (tt_ptr->mupintr && !dollar_zininterrupt)
	{	/* The interrupted read was not properly resumed so clear it now */
		tt_ptr->mupintr = FALSE;
		tt_ptr->tt_state_save.who_saved = ttwhichinvalid;
		io_find_mvstent(iod, TRUE);	/* clear mv stack entry */
	}
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}

