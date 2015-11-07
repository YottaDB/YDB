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
#include <trmdef.h>
#include <ttdef.h>
#include <tt2def.h>
#include <ssdef.h>
#include <efndef.h>

#include "efn.h"
#include "io.h"
#include "iottdef.h"
#include "outofband.h"
#include "curr_dev_outbndset.h"
#include "std_dev_outbndset.h"

#define ONE_CHAR 1
#define ALL_CTRL 0x0ffffffff

GBLREF io_pair		io_curr_device;
GBLREF io_pair 		io_std_device;
GBLREF bool		prin_in_dev_failure;
GBLREF bool 		ctrlu_occurred;
GBLREF uint4		std_dev_outofband_msk;
GBLREF int4		outofband;
GBLREF int4		spc_inp_prc;

int iott_rdone( mint *v, int4 t)
{
	boolean_t	timed_out;
	unsigned char	ch[ESC_LEN];
	uint4		buff_len, char_msk, efn_mask, len, mask_in1, save_term_msk, status;
	iosb		stat_blk;
  	io_desc		*io_ptr;
	t_cap		s_mode;
	d_tt_struct	*tt_ptr;

	error_def(ERR_IOEOF);

	io_ptr = io_curr_device.in;
	assert (io_ptr->state == dev_open);
	tt_ptr = (d_tt_struct*) io_ptr->dev_sp;
	if (io_curr_device.out == io_ptr)
		iott_flush(io_curr_device.out);

	if (!(tt_ptr->ext_cap & TT2$M_PASTHRU))
	{
		/* in order to access all characters, except <CTRL-Q> and <CTRL-S>, we go into pasthru */

		status = sys$qiow(EFN$C_ENF,tt_ptr->channel,IO$_SENSEMODE,
			&stat_blk,0,0,&s_mode,12,0,0,0,0);
		if (status == SS$_NORMAL)
			status = stat_blk.status;
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
		s_mode.ext_cap |= TT2$M_PASTHRU;
		status = sys$qiow(EFN$C_ENF,tt_ptr->channel,IO$_SETMODE,
			&stat_blk,0,0,&s_mode,12,0,0,0,0);
		if (status == SS$_NORMAL)
			status = stat_blk.status;
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
		tt_ptr->term_chars_twisted = TRUE;
	}

	if (tt_ptr->term_char & TT$M_ESCAPE)
	{
		len = 4 * (SIZEOF(item_list_struct));
		buff_len = ESC_LEN;
	}else
	{
		len = 3 * (SIZEOF(item_list_struct));
		buff_len = ONE_CHAR;
	}

	efn_mask = (SHFT_MSK << efn_immed_wait | SHFT_MSK << efn_outofband);
	tt_ptr->item_list[1].addr = t;
	ch[0] = 0;
	timed_out = FALSE;

	save_term_msk = ((io_termmask *)tt_ptr->item_list[2].addr)->mask[0];
	((io_termmask *)tt_ptr->item_list[2].addr)->mask[0] = ALL_CTRL;
	/* the above prevents echoing of control characters, which preserves established behavior */

	status = sys$qio(efn_immed_wait,tt_ptr->channel
	  		  ,tt_ptr->read_mask ,&tt_ptr->stat_blk
			  ,NULL ,0
		 	  ,&ch[0] ,buff_len
			  ,0 ,0 ,&(tt_ptr->item_list[0]) ,len);
	if (status != SS$_NORMAL)
	{
		if (io_curr_device.in == io_std_device.in && io_curr_device.out == io_std_device.out)
		{
			if (prin_in_dev_failure)
			{
				((io_termmask *)tt_ptr->item_list[2].addr)->mask[0] = save_term_msk;
				sys$exit(status);
			}else
				prin_in_dev_failure = TRUE;
		}
		rts_error(VARLSTCNT(1) status);
	}

	status = sys$wflor(efn_immed_wait,efn_mask);
	if (status != SS$_NORMAL)
	{
		((io_termmask *)tt_ptr->item_list[2].addr)->mask[0] = save_term_msk;
		rts_error(VARLSTCNT(1) status);
        }

	if (!tt_ptr->stat_blk.status)
	{
		if (outofband)
		{
			status = sys$dclast(iott_cancel_read, io_ptr, 0);
			if (status != SS$_NORMAL)
			{
				((io_termmask *)tt_ptr->item_list[2].addr)->mask[0] = save_term_msk;
				rts_error(VARLSTCNT(1) status);
			}
			iott_resetast(io_ptr);			/*reset ast after cancel */
		}
		status = sys$synch(efn_immed_wait ,&tt_ptr->stat_blk);
		if (status != SS$_NORMAL)
		{
			((io_termmask *)tt_ptr->item_list[2].addr)->mask[0] = save_term_msk;
			rts_error(VARLSTCNT(1) status);
		}
	}

	assert(tt_ptr->stat_blk.status);
	/* can't put this back until the read is definately finished, which is why it's repeated above for error exits */
	((io_termmask *)tt_ptr->item_list[2].addr)->mask[0] = save_term_msk;

	if (outofband)
	{
		outofband_action(FALSE);
		assert(FALSE);
	}

	if (ctrlu_occurred)
	{
		ctrlu_occurred = FALSE;
		ch[0] = CTRL_U;
	}

	if (tt_ptr->stat_blk.term_length > ESC_LEN - 1)
	{
		tt_ptr->stat_blk.term_length = ESC_LEN - 1;
		/* this error may be overridden by an error in the iosb */
		io_ptr->dollar.za = 2;
	}else
		io_ptr->dollar.za = 0;

	switch(tt_ptr->stat_blk.status)
	{
	case SS$_CONTROLC:
	case SS$_CONTROLY:
	case SS$_NORMAL:
		break;
	case SS$_ABORT:
	case SS$_CANCEL:
		assert(FALSE);				/* not planning on this, even though it should be ok */
		iott_resetast(io_ptr);			/* reset ast after cancel */
	case SS$_TIMEOUT:				/* CAUTION: fallthrough */
		if (ch[0] == 0)				/* if the buffer holds a non-null character, accept it */
		{
			*v = -1;
			io_ptr->dollar.za = 0;
			io_ptr->dollar.zb[0] = '\0';
			timed_out = TRUE;
		}
		break;
	case SS$_DATACHECK:
	case SS$_DATAOVERUN:
	case SS$_PARITY:
		io_ptr->dollar.za = 1; /* data mangled */
		break;
	case SS$_BADESCAPE:
	case SS$_PARTESCAPE:
		io_ptr->dollar.za = 2; /* escape mangled */
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
		io_ptr->dollar.za = 3; /* hardware contention or failure */
		break;
	case SS$_DUPUNIT:
	case SS$_INSFMEM:
	case SS$_INSFMAPREG:
	case SS$_INCOMPAT:
		io_ptr->dollar.za = 4; /* system configuration */
		break;
	case SS$_EXQUOTA:
	case SS$_NOPRIV:
		io_ptr->dollar.za = 5; /* process limits */
		break;
	default:
		io_ptr->dollar.za = 9; /* if list above is maintained, indicates an iott_ programming defect */
	}

	if (tt_ptr->term_chars_twisted)
	{
		if (!(tt_ptr->ext_cap & TT2$M_PASTHRU))
			s_mode.ext_cap &= (~TT2$M_PASTHRU);
		status = sys$qiow(EFN$C_ENF,tt_ptr->channel,
			IO$_SETMODE,&stat_blk,0,0,&s_mode,12,0,0,0,0);
		if (status == SS$_NORMAL)
			status = stat_blk.status;
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
		tt_ptr->term_chars_twisted = FALSE;
	}

	if (timed_out)
		return FALSE;

	if (ch[0] < 32)
	{
		char_msk = SHFT_MSK << ch[0];
		if (!(tt_ptr->ext_cap & TT2$M_PASTHRU))
		{
			if ((tt_ptr->enbld_outofbands.mask & char_msk)
				|| ((io_ptr == io_std_device.in) && (std_dev_outofband_msk & char_msk & CTRLC_MSK)))
			/* if and when <CTRL-C> and <CTRL-Y> handling is normalized the
			CTRLC_MSK above should probably be removed; nonetheless, currently
			<CTRL-Y> just gives gt.m a seizure and restarts the current operation - why waste the character */
			{
				/* pass on the outofband that was grabbed by the special pasthru */
				status = sys$dclast(((io_ptr == io_std_device.in) ? std_dev_outbndset : curr_dev_outbndset),
					(int4*)ch[0], 0);
				if (status != SS$_NORMAL)
					rts_error(VARLSTCNT(1) status);
				outofband_action(FALSE);
				assert(FALSE);
			}

			if (spc_inp_prc & (SHFT_MSK << CTRL_Z))
			{
				if ((tt_ptr->stat_blk.term_char == CTRL_Z) && (save_term_msk & char_msk))
					io_ptr->dollar.zeof = TRUE;
				else
					io_ptr->dollar.zeof = FALSE;
			}
                }

		if (tt_ptr->stat_blk.term_length && !(save_term_msk & char_msk) &&
			((buff_len == ONE_CHAR)	/* this is a proxy for !TT$M_ESCAPE */
			|| (ch[0] != ESC)))
		{
			/* demote control character terminators unless they are really enabled by the application */
			tt_ptr->stat_blk.char_ct++;
			tt_ptr->stat_blk.term_length--;
		}
	}
	assert(tt_ptr->stat_blk.term_length < ESC_LEN);
	*v = ch[0];
	memcpy(io_ptr->dollar.zb, &ch[0], tt_ptr->stat_blk.term_length);
	io_ptr->dollar.zb[tt_ptr->stat_blk.term_length] = '\0';

	/* The three lines above used to be implemented by the
	following code:

	if (buff_len == ONE_CHAR)
	{	*v = ch[0];
		io_ptr->dollar.zb[0] = '\0';
	}
	else
	{	*v = (ch[0] == ESC) ? 0 : ch[0];
		if (tt_ptr->stat_blk.term_length > SIZEOF(io_ptr->dollar.zb) - 1)
		{	tt_ptr->stat_blk.term_length = SIZEOF(io_ptr->dollar.zb) - 1;
			io_ptr->dollar.za = 2;
		}
		memcpy(io_ptr->dollar.zb,&ch[0],tt_ptr->stat_blk.term_length);
		io_ptr->dollar.zb[tt_ptr->stat_blk.term_length] = '\0';
	}

	This caused $zb to always be null if NOESCAPE
	($ZB normally contains the terminator) and
	the value to be 0 if ESCAPE and an escape sequence arrived.
	The new behavior is to ALWAYS report the (1st) arriving
	character and ALWAYS maintain $ZB.  Note that $x is/was not
	increased if the (1st) arriving character is a terminator.
	The old code has been left in case we wish to quickly return
	to the old behavior.
	*/

	if (!((int)tt_ptr->item_list[0].addr & TRM$M_TM_NOECHO))
	{
		if ((io_ptr->dollar.x += tt_ptr->stat_blk.char_ct) > io_ptr->width && io_ptr->wrap)
		{
			io_ptr->dollar.y = ++(io_ptr->dollar.y);
			if(io_ptr->length)
				io_ptr->dollar.y %= io_ptr->length;
			io_ptr->dollar.x = 0;
		}
	}
	if (io_ptr->dollar.zeof && (io_ptr->error_handler.len > 0))
		rts_error(VARLSTCNT(1) ERR_IOEOF);
	return TRUE;
}
