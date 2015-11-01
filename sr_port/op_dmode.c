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

#ifdef UNIX
#include <errno.h>
#endif

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "indir_enum.h"
#include "io.h"
#include "iotimer.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "send_msg.h"
#include "op.h"
#include "change_reg.h"
#include "dm_read.h"
#include "tp_change_reg.h"

GBLREF io_pair		io_curr_device;
GBLREF io_pair		io_std_device;
GBLREF stack_frame	*frame_pointer;
GBLREF int		restart_pc;
GBLREF unsigned char	*restart_ctxt;
GBLREF mstr		gtmprompt;
GBLREF short		dollar_tlevel;
GBLREF unsigned int	t_tries;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF tp_region	*tp_reg_list;

void	op_dmode(void)
{
	gd_region	*save_reg;
	mval		prompt, dummy, *input_line;
	tp_region	*tr;
	static io_pair	save_device;
	static bool	dmode_intruptd;

#ifdef UNIX
	static int	loop_cnt = 0;
	static int	old_errno = 0;
#endif

	static bool	kill = FALSE;
	error_def(ERR_NOTPRINCIO);
	error_def(ERR_NOPRINCIO);

	dummy.mvtype = dummy.str.len = 0;
	input_line = push_mval(&dummy);
	if (dmode_intruptd)
	{
		if (io_curr_device.out != save_device.out)
		{
			dec_err(VARLSTCNT(4) ERR_NOTPRINCIO, 2, save_device.out->trans_name->len,
				save_device.out->trans_name->dollar_io);
		}
	} else
	{
		dmode_intruptd = TRUE;
		save_device = io_curr_device;
	}
	io_curr_device = io_std_device;
	if ((TRUE == io_curr_device.in->dollar.zeof) || kill)
		op_halt();

	/*****************************************************
	The following code  is meant to avoid an infinite
	loop on UNIX systems which occurs when there is an
	error in writing the the principal output device here
	that results in a call to the condition handler
	and an eventual return to this location.
	*****************************************************/

#ifdef UNIX
	if ((loop_cnt > 0) && (errno == old_errno))
	{	/* Tried and failed 2x to write to principal device */
		kill = TRUE;
		send_msg(VARLSTCNT(1) ERR_NOPRINCIO);
		rts_error(VARLSTCNT(1) ERR_NOPRINCIO);
	}
	++loop_cnt;
	old_errno = errno;
#endif

	*((int4 **)&restart_pc) = (int4 *)CODE_ADDRESS(call_dm);
	*((int4 **)&restart_ctxt) = (int4 *)CONTEXT(call_dm);
	op_wteol(1);

#ifdef UNIX
	loop_cnt = 0;
	old_errno = 0;
#endif
	if (0 < dollar_tlevel)
	{
		save_reg = gv_cur_region;
		for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)
		{
			gv_cur_region = tr->reg;
			tp_change_reg();
			if (TRUE == cs_addrs->now_crit)
				rel_crit(gv_cur_region);
		}
		gv_cur_region = save_reg;
		change_reg();
		if (CDB_STAGNATE <= t_tries)
		{
			assert(CDB_STAGNATE == t_tries);
			t_tries = CDB_STAGNATE - 1;
		}
	} else  if (cs_addrs && cs_addrs->now_crit)
		GTMASSERT;
	if (io_curr_device.in->type == tt)
		dm_read(input_line);
	else
	{
		prompt.mvtype = MV_STR;
		prompt.str = gtmprompt;
		op_write(&prompt);
		op_read(input_line, NO_M_TIMEOUT);
	}
	op_wteol(1);
	io_curr_device = save_device;
	dmode_intruptd = FALSE;
	op_commarg(input_line, indir_linetail);
	frame_pointer->type = 0;
}
