/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gt_timer.h"
#include "io.h"
#include "iotimer.h"
#include "io_params.h"
#include "io_dev_dispatch.h"
#include "gtm_caseconv.h"
#include "outofband.h"
#include "gtmimagename.h"

LITREF dev_dispatch_struct	io_dev_dispatch[];
LITREF unsigned char		io_params_size[];
GBLREF int4			outofband;
GBLREF int4			write_filter;
GBLREF io_desc			*active_device;

bool io_open_try(io_log_name *naml, io_log_name *tl, mval *pp, int4 timeout, mval *mspace)
{
	char		buf1[MAX_TRANS_NAME_LEN];	/* buffer to hold translated name */
	char		dev_type[MAX_DEV_TYPE_LEN];
        int             n;
	mstr		tn;				/* translated name */
	uint4		stat;				/* status */
	int		p_offset;
	unsigned char	ch;
	ABS_TIME	cur_time, end_time;
	bool		out_of_time = FALSE;

	if (0 == naml->iod)
	{
		if (0 == tl->iod)
		{
			tl->iod = (io_desc *)malloc(SIZEOF(io_desc));
			memset((char*)tl->iod, 0, SIZEOF(io_desc));
			tl->iod->pair.in  = tl->iod;
			tl->iod->pair.out = tl->iod;
			tl->iod->trans_name = tl;
			p_offset = 0;
			while (iop_eol != *(pp->str.addr + p_offset))
			{
				if ((iop_tmpmbx == (ch = *(pp->str.addr + p_offset++))) || (iop_prmmbx == ch))
					tl->iod->type = mb;
				else  if (iop_nl == ch)
					tl->iod->type = nl;
				p_offset += ((IOP_VAR_SIZE == io_params_size[ch]) ?
					(unsigned char)*(pp->str.addr + p_offset) + 1 : io_params_size[ch]);
			}
			if (!tl->iod->type && mspace && mspace->str.len)
			{
				lower_to_upper(dev_type, mspace->str.addr, mspace->str.len);
				if (((SIZEOF("TCP") - 1) == mspace->str.len)
					&& (0 == memcmp(dev_type, LIT_AND_LEN("TCP"))))
					tl->iod->type = tcp;
				else if (((SIZEOF("SOCKET") - 1) == mspace->str.len)
					&& (0 == memcmp(dev_type, LIT_AND_LEN("SOCKET"))))
					tl->iod->type = gtmsocket;
				else
					tl->iod->type = us;
			}
			if (!tl->iod->type)
			{
				tn.len = tl->len;
				tn.addr = &tl->dollar_io;
				tl->iod->type = io_type(&tn);
			}
		}
		naml->iod = tl->iod;
	}
	tl->iod->disp_ptr = &io_dev_dispatch[tl->iod->type];
	assert(0 != naml->iod);
	active_device = naml->iod;
	if (dev_never_opened == naml->iod->state)
	{
		naml->iod->wrap = DEFAULT_IOD_WRAP;
		naml->iod->width = DEFAULT_IOD_WIDTH;
		naml->iod->length = DEFAULT_IOD_LENGTH;
		naml->iod->write_filter = write_filter;
	}
	if (dev_open != naml->iod->state)
	{
		naml->iod->dollar.x = 0;
		naml->iod->dollar.y = 0;
		naml->iod->dollar.za = 0;
		naml->iod->dollar.zb[0] = 0;
		naml->iod->dollar.zeof = FALSE;
	}
	if (0 == timeout)
		stat = (naml->iod->disp_ptr->open)(naml, pp, -1, mspace, timeout);	/* ZY: add a parameter timeout */
	else  if (NO_M_TIMEOUT == timeout)
	{
		while (FALSE == (stat = (naml->iod->disp_ptr->open)(naml, pp, -1, mspace, timeout)))	/* ZY: add timeout */
		{
			hiber_start(1000);	/* 1 second */
			if (outofband)
				outofband_action(FALSE);
		}
	} else
	{
		sys_get_curr_time(&cur_time);
		add_int_to_abs_time(&cur_time, timeout * 1000, &end_time);
		while (FALSE == (stat = (naml->iod->disp_ptr->open)(naml, pp, -1, mspace, timeout))	/* ZY: add timeout */
			&& (!out_of_time))
		{
			hiber_start(1000);	/* 1 second */
			if (outofband)
				outofband_action(FALSE);
			sys_get_curr_time(&cur_time);
			if (abs_time_comp(&end_time, &cur_time) <= 0)
				out_of_time = TRUE;
		}
	}
	if (TRUE == stat)
	{
		naml->iod->state = dev_open;
		if (27 == naml->iod->trans_name->dollar_io[0])
		{
			tn.addr = &naml->iod->trans_name->dollar_io[4];
			n = naml->iod->trans_name->len - 4;
			if (n < 0)
				n = 0;
			tn.len = n;
			naml->iod->trans_name = get_log_name(&tn, INSERT);
			naml->iod->trans_name->iod = naml->iod;
		}
	}
	else
	{
		if (dev_open == naml->iod->state && (gtmsocket != naml->iod->type))
			naml->iod->state = dev_closed;
		else if ((gtmsocket == naml->iod->type) && naml->iod->newly_created)
		{
			assert(naml->iod->state != dev_open);
			iosocket_destroy(naml->iod);
		}
	}
	active_device = 0;
	if ((NO_M_TIMEOUT != timeout) && IS_MCODE_RUNNING)
		return (stat);
	return FALSE;
}
