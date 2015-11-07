/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

#include "efn.h"
#include "io.h"
#include "iottdef.h"
#include "timedef.h"
#include "dollarx.h"

GBLDEF uint4 iott_write_delay[2] =
			{
				(~(3300000) + 1),
				0xffffffff
			};
GBLREF io_pair io_curr_device;

void iott_write(mstr *mstr_ptr)
{
	d_tt_struct	*tt_ptr;
	char		*iop, *io_in, *str;
	uint4		status;
	int		cpy_len, str_len, t_len;
	io_desc		*io_ptr;
	iosb_struct	*sb[2];

	io_ptr = io_curr_device.out;
	tt_ptr = (d_tt_struct *)io_curr_device.out->dev_sp;
	/* pending must follow free - must be able to get pair uninterrupted with movq */
	assert(&tt_ptr->sb_free + 1 == &tt_ptr->sb_pending);
	str = mstr_ptr->addr;
	str_len = mstr_ptr->len;
	while (0 != str_len)
	{
		for (;  ;)
		{
			movq(&tt_ptr->sb_free, sb);
			if (sb_dist(sb[0], sb[1]) < MIN_IOSB_SP)
				sys$hiber();
			else
				break;
		}
		for (;  ;)
		{
			iop = tt_ptr->io_pending;
			if (io_space(tt_ptr->io_free, (unsigned char *)iop) < MIN_RINGBUF_SP)
				sys$hiber();
			else
				break;
		}
		assert(tt_ptr->io_free < tt_ptr->io_buftop);
		if (tt_ptr->io_free < (iop = tt_ptr->io_pending))
			cpy_len = ((t_len = (unsigned char *)iop - tt_ptr->io_free) <= str_len ?
					(t_len > MAX_MEMCPY ? MAX_MEMCPY : t_len) :
					(str_len > MAX_MEMCPY ? MAX_MEMCPY : str_len));
		else
			cpy_len = ((t_len = tt_ptr->io_buftop - tt_ptr->io_free) <= str_len ?
					(t_len > MAX_MEMCPY ? MAX_MEMCPY : t_len) :
					(str_len > MAX_MEMCPY ? MAX_MEMCPY : str_len));
		assert(tt_ptr->io_free + cpy_len <= tt_ptr->io_buftop);
		memcpy(tt_ptr->io_free, str, cpy_len);
		str += cpy_len;
		str_len -= cpy_len;
		if ((tt_ptr->io_free += cpy_len) == tt_ptr->io_buftop)
			tt_ptr->io_free = tt_ptr->io_buffer;
		if (tt_ptr->io_free < tt_ptr->io_inuse)
		{
			if (SS$_NORMAL != (status = sys$dclast(iott_wtstart, tt_ptr, 0)))
				rts_error(VARLSTCNT(1) status);
		} else  if (tt_ptr->io_free - tt_ptr->io_inuse >= SPACE_INUSE)
		{
			if (SS$_NORMAL != (status = sys$dclast(iott_wtstart, tt_ptr, 0)))
				rts_error(VARLSTCNT(1) status);
		} else  if (FALSE == tt_ptr->clock_on)
		{
			tt_ptr->clock_on = TRUE;
			if (SS$_NORMAL != (status = sys$setimr(efn_ignore, iott_write_delay, iott_clockfini ,tt_ptr, 0)))
				rts_error(VARLSTCNT(1) status);
		}
	}
	dollarx(io_ptr, (uchar_ptr_t)mstr_ptr->addr, (uchar_ptr_t)mstr_ptr->addr + mstr_ptr->len);
	tt_ptr->write_mask &= (~IO$M_CANCTRLO);
	return;
}
