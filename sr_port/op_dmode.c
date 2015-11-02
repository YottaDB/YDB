/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#ifdef UNIX
#include "iottdef.h"
#endif
#include "iotimer.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "op.h"
#include "change_reg.h"
#include "dm_read.h"
#include "tp_change_reg.h"
#include "getzposition.h"
#include "have_crit.h"

#define	DIRECTMODESTR	"Entering DIRECT MODE"

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
	mval		prompt, dummy, *input_line, zpos;
	tp_region	*tr;
	static io_pair	save_device;
	static bool	dmode_intruptd;
#ifdef UNIX
	static int	loop_cnt = 0;
	static int	old_errno = 0;
	d_tt_struct	*tt_ptr;
#endif
	static bool	kill = FALSE;

	error_def(ERR_NOTPRINCIO);
	error_def(ERR_NOPRINCIO);
	error_def(ERR_TPNOTACID);

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
	*((int4 **)&restart_ctxt) = (int4 *)GTM_CONTEXT(call_dm);
#ifdef UNIX
	loop_cnt = 0;
	old_errno = 0;
	if (tt == io_curr_device.in->type)
		tt_ptr = (d_tt_struct *)io_curr_device.in->dev_sp;
	else
		tt_ptr = NULL;
	if (!tt_ptr || !tt_ptr->mupintr)
#endif
		op_wteol(1);

	if (0 < dollar_tlevel)
	{	/* Note the existence of similar code in op_zsystem.c and mdb_condition_handler.c.
		 * Any changes here should be reflected there too. We don't have a macro for this because
		 * 	(a) This code is considered pretty much stable.
		 * 	(b) Making it a macro makes it less readable.
		 */
		save_reg = gv_cur_region;
		for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)
		{
			assert(tr->reg->open);
			if (!tr->reg->open)
				continue;
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
			getzposition(&zpos);
			gtm_putmsg(VARLSTCNT(6) ERR_TPNOTACID, 4, LEN_AND_LIT(DIRECTMODESTR), zpos.str.len, zpos.str.addr);
			send_msg(VARLSTCNT(6) ERR_TPNOTACID, 4, LEN_AND_LIT(DIRECTMODESTR), zpos.str.len, zpos.str.addr);
		}
	} else if (have_crit(CRIT_HAVE_ANY_REG))
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
