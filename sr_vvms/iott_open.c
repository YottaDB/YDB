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

#include <dcdef.h>
#include <descrip.h>
#include <dvidef.h>
#include <iodef.h>
#include <smgtrmptr.h>
#include <ssdef.h>
#include <trmdef.h>
#include <ttdef.h>
#include <tt2def.h>
#include <efndef.h>

#include "io.h"
#include "iotimer.h"
#include "io_params.h"
#include "iottdef.h"
#include "vmsdtype.h"
#include "stringpool.h"

GBLREF short		astq_dyn_avail;
GBLREF short		astq_dyn_alloc;
GBLREF int4		spc_inp_prc;
GBLREF io_log_name	*io_root_log_name;
GBLREF io_pair		io_std_device;

LITREF unsigned char	io_params_size[];

error_def(ERR_TERMASTQUOTA);

short iott_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, int4 timeout)
{
	bool		ast_get_static(int);  /* TODO; move to a header */
	unsigned char	buf[256], ch, sensemode[8];
	short		dummy;
	int4		bufsz, devtype, buflen;
	uint4		status;
	unsigned int	req_code;
	d_tt_struct	*tt_ptr;
	io_desc		*ioptr;
	iosb		dvisb;
	t_cap		t_mode;
	int		p_offset;
	struct
	{
		item_list_3	item[1];
		int4		terminator;
	} item_list;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ioptr = dev_name->iod;
	if (dev_never_opened == ioptr->state)
		ioptr->dev_sp = (d_tt_struct *)(malloc(SIZEOF(d_tt_struct)));
	tt_ptr = (d_tt_struct *)ioptr->dev_sp;
	if (dev_open != ioptr->state)
	{
		short channel;
		$DESCRIPTOR(file_name, "");
		if (FALSE == ast_get_static(TERMINAL_STATIC_ASTS))
			rts_error(VARLSTCNT(1) ERR_TERMASTQUOTA);
		file_name.dsc$a_pointer = dev_name->dollar_io;
		file_name.dsc$w_length = (unsigned short)dev_name->len;
		if (SS$_DEVALLOC == (status = sys$assign(&file_name, &channel, 0, 0))
				|| (SS$_INSFMEM == status) || (SS$_NOIOCHAN == status))
		{
			astq_dyn_avail += TERMINAL_STATIC_ASTS;
			astq_dyn_alloc += TERMINAL_STATIC_ASTS;
			return FALSE;
		}
		if ((SS$_NORMAL != status) && (SS$_REMOTE != status))
		{
			astq_dyn_avail += TERMINAL_STATIC_ASTS;
			rts_error(VARLSTCNT(1) status);
		}
		tt_ptr->channel = (int4)channel;
		tt_ptr->io_pending = tt_ptr->io_inuse =
			tt_ptr->io_free = tt_ptr->io_buffer = malloc(RING_BUF_SZ);
		tt_ptr->io_buftop = tt_ptr->io_buffer + RING_BUF_SZ;
		tt_ptr->sb_pending = tt_ptr->sb_free = tt_ptr->sb_buffer =
					malloc(IOSB_BUF_SZ * SIZEOF(iosb_struct));
		tt_ptr->sb_buftop = tt_ptr->sb_buffer + IOSB_BUF_SZ;
	}
	if (dev_never_opened == ioptr->state)
	{
		status = sys$qiow(EFN$C_ENF, tt_ptr->channel, IO$_SENSEMODE,
			&tt_ptr->stat_blk, 0, 0, &t_mode, 12, 0, 0, 0, 0);
		if (SS$_NORMAL == status)
			status = tt_ptr->stat_blk.status;
		if (SS$_NORMAL != status)
		{
			astq_dyn_avail += TERMINAL_STATIC_ASTS;
			rts_error(VARLSTCNT(1) status);
		}
		tt_ptr->read_mask = IO_FUNC_R;
		tt_ptr->write_mask = IO_FUNC_W;
		tt_ptr->clock_on = FALSE;
		tt_ptr->term_char = t_mode.term_char;
		tt_ptr->ext_cap = t_mode.ext_cap;
		ioptr->width = t_mode.pg_width;
		ioptr->length = t_mode.pg_length;
		tt_ptr->in_buf_sz = TTDEF_BUF_SZ;
		tt_ptr->term_chars_twisted = FALSE;
		tt_ptr->enbld_outofbands.x = 0;
		if ((spc_inp_prc & (SHFT_MSK << CTRL_U)) && (tt_ptr->term_char & TT$M_SCOPE)
			&& !(tt_ptr->ext_cap & TT2$M_PASTHRU))
			tt_ptr->ctrlu_msk = (SHFT_MSK << CTRL_U);
		else
			tt_ptr->ctrlu_msk = 0;
		if (io_std_device.in)
			/* if this is the principal device, io_std_device.in is not yet set up, therefore
			the resetast is done later in term_setup so that it can pick the correct handler */
			iott_resetast(ioptr);
		status = sys$qiow(EFN$C_ENF, tt_ptr->channel
				,IO$_SENSEMODE|IO$M_RD_MODEM
				,&tt_ptr->stat_blk, 0, 0
				,sensemode
				,0, 0, 0, 0, 0);
		/* The first time this code is called is to open the principal device
		 * and io_root_log_name->iod will be == 0,  when that is true we do
		 * not want to do the lat connect even if it is a lat device */
		if ((SS$_NORMAL == status) && (SS$_NORMAL == tt_ptr->stat_blk.status) &&
			(DT$_LAT == sensemode[0]) && (0 != io_root_log_name->iod))
		{
			status = sys$qiow(EFN$C_ENF, tt_ptr->channel,
				IO$_TTY_PORT|IO$M_LT_CONNECT,
				&tt_ptr->stat_blk, 0, 0, 0, 0, 0, 0, 0, 0);
			/* If we try to open the principal device with a statement like
			 * open "LTA66:" we will come through here and will get the
			 * illegal io function error...just ignore it */
			if (SS$_NORMAL == status)
				status = tt_ptr->stat_blk.status;
			if ((SS$_NORMAL != status) && (SS$_ILLIOFUNC != status))
			{
				astq_dyn_avail += TERMINAL_STATIC_ASTS;
				rts_error(VARLSTCNT(1) status);
			}
		}
		item_list.item[0].buffer_length		= SIZEOF(devtype);
		item_list.item[0].item_code		= DVI$_DEVTYPE;
		item_list.item[0].buffer_address	= &devtype;
		item_list.item[0].return_length_address	= &dummy;
		item_list.terminator			= 0;
		status = sys$getdviw(EFN$C_ENF, tt_ptr->channel, 0, &item_list, &dvisb, 0, 0, 0);
		if (SS$_NORMAL == status)
			status = dvisb.status;
		if (SS$_NORMAL != status)
			rts_error(VARLSTCNT(1) status);
		status = smg$init_term_table_by_type(&devtype, &tt_ptr->term_tab_entry, 0);
		if (!(status & 1))
		{
			tt_ptr->erase_to_end_line.len = 0;
			tt_ptr->key_up_arrow.len = 0;
			tt_ptr->key_down_arrow.len = 0;
			tt_ptr->clearscreen.len = 0;
		} else
		{
			bufsz = SIZEOF(buf);
			req_code = SMG$K_ERASE_TO_END_LINE;
			status = smg$get_term_data(&tt_ptr->term_tab_entry, &req_code, &bufsz, &buflen, buf, 0);
			if (status & 1)
			{
				tt_ptr->erase_to_end_line.len = buflen;
				tt_ptr->erase_to_end_line.addr = malloc(tt_ptr->erase_to_end_line.len);
				memcpy(tt_ptr->erase_to_end_line.addr, buf, buflen);
			} else
				tt_ptr->erase_to_end_line.len = 0;
			req_code = SMG$K_KEY_UP_ARROW;
			status = smg$get_term_data(&tt_ptr->term_tab_entry, &req_code, &bufsz, &buflen, buf, 0);
			if (status & 1)
			{
				tt_ptr->key_up_arrow.len = buflen;
				tt_ptr->key_up_arrow.addr = malloc(tt_ptr->key_up_arrow.len);
				memcpy(tt_ptr->key_up_arrow.addr, buf, buflen);
			} else
				tt_ptr->key_up_arrow.len = 0;
			req_code = SMG$K_KEY_DOWN_ARROW;
			status = smg$get_term_data(&tt_ptr->term_tab_entry, &req_code, &bufsz, &buflen, buf, 0);
			if (status & 1)
			{
				tt_ptr->key_down_arrow.len = buflen;
				tt_ptr->key_down_arrow.addr = malloc(tt_ptr->key_down_arrow.len);
				memcpy(tt_ptr->key_down_arrow.addr, buf, buflen);
			} else
				tt_ptr->key_down_arrow.len = 0;
			req_code = SMG$K_ERASE_TO_END_DISPLAY;
			status = smg$get_term_data(&tt_ptr->term_tab_entry, &req_code, &bufsz, &buflen, buf, 0);
			if (status & 1)
			{
				tt_ptr->clearscreen.len = buflen;
				tt_ptr->clearscreen.addr = malloc(tt_ptr->clearscreen.len);
				memcpy(tt_ptr->clearscreen.addr, buf, buflen);
			} else
				tt_ptr->clearscreen.len = 0;
		}
		tt_ptr->item_len = 3 * SIZEOF(item_list_struct);
		tt_ptr->item_list[0].buf_len = 0;
		tt_ptr->item_list[0].item_code = TRM$_MODIFIERS;
		tt_ptr->item_list[0].addr = TRM$M_TM_TRMNOECHO;
		tt_ptr->item_list[0].ret_addr = 0;
		tt_ptr->item_list[1].buf_len = 0;
		tt_ptr->item_list[1].item_code = TRM$_TIMEOUT;
		tt_ptr->item_list[1].addr = NO_M_TIMEOUT;
		tt_ptr->item_list[1].ret_addr = 0;
		tt_ptr->item_list[2].buf_len = SIZEOF(io_termmask);
		tt_ptr->item_list[2].item_code = TRM$_TERM;
		tt_ptr->item_list[2].addr = malloc(SIZEOF(io_termmask));
		memset(tt_ptr->item_list[2].addr, 0, SIZEOF(io_termmask));
		((io_termmask *)tt_ptr->item_list[2].addr)->mask[0] =
			(spc_inp_prc & (SHFT_MSK << CTRL_Z)) ? (TERM_MSK | (SHFT_MSK << CTRL_Z)) : TERM_MSK;
		tt_ptr->item_list[2].ret_addr = 0;
		tt_ptr->item_list[3].buf_len	= 0;
		tt_ptr->item_list[3].item_code	= TRM$_ESCTRMOVR;
		tt_ptr->item_list[3].addr	= ESC_LEN - 1;
		tt_ptr->item_list[3].ret_addr	= 0;
		tt_ptr->item_list[4].buf_len	= (TREF(gtmprompt)).len;
		tt_ptr->item_list[4].item_code	= TRM$_PROMPT;
		tt_ptr->item_list[4].addr	= (TREF(gtmprompt)).addr;
		tt_ptr->item_list[4].ret_addr	= 0;
		tt_ptr->item_list[5].buf_len	= 0;
		tt_ptr->item_list[5].item_code	= TRM$_INISTRNG;
		tt_ptr->item_list[5].addr	= 0;
		tt_ptr->item_list[5].ret_addr	= 0;

		if (0 != (t_mode.term_char & TT$M_WRAP))
			ioptr->wrap = TRUE;
	}
	ioptr->state = dev_open;
	p_offset = 0;
	while (iop_eol != *(pp->str.addr + p_offset))
	{
		if (iop_exception == (ch = *(pp->str.addr + p_offset++)))
		{
			ioptr->error_handler.len = *(pp->str.addr + p_offset);
			ioptr->error_handler.addr = pp->str.addr + p_offset + 1;
			s2pool(&ioptr->error_handler);
			break;
		}
		p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
			(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
	}
	return TRUE;
}
