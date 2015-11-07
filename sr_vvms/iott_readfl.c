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
#include <ssdef.h>
#include <trmdef.h>
#include <ttdef.h>
#include <tt2def.h>

#include "efn.h"
#include "io.h"
#include "iottdef.h"
#include "iotimer.h"
#include "outofband.h"
#include "timedef.h"

#define TIMER_FLAGS 0

GBLREF io_pair		io_curr_device;
GBLREF io_pair		io_std_device;
GBLREF bool		prin_in_dev_failure;
GBLREF int4		outofband;
GBLDEF bool		ctrlu_occurred;
GBLDEF int4		spc_inp_prc;

int iott_readfl(mval *v, int4 length, int4 timeout)
{
	int		not_timed_out;
	boolean_t	timed;
	uint4		efn_mask, status, time[2];
	iosb		stat_blk;
	io_desc		*io_ptr;
	t_cap		s_mode;
	d_tt_struct	*tt_ptr;
	io_termmask	save_term_msk;

	error_def(ERR_IOEOF);

	io_ptr = io_curr_device.in;
	assert(dev_open == io_ptr->state);
	tt_ptr = (d_tt_struct *)io_ptr->dev_sp;
	if (io_curr_device.out == io_ptr)
		iott_flush(io_curr_device.out);
	memset(&tt_ptr->stat_blk, 0, SIZEOF(read_iosb));
	efn_mask = (SHFT_MSK << efn_immed_wait | SHFT_MSK << efn_outofband);
	not_timed_out = TRUE;
#ifdef DEBUG
/* this is for an assert that verifies a reliance on VMS IOSB maintenance */
		tt_ptr->read_timer = FALSE;
#endif
	tt_ptr->item_list[1].addr = timeout;
	if ((NO_M_TIMEOUT == timeout) || !timeout)
		timed = FALSE;
	else
	{
		timed = TRUE;
		time[0] = -time_low(timeout);
		time[1] = -time_high(timeout) - 1;
		efn_mask |= SHFT_MSK << efn_timer;
		status = sys$setimr(efn_timer, &time, iott_cancel_read, io_ptr ,TIMER_FLAGS);
		if (SS$_NORMAL != status)
			rts_error(VARLSTCNT(1) status);
	}
	for (;;)
	{
		status = sys$qio(efn_immed_wait ,tt_ptr->channel
					,tt_ptr->read_mask, &tt_ptr->stat_blk
					,NULL, 0
					,v->str.addr, length + ESC_LEN - 1
					,0, 0, &(tt_ptr->item_list[0]), 4 * (SIZEOF(item_list_struct)));
		if (SS$_NORMAL != status)
		{
			if ((io_curr_device.in == io_std_device.in) && (io_curr_device.out == io_std_device.out))
			{
				if (prin_in_dev_failure)
					sys$exit(status);
				else
					prin_in_dev_failure = TRUE;
			}
			break;
		}
		status = sys$wflor(efn_immed_wait, efn_mask);
		if (SS$_NORMAL != status)
			break;
		if (timed)
		{
			status = sys$cantim(io_ptr, 0);
			if (SS$_NORMAL != status)
				break;
		}
		if (!tt_ptr->stat_blk.status)
		{
			if (outofband)
			{
				status = sys$dclast(iott_cancel_read, io_ptr, 0);
				if (SS$_NORMAL != status)
					break;
				iott_resetast(io_ptr);		/* reset ast after cancel */
			}
			status = sys$synch(efn_immed_wait, &tt_ptr->stat_blk);
			if (SS$_NORMAL != status)
				break;
		}
		if (outofband)
		{
			outofband_action(FALSE);
			assert(FALSE);
		}
		if (ctrlu_occurred)
		{
			ctrlu_occurred = FALSE;
			iott_wtctrlu(tt_ptr->stat_blk.char_ct, io_ptr);
			if (timed)
			{
				/* strictly speaking a <CTRL-U> shouldn't buy you more time, but practically, it works better */
				status = sys$setimr(efn_timer, &time, iott_cancel_read, io_ptr, TIMER_FLAGS);
				if (SS$_NORMAL != status)
					break;
			}
			continue;
		}
		break;
	}
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) status);
	if (tt_ptr->stat_blk.term_length > ESC_LEN - 1)
	{
		tt_ptr->stat_blk.term_length = ESC_LEN - 1;
		/* this error may be overridden by an error in the iosb */
		io_ptr->dollar.za = 2;
	} else
		io_ptr->dollar.za = 0;
	v->str.len = tt_ptr->stat_blk.char_ct;
	memcpy(io_ptr->dollar.zb, v->str.addr + tt_ptr->stat_blk.char_ct, tt_ptr->stat_blk.term_length);
	io_ptr->dollar.zb[tt_ptr->stat_blk.term_length] = '\0';
	switch (tt_ptr->stat_blk.status)
	{
	case SS$_CONTROLC:
	case SS$_CONTROLY:
		assert(0 == tt_ptr->stat_blk.term_length);
		io_ptr->dollar.zb[0] = (SS$_CONTROLC == tt_ptr->stat_blk.status) ? CTRLC : CTRLY;
		if (((io_termmask *)tt_ptr->item_list[2].addr)->mask[0] & (SHFT_MSK << io_ptr->dollar.zb[0]))
			io_ptr->dollar.zb[1] = '\0';
		else
			io_ptr->dollar.zb[0] = '\0';
		break;
	case SS$_NORMAL:
		break;
	case SS$_ABORT:
	case SS$_CANCEL:
		assert(!tt_ptr->read_timer);
		iott_resetast(io_ptr);			/* reset ast after cancel */
	case SS$_TIMEOUT:				/* CAUTION: fallthrough */
		not_timed_out = FALSE;
		break;
	case SS$_DATACHECK:
	case SS$_DATAOVERUN:
	case SS$_PARITY:
		io_ptr->dollar.za = 1;	/* data mangled */
		break;
	case SS$_BADESCAPE:
	case SS$_PARTESCAPE:
		io_ptr->dollar.za = 2;	/* escape mangled */
		break;
	case SS$_CHANINTLK:
	case SS$_COMMHARD:
	case SS$_CTRLERR:
	case SS$_DEVACTIVE:
	case SS$_DEVALLOC:
	case SS$_DEVINACT:
	case SS$_DEVOFFLINE:
	case SS$_DEVREQERR:
	case SS$_DISCONNECT:
	case SS$_MEDOFL:
	case SS$_OPINCOMPL:
	case SS$_TOOMUCHDATA:
		io_ptr->dollar.za = 3;	/* hardware contention or failure */
		break;
	case SS$_DUPUNIT:
	case SS$_INSFMEM:
	case SS$_INSFMAPREG:
	case SS$_INCOMPAT:
		io_ptr->dollar.za = 4;	/* system configuration */
		break;
	case SS$_EXQUOTA:
	case SS$_NOPRIV:
		io_ptr->dollar.za = 5;	/* process limits */
		break;
	default:
		io_ptr->dollar.za = 9;	/* if list above is maintained, indicates an iott_ programming defect */
	}
	if (!((int)tt_ptr->item_list[0].addr & TRM$M_TM_NOECHO))
	{
		if ((io_ptr->dollar.x += v->str.len) > io_ptr->width && io_ptr->wrap)
		{
			io_ptr->dollar.y += (io_ptr->dollar.x / io_ptr->width);
			if(io_ptr->length)
				io_ptr->dollar.y %= io_ptr->length;
			io_ptr->dollar.x %= io_ptr->width;
		}
	}
	if (spc_inp_prc & (SHFT_MSK << CTRL_Z))
	{
		if ((CTRL_Z == tt_ptr->stat_blk.term_char) && !(tt_ptr->ext_cap & TT2$M_PASTHRU))
		{
			io_ptr->dollar.zeof =  TRUE;
			if (io_ptr->error_handler.len > 0)
				rts_error(VARLSTCNT(1) ERR_IOEOF);
		} else
			io_ptr->dollar.zeof = FALSE;
        }
	return not_timed_out;
}
