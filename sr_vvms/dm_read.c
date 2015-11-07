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

#include <iodef.h>
#include <ssdef.h>
#include <trmdef.h>
#include <ttdef.h>
#include <tt2def.h>
#include <efndef.h>


#include "comline.h"
#include "io.h"
#include "iotimer.h"
#include "iottdef.h"
#include "stringpool.h"
#include "op.h"
#include "outofband.h"
#include "m_recall.h"
#include "dm_read.h"

GBLDEF int		comline_index;

GBLREF mstr		*comline_base;
GBLREF io_pair		io_curr_device;
GBLREF io_pair		io_std_device;
GBLREF bool		prin_in_dev_failure;
GBLREF int4		outofband;
GBLREF bool		ctrlu_occurred;
GBLREF spdesc		stringpool;
GBLREF io_desc		*active_device;

#define	CCPROMPT	0x00010000
#define CCRECALL	0x008D0000

void dm_read(mval *v)
{
	boolean_t	done;
	unsigned short	iosb[4];
	int		cl, index;
	uint4		max_width, save_modifiers, save_term_msk, status;
	read_iosb	stat_blk;
	io_desc		*io_ptr;
	t_cap		s_mode;
	d_tt_struct	*tt_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (tt == io_curr_device.out->type)
		iott_flush(io_curr_device.out);
	if (!comline_base)
	{
		comline_base = malloc(MAX_RECALL * SIZEOF(mstr));
		memset(comline_base, 0, (MAX_RECALL * SIZEOF(mstr)));
	}
	io_ptr = io_curr_device.in;
	assert(tt == io_ptr->type);
	assert(dev_open == io_ptr->state);
	if (io_ptr->dollar.zeof)
		op_halt();
	if (outofband)
	{
		outofband_action(FALSE);
		assert(FALSE);
	}
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	max_width = (io_ptr->width > tt_ptr->in_buf_sz) ? io_ptr->width : tt_ptr->in_buf_sz;
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	ENSURE_STP_FREE_SPACE(max_width);
	active_device = io_ptr;
	index = 0;
	/* the following section of code puts the terminal in "easy of use" mode */
	status = sys$qiow(EFN$C_ENF, tt_ptr->channel, IO$_SENSEMODE,
		&stat_blk, 0, 0, &s_mode, 12, 0, 0, 0, 0);
	if (SS$_NORMAL == status)
		status = stat_blk.status;
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) status);
	if ((s_mode.ext_cap & TT2$M_PASTHRU) ||
		!(s_mode.ext_cap & TT2$M_EDITING) ||
		!(s_mode.term_char & TT$M_ESCAPE) ||
		!(s_mode.term_char & TT$M_TTSYNC))
	{
		s_mode.ext_cap &= (~TT2$M_PASTHRU);
		s_mode.ext_cap |= TT2$M_EDITING;
		s_mode.term_char |= (TT$M_ESCAPE | TT$M_TTSYNC);
		status = sys$qiow(EFN$C_ENF, tt_ptr->channel,
			IO$_SETMODE, &stat_blk, 0, 0, &s_mode, 12, 0, 0, 0, 0);
		if (SS$_NORMAL == status)
			status = stat_blk.status;
		if (SS$_NORMAL != status)
			/* The following error is probably going to cause the terminal state to get mucked up */
			rts_error(VARLSTCNT(1) status);
		/* the following flag is normally used by iott_rdone, iott_readfl and iott_use but dm_read resets it when done */
		tt_ptr->term_chars_twisted = TRUE;
	}
	save_modifiers = (unsigned)tt_ptr->item_list[0].addr;
	tt_ptr->item_list[0].addr = (unsigned)tt_ptr->item_list[0].addr | TRM$M_TM_NORECALL & (~TRM$M_TM_NOECHO);
	tt_ptr->item_list[1].addr = NO_M_TIMEOUT;			/* reset key click timeout */
	save_term_msk = ((io_termmask *)tt_ptr->item_list[2].addr)->mask[0];
	((io_termmask *)tt_ptr->item_list[2].addr)->mask[0] = TERM_MSK | (SHFT_MSK << CTRL_B) | (SHFT_MSK << CTRL_Z);
	tt_ptr->item_list[4].buf_len = (TREF(gtmprompt)).len;
	do
	{
		done = TRUE;
		assert(0 <= index && index <= MAX_RECALL + 1);
		cl = clmod(comline_index - index);
		if ((0 == index) || (MAX_RECALL + 1 == index))
			tt_ptr->item_list[5].buf_len	= 0;
		else
		{
			tt_ptr->item_list[5].buf_len	= comline_base[cl].len;
			tt_ptr->item_list[5].addr	= comline_base[cl].addr;
		}
		status = sys$qiow(EFN$C_ENF, tt_ptr->channel, tt_ptr->read_mask, &stat_blk, 0, 0,
			stringpool.free, tt_ptr->in_buf_sz, 0, 0, tt_ptr->item_list, 6 * SIZEOF(item_list_struct));
		if (outofband)
			break;
		if (SS$_NORMAL != status)
		{
			if (io_curr_device.in == io_std_device.in && io_curr_device.out == io_std_device.out)
			{
				if (prin_in_dev_failure)
					sys$exit(status);
				else
					prin_in_dev_failure = TRUE;
			}
			break;
		}
		if (stat_blk.term_length > ESC_LEN - 1)
		{
			stat_blk.term_length = ESC_LEN - 1;
			if (SS$_NORMAL == stat_blk.status)
				stat_blk.status = SS$_PARTESCAPE;
		}
		if (SS$_NORMAL != stat_blk.status)
		{
			if (ctrlu_occurred)
			{
				index = 0;
				done = FALSE;
				ctrlu_occurred = FALSE;
				iott_wtctrlu(stat_blk.char_ct + (TREF(gtmprompt)).len, io_ptr);
			} else
			{
				status = stat_blk.status;
				break;
			}
		} else
		{
			if ((CTRL_B == stat_blk.term_char) ||
				(stat_blk.term_length == tt_ptr->key_up_arrow.len &&
				!memcmp(tt_ptr->key_up_arrow.addr, stringpool.free + stat_blk.char_ct,
				tt_ptr->key_up_arrow.len)))
			{
				done = FALSE;
				if ((MAX_RECALL + 1 != index) && (comline_base[cl].len || !index))
					index++;
			} else
			{
				if (stat_blk.term_length == tt_ptr->key_down_arrow.len &&
					!memcmp(tt_ptr->key_down_arrow.addr, stringpool.free + stat_blk.char_ct,
					tt_ptr->key_down_arrow.len))
				{
					done = FALSE;
					if (index)
						--index;
				}
			}
			if (!done)
			{
				status = sys$qiow(EFN$C_ENF, tt_ptr->channel,
					IO$_WRITEVBLK, &iosb, NULL, 0,
					tt_ptr->erase_to_end_line.addr, tt_ptr->erase_to_end_line.len,
					0, CCRECALL, 0, 0);
			} else
			{
				if (stat_blk.char_ct > 0
					&& (('R' == *stringpool.free) || ('r' == *stringpool.free)) &&
					(TRUE == m_recall(stat_blk.char_ct, stringpool.free, &index, tt_ptr->channel)))
				{
					assert(-1 <= index && index <= MAX_RECALL);
					done = FALSE;
					flush_pio();
					status = sys$qiow(EFN$C_ENF, tt_ptr->channel,
						IO$_WRITEVBLK, &iosb, NULL, 0,
						0, 0, 0, CCPROMPT, 0, 0);
					if ((-1 == index) || (CTRL_Z == stat_blk.term_char))
						index = 0;
				}
			}
			if (!done)
			{
				if (SS$_NORMAL == status)
					status = iosb[0];
				if (SS$_NORMAL != status)
					break;
			} else
			{
				if (CTRL_Z == stat_blk.term_char)
					io_curr_device.in->dollar.zeof = TRUE;
			}
		}
	} while (!done);
	/* put the terminal back the way the user had it set up */
	tt_ptr->item_list[0].addr = save_modifiers;
	((io_termmask *)tt_ptr->item_list[2].addr)->mask[0] = save_term_msk;
	if (tt_ptr->term_chars_twisted)
	{
		s_mode.ext_cap &= (~TT2$M_PASTHRU & ~TT2$M_EDITING);
		s_mode.ext_cap |= (tt_ptr->ext_cap & (TT2$M_PASTHRU | TT2$M_EDITING));
		s_mode.term_char &= (~TT$M_ESCAPE);
		s_mode.term_char |= (tt_ptr->term_char & TT$M_ESCAPE);
		status = sys$qiow(EFN$C_ENF, tt_ptr->channel,
			IO$_SETMODE, iosb, 0, 0, &s_mode, 12, 0, 0, 0, 0);
		if (SS$_NORMAL == status)
			status = iosb[0];
		tt_ptr->term_chars_twisted = FALSE;
	}
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) status);
	if (outofband)
	{
		/* outofband not going to help more than a error so it's checked 2nd */
		outofband_action(FALSE);
		assert(FALSE);
	}
	v->mvtype = MV_STR;
	v->str.len = stat_blk.char_ct;
	v->str.addr = stringpool.free;
	if (stat_blk.char_ct)
	{
		cl = clmod(comline_index - 1);
		if (stat_blk.char_ct != comline_base[cl].len || memcmp(comline_base[cl].addr, stringpool.free, stat_blk.char_ct))
		{
			comline_base[comline_index] = v->str;
			comline_index = clmod(comline_index + 1);
		}
		stringpool.free += stat_blk.char_ct;
	}
	assert(stringpool.free <= stringpool.top);
	if ((io_ptr->dollar.x += stat_blk.char_ct) > io_ptr->width && io_ptr->wrap)
	{
		/* dm_read doesn't maintain the other io status isv's,
		 but it does $x and $y so the user can find out where they wound up */
		io_ptr->dollar.y += io_ptr->dollar.x / io_ptr->width;
		if (io_ptr->length)
			io_ptr->dollar.y %= io_ptr->length;
		io_ptr->dollar.x %= io_ptr->width;
	}
	active_device = 0;
}
