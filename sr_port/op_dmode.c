/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_stdlib.h"
#include "gtm_fcntl.h"	/* Needed for AIX's silly open to open64 translations */
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
#include "iottdef.h"
#include "iotimer.h"
#include "stack_frame.h"
#include "tpnotacid_chk_inline.h"
#include "op.h"
#include "dm_read.h"
#include "restrict.h"
#include "dm_audit_log.h"
#include "svnames.h"
#include "util.h"
#include "deferred_events_queue.h"

#define	DIRECTMODESTR	"DIRECT MODE (any TP RESTART will fail)"

GBLREF	boolean_t	hup_on, prin_dm_io, prin_in_dev_failure, prin_out_dev_failure;
GBLREF	int		exi_condition;
GBLREF	io_pair		io_curr_device, io_std_device;
GBLREF	mval		dollar_zstatus;
GBLREF	stack_frame	*frame_pointer;
GBLREF	uint4		dollar_trestart;
GBLREF	unsigned char	*restart_ctxt, *restart_pc;
GBLREF	volatile int4	outofband;

LITREF	mval		literal_notimeout;

error_def(ERR_APDLOGFAIL);
error_def(ERR_NOPRINCIO);
error_def(ERR_NOTPRINCIO);
error_def(ERR_TERMHANGUP);
error_def(ERR_TERMWRITE);

void	op_dmode(void)
{
	d_tt_struct		*tt_ptr;
	gd_region		*save_re;
	io_desc			*io_ptr;
	mval			prompt, dummy, *input_line;
	boolean_t		xec_cmd = TRUE;			/* Determines if command should be
								 * executed (for direct mode auditing purposes)
								 */
	static boolean_t	dmode_intruptd;
	static io_pair		save_device;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	dummy.mvtype = dummy.str.len = 0;
	assert((intptr_t)frame_pointer->l_symtab > (intptr_t)msp);
	assert(MVST_MVAL == ((mv_stent *)(((char *)frame_pointer->l_symtab) - mvs_size[MVST_MVAL]))->mv_st_type);
	input_line = &((mv_stent *)(((char *)frame_pointer->l_symtab) - mvs_size[MVST_MVAL]))->mv_st_cont.mvs_mval;
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
	if (sighup == outofband)
	{
		io_ptr = io_curr_device.out;
		TERMHUP_NOPRINCIO_CHECK(TRUE);					/* TRUE for WRITE */
		stop_image_no_core();
	}
	if ((prin_in_dev_failure && (TRUE == io_curr_device.in->dollar.zeof)) || prin_out_dev_failure)
		ISSUE_NOPRINCIO_IF_NEEDED((prin_out_dev_failure ? io_curr_device.out : io_curr_device.in), prin_out_dev_failure,
			FALSE);
	frame_pointer->restart_pc = CODE_ADDRESS(call_dm);
	frame_pointer->restart_ctxt = GTM_CONTEXT(call_dm);
	if (tt == io_curr_device.in->type)
		tt_ptr = (d_tt_struct *)io_curr_device.in->dev_sp;
	else
		tt_ptr = NULL;
	if (!prin_out_dev_failure && (!tt_ptr || !tt_ptr->mupintr))
		op_wteol(1);
	TPNOTACID_CHECK(DIRECTMODESTR);
	if (io_curr_device.in->type == tt)
	{
		input_line = dm_read(input_line);
		/* If direct mode auditing is enabled, this attempts to send the command to logger */
		if ((AUDIT_ENABLE_DMODE & RESTRICTED(dm_audit_enable)) && !dm_audit_log(input_line, AUDIT_SRC_DMREAD))
		{
			xec_cmd = FALSE;	/* Do not execute command because logging failed */
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_APDLOGFAIL);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_APDLOGFAIL);
		}
	} else
	{
		prompt.mvtype = MV_STR;
		prompt.str = TREF(gtmprompt);
		prin_dm_io = TRUE;
		op_write(&prompt);
		op_read(input_line, (mval *)&literal_notimeout);
		/* Check if auditing for direct mode is enabled. Also check if
		 * auditing for all READ is disabled, so we do not log twice.
		 */
		if ((AUDIT_ENABLE_DMODE & RESTRICTED(dm_audit_enable)) && !(AUDIT_ENABLE_RDMODE & RESTRICTED(dm_audit_enable))
			       	&& !dm_audit_log(input_line, AUDIT_SRC_OPREAD))
		{
			xec_cmd = FALSE;	/* Do not execute command because logging failed */
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_APDLOGFAIL);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_APDLOGFAIL);
		}
	}
	op_wteol(1);
	prin_dm_io = FALSE;
	io_curr_device = save_device;
	dmode_intruptd = FALSE;
	/* Only executes command when Direct Mode Auditing
	 * is disabled or when sending/logging is successful
	 */
	if (xec_cmd)
		op_commarg(input_line, indir_linetail);
	frame_pointer->type = 0;
}
