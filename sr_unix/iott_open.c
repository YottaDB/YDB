/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "stringpool.h"
#include "setterm.h"
#include "getcaps.h"
#include "gtm_isanlp.h"
#include "gtm_conv.h"
#include "gtmimagename.h"
#include "error.h"
#include "ydb_getenv.h"

GBLREF int		COLUMNS, GTM_LINES, AUTO_RIGHT_MARGIN;
GBLREF uint4		ydb_principal_editing_defaults;
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
	int		status, chset_index;
	int		save_errno;
	int		p_offset;
	mstr		chset_mstr;
	gtm_chset_t	temp_chset, old_ichset, old_ochset;
	boolean_t	empt = FALSE;
	boolean_t	ch_set;

	ioptr = dev_name->iod;
	ESTABLISH_RET_GTMIO_CH(&ioptr->pair, -1, ch_set);
	if (ioptr->state == dev_never_opened)
	{
		dev_name->iod->dev_sp = (void *)malloc(SIZEOF(d_tt_struct) + SIZEOF(struct termios));
		memset(dev_name->iod->dev_sp, 0, SIZEOF(d_tt_struct) + SIZEOF(struct termios));
		tt_ptr = (d_tt_struct *)dev_name->iod->dev_sp;
		tt_ptr->ttio_struct = (struct termios *)((char *)tt_ptr + SIZEOF(d_tt_struct));
		tt_ptr->in_buf_sz = TTDEF_BUF_SZ;
		tt_ptr->enbld_outofbands.x = 0;
		tt_ptr->term_ctrl &= (~TRM_NOECHO);
		tt_ptr->ttybuff = (char *)malloc(IOTT_BUFF_LEN);
		tt_ptr->default_mask_term = TRUE;
		ioptr->ichset = ioptr->ochset = gtm_utf8_mode ? CHSET_UTF8 : CHSET_M;	/* default */
	}
	tt_ptr = (d_tt_struct *)dev_name->iod->dev_sp;
	if (tt_ptr->mupintr)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZINTRECURSEIO);
	p_offset = 0;
	old_ichset = ioptr->ichset;
	old_ochset = ioptr->ochset;
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
		else if (ch == iop_nocanonical)
			tt_ptr->canonical = FALSE;
		else if (ch == iop_empterm)
			empt = TRUE;
		else if (ch == iop_noempterm)
			empt = FALSE;
		else if (iop_m == ch)
			ioptr->ichset = ioptr->ochset = CHSET_M;
		else if (gtm_utf8_mode && iop_utf8 == ch)
			ioptr->ichset = ioptr->ochset = CHSET_UTF8;
		else if (gtm_utf8_mode && (iop_ipchset == ch || iop_opchset == ch || iop_chset == ch))
		{
			GET_ADDR_AND_LEN(chset_mstr.addr, chset_mstr.len);
			SET_ENCODING(temp_chset, &chset_mstr);
			if (IS_UTF16_CHSET(temp_chset))		/* Not allowed for terminals */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADCHSET, 2, chset_mstr.len, chset_mstr.addr);
			else
			{
				switch(ch)
				{
					case iop_ipchset:
						ioptr->ichset = temp_chset;
						break;
					case iop_opchset:
						ioptr->ochset = temp_chset;
						break;
					case iop_chset:
						ioptr->ichset = temp_chset;
						ioptr->ochset = temp_chset;
						break;
				}
			}
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
		status = tcgetattr(tt_ptr->fildes, tt_ptr->ttio_struct);
		if (0 != status)
		{
			save_errno = errno;
			if (gtm_isanlp(tt_ptr->fildes) == 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TCGETATTR, 1, tt_ptr->fildes, save_errno);
		}
		status = getcaps(tt_ptr->fildes);
		if (1 != status)
		{
			if (status == 0)
			{
				env_term = ydb_getenv(YDBENVINDX_GENERIC_TERM, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH);
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
		ioptr->wrap = (0 == AUTO_RIGHT_MARGIN) ? FALSE : TRUE; /* defensive programming; till we are absolutely, positively
									* certain that there are no uses of wrap == TRUE */
		tt_ptr->tbuffp = tt_ptr->ttybuff;	/* Buffer is now empty */
		tt_ptr->discard_lf = FALSE;
		if (!io_std_device.in || io_std_device.in == ioptr->pair.in)	/* io_std_device.in not set yet in io_init */
		{	/* $PRINCIPAL */
			tt_ptr->ext_cap = ydb_principal_editing_defaults;
			ioptr->ichset = ioptr->ochset = gtm_utf8_mode ? CHSET_UTF8 : CHSET_M;	/* default */
		} else
			tt_ptr->ext_cap = 0;
		if (empt)
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
