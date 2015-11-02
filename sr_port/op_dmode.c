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
#include <rtnhdr.h>
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
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

#define	DIRECTMODESTR	"Entering DIRECT MODE - TP RESTART will fail"

GBLREF	uint4		dollar_trestart;
GBLREF	io_pair		io_curr_device;
GBLREF	io_pair		io_std_device;
GBLREF	stack_frame	*frame_pointer;
GBLREF	unsigned char	*restart_pc;
GBLREF	unsigned char	*restart_ctxt;

error_def(ERR_NOTPRINCIO);
error_def(ERR_NOPRINCIO);

void	op_dmode(void)
{
	gd_region	*save_reg;
	mval		prompt, dummy, *input_line;
	static io_pair	save_device;
	static bool	dmode_intruptd;
#	ifdef UNIX
	static int	loop_cnt = 0;
	static int	old_errno = 0;
	d_tt_struct	*tt_ptr;
#	endif
	static bool	kill = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
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
	/* The following code avoids an infinite loop on UNIX systems when there is an error in writing to the principal device
	 * resulting in a call to the condition handler and an eventual return to this location
	 */
#	ifdef UNIX
	if ((loop_cnt > 0) && (errno == old_errno))
	{	/* Tried and failed 2x to write to principal device */
		kill = TRUE;
		send_msg(VARLSTCNT(1) ERR_NOPRINCIO);
		rts_error(VARLSTCNT(1) ERR_NOPRINCIO);
	}
	++loop_cnt;
	old_errno = errno;
#	endif
	*((INTPTR_T **)&restart_pc) = (INTPTR_T *)CODE_ADDRESS(call_dm);
	*((INTPTR_T **)&restart_ctxt) = (INTPTR_T *)GTM_CONTEXT(call_dm);
#	ifdef UNIX
	loop_cnt = 0;
	old_errno = 0;
	if (tt == io_curr_device.in->type)
		tt_ptr = (d_tt_struct *)io_curr_device.in->dev_sp;
	else
		tt_ptr = NULL;
	if (!tt_ptr || !tt_ptr->mupintr)
#	endif
		op_wteol(1);
	TPNOTACID_CHECK(DIRECTMODESTR);
	if (io_curr_device.in->type == tt)
		dm_read(input_line);
	else
	{
		prompt.mvtype = MV_STR;
		prompt.str = TREF(gtmprompt);
		op_write(&prompt);
		op_read(input_line, NO_M_TIMEOUT);
	}
	op_wteol(1);
	io_curr_device = save_device;
	dmode_intruptd = FALSE;
	op_commarg(input_line, indir_linetail);
	frame_pointer->type = 0;
}
