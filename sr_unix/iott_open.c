/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
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
#include "gtm_fcntl.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_termios.h"

#include "io.h"
#include "iottdef.h"
#include "io_params.h"
#include "trmdef.h"
#include "gtmio.h"
#include "iott_setterm.h"
#include "stringpool.h"
#include "getcaps.h"
#include "gtm_isanlp.h"
#include "gtm_conv.h"
#include "gtmimagename.h"
#include "error.h"
#include "op.h"
#include "indir_enum.h"

GBLREF int		COLUMNS, GTM_LINES, AUTO_RIGHT_MARGIN;
GBLREF uint4		gtm_principal_editing_defaults;
GBLREF io_pair		io_std_device;
GBLREF	boolean_t	gtm_utf8_mode;
LITREF unsigned char	io_params_size[];

error_def(ERR_BADCHSET);
error_def(ERR_NOTERMENTRY);
error_def(ERR_NOTERMENV);
error_def(ERR_NOTERMINFODB);
error_def(ERR_TCGETATTR);
error_def(ERR_ZINTRECURSEIO);

short iott_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, int4 timeout)
{
	unsigned char	ch;
	d_tt_struct	*tt_ptr;
	io_desc		*ioptr;
	int		status, chset_index, dev_sp_size;
	int		save_errno;
	int		p_offset;
	mstr		chset_mstr;
	gtm_chset_t	temp_chset, old_ichset, old_ochset;
	boolean_t	empt = FALSE, wrap_specified = FALSE, wrap_parm;
	boolean_t	ch_set;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ioptr = dev_name->iod;
	ESTABLISH_RET_GTMIO_CH(&ioptr->pair, -1, ch_set);
	if (ioptr->state == dev_never_opened)
	{
		dev_sp_size = SIZEOF(d_tt_struct) + (TTIO_NUM_TERMIOS * SIZEOF(struct termios));
		dev_name->iod->dev_sp = (void *)malloc(dev_sp_size);
		memset(dev_name->iod->dev_sp, 0, dev_sp_size);
		tt_ptr = (d_tt_struct *)dev_name->iod->dev_sp;
		tt_ptr->ttio_struct = (struct termios *)((char *)tt_ptr + SIZEOF(d_tt_struct));
		tt_ptr->ttio_struct_start = (struct termios *)((char *)tt_ptr + SIZEOF(d_tt_struct) + SIZEOF(struct termios));
		tt_ptr->in_buf_sz = TTDEF_BUF_SZ;
		tt_ptr->enbld_outofbands.x = 0;
		tt_ptr->term_ctrl &= (~TRM_NOECHO);
		tt_ptr->ttybuff = (char *)malloc(IOTT_BUFF_LEN);
		tt_ptr->default_mask_term = TRUE;
		ioptr->ichset = ioptr->ochset = gtm_utf8_mode ? CHSET_UTF8 : CHSET_M;	/* default */
	}
	assert(tt == dev_name->iod->type);
	tt_ptr = (d_tt_struct *)dev_name->iod->dev_sp;
	if (tt_ptr->mupintr)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZINTRECURSEIO);
	p_offset = 0;
	old_ichset = ioptr->ichset;
	old_ochset = ioptr->ochset;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
		switch (ch = *(pp->str.addr + p_offset++))
		{
		case iop_exception:
			DEF_EXCEPTION(pp, p_offset, ioptr);
			break;
		case iop_canonical:
			tt_ptr->canonical = TRUE;
			break;
		case iop_nocanonical:
			tt_ptr->canonical = FALSE;
			break;
		case iop_empterm:
			empt = TRUE;
			break;
		case iop_noempterm:
			empt = FALSE;
			break;
		case iop_wrap:
			wrap_specified = TRUE;
			wrap_parm = TRUE;
			break;
		case iop_nowrap:
			wrap_specified = TRUE;
			wrap_parm = FALSE;
			break;
		case iop_m:
			ioptr->ichset = ioptr->ochset = CHSET_M;
			break;
		case iop_utf8:
			if (gtm_utf8_mode)
				ioptr->ichset = ioptr->ochset = CHSET_UTF8;
			break;
		case iop_ipchset:
		case iop_opchset:
		case iop_chset:
			if (gtm_utf8_mode)
			{
				GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
				SET_ENCODING(temp_chset, &chset_mstr);
				if (IS_UTF16_CHSET(temp_chset)) /* Not allowed for terminals */
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_BADCHSET, 2, chset_mstr.len, chset_mstr.addr);
					else if (ch == iop_ipchset)
						ioptr->ichset = temp_chset;
					else if (ch == iop_opchset)
						ioptr->ochset = temp_chset;
					else if (ch == iop_chset)
					{
						ioptr->ichset = temp_chset;
						ioptr->ochset = temp_chset;
					}
			}
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
	if (ioptr->state != dev_open)
	{
		int	status;
		char	*env_term;

		assert(fd >= 0);
		tt_ptr->fildes = fd;
		status = tcgetattr(tt_ptr->fildes, tt_ptr->ttio_struct_start);
		if (0 != status)
		{
			save_errno = errno;
			if (gtm_isanlp(tt_ptr->fildes) == 0)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_TCGETATTR, 1, tt_ptr->fildes, save_errno);
		}
		memcpy(tt_ptr->ttio_struct, tt_ptr->ttio_struct_start, SIZEOF(struct termios));
		if (IS_GTM_IMAGE)
		{ /* Only the true runtime uses the modified terminal settings */
			iott_setterm(ioptr);
			tt_ptr->ttio_modified = TRUE;
		}
		status = getcaps(tt_ptr->fildes);
		if (1 != status)
		{
			if (status == 0)
			{
				env_term = GETENV("TERM");
				if (!env_term)
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTERMENV);
					env_term = "unknown";
				}
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOTERMENTRY, 2, LEN_AND_STR(env_term));
			} else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTERMINFODB);
		}
		ioptr->width = COLUMNS;
		ioptr->length = GTM_LINES;
		if (!wrap_specified)
			ioptr->wrap = (0 == AUTO_RIGHT_MARGIN) ? FALSE : TRUE; /* defensive programming; till we are absolutely,
									* positively certain there are no uses of wrap == TRUE */
		else
			ioptr->wrap = wrap_parm;
		tt_ptr->tbuffp = tt_ptr->ttybuff;	/* Buffer is now empty */
		tt_ptr->discard_lf = FALSE;
		if (!io_std_device.in || io_std_device.in == ioptr->pair.in)	/* io_std_device.in not set yet in io_init */
		{	/* $PRINCIPAL */
			tt_ptr->ext_cap = gtm_principal_editing_defaults;
			ioptr->ichset = ioptr->ochset = gtm_utf8_mode ? CHSET_UTF8 : CHSET_M;	/* default */
		} else
			tt_ptr->ext_cap = 0;
		if (empt && (ioptr == ioptr->pair.in))
			tt_ptr->ext_cap |= TT_EMPTERM;
		/* Set terminal mask on the terminal not open, if default_term or if CHSET changes */
		if (tt_ptr->default_mask_term || (old_ichset != ioptr->ichset))
		{
			memset(&tt_ptr->mask_term.mask[0], 0, SIZEOF(io_termmask));
			if (CHSET_M != ioptr->ichset)
			{
				tt_ptr->mask_term.mask[0] = TERM_MSK_UTF8_0;
				tt_ptr->mask_term.mask[4] = TERM_MSK_UTF8_4;
			} else
				tt_ptr->mask_term.mask[0] = TERM_MSK;
			tt_ptr->default_mask_term = TRUE;
		}
		ioptr->state = dev_open;
		if ((TT_EDITING & tt_ptr->ext_cap) && !tt_ptr->recall_buff.addr)
		{
			assert(tt_ptr->in_buf_sz);
			tt_ptr->recall_buff.addr = malloc(tt_ptr->in_buf_sz);
			tt_ptr->recall_size = tt_ptr->in_buf_sz;
			tt_ptr->recall_buff.len = 0;	/* nothing in buffer */
			tt_ptr->recall_width = 0;
		}
	} else
	{
		/* Set terminal mask on the already open terminal, if CHSET changes */
		if (old_ichset != ioptr->ichset)
		{
			memset(&tt_ptr->mask_term.mask[0], 0, SIZEOF(io_termmask));
			if (CHSET_M != ioptr->ichset)
			{
				tt_ptr->mask_term.mask[0] = TERM_MSK_UTF8_0;
				tt_ptr->mask_term.mask[4] = TERM_MSK_UTF8_4;
			} else
				tt_ptr->mask_term.mask[0] = TERM_MSK;
			tt_ptr->default_mask_term = TRUE;
		}
	}
	REVERT_GTMIO_CH(&ioptr->pair, ch_set);
	return TRUE;
}
