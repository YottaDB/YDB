/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
	int		status, chset_index;
	int		save_errno;
	int		p_offset;
	mstr		chset;
	boolean_t	empt = FALSE;

	ioptr = dev_name->iod;
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
		else if (gtm_utf8_mode && (iop_ipchset == ch || iop_opchset == ch))
		{
			chset.len = *(pp->str.addr + p_offset);
			chset.addr = (char *)(pp->str.addr + p_offset + 1);
			chset_index = verify_chset(&chset);
			if (CHSET_M == chset_index)
				if (iop_ipchset == ch)
					ioptr->ichset = CHSET_M;
				else
					ioptr->ochset = CHSET_M;
			else if (CHSET_UTF8 == chset_index)
				if (iop_ipchset == ch)
					ioptr->ichset = CHSET_UTF8;
				else
					ioptr->ochset = CHSET_UTF8;
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADCHSET, 2, chset.len, chset.addr);
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
		if (IS_GTM_IMAGE)
			/* Only the true runtime runs with the modified terminal settings */
			setterm(ioptr);
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
		ioptr->wrap = (0 == AUTO_RIGHT_MARGIN) ? FALSE : TRUE; /* defensive programming; till we are absolutely, positively
									* certain that there are no uses of wrap == TRUE */
		tt_ptr->tbuffp = tt_ptr->ttybuff;	/* Buffer is now empty */
		tt_ptr->discard_lf = FALSE;
		if (!io_std_device.in || io_std_device.in == ioptr->pair.in)	/* io_std_device.in not set yet in io_init */
		{	/* $PRINCIPAL */
			tt_ptr->ext_cap = gtm_principal_editing_defaults;
			ioptr->ichset = ioptr->ochset = gtm_utf8_mode ? CHSET_UTF8 : CHSET_M;	/* default */
		} else
			tt_ptr->ext_cap = 0;
		if (empt)
			tt_ptr->ext_cap |= TT_EMPTERM;
		if (tt_ptr->default_mask_term)
		{
			memset(&tt_ptr->mask_term.mask[0], 0, SIZEOF(io_termmask));
			if (CHSET_M != ioptr->ichset)
			{
				tt_ptr->mask_term.mask[0] = TERM_MSK_UTF8_0;
				tt_ptr->mask_term.mask[4] = TERM_MSK_UTF8_4;
			} else
				tt_ptr->mask_term.mask[0] = TERM_MSK;
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
	}
	return TRUE;
}
