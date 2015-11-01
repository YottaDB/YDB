/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "iosp.h"
#include "error.h"
#include "stp_parms.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "util.h"
#include "gt_timer.h"
#include "mupipbckup.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "change_reg.h"
#include "mupip_exit.h"
#include "mu_getlst.h"
#include "mu_outofband_setup.h"
#include "gtmmsg.h"
#include "wcs_flu.h"
#include "mupip_freeze.h"

GBLREF bool		mu_ctrly_occurred;
GBLREF bool		mu_ctrlc_occurred;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF tp_region	*halt_ptr;
GBLREF tp_region	*grlist;
GBLREF bool		in_mupip_freeze;
GBLREF bool		error_mupip;

void	mupip_freeze(void)
{
	int4		status;
	bool		freeze, record;
	tp_region	*rptr, *rptr1;
	bool		override;

	error_def(ERR_BUFFLUFAILED);
	error_def(ERR_DBRDONLY);
	error_def(ERR_FREEZECTRL);
	error_def(ERR_MUNOACTION);
	error_def(ERR_MUPCLIERR);
	error_def(ERR_MUNOFINISH);

	status = SS_NORMAL;
	in_mupip_freeze = TRUE;
	mu_outofband_setup();
	gvinit();
	freeze = (CLI_PRESENT == cli_present("ON"));
	if (CLI_PRESENT == cli_present("OFF"))
	{
		if (TRUE == freeze)
		{
			util_out_print("The /ON qualifier is invalid with the /OFF qualifier", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	}
	if (CLI_PRESENT == cli_present("RECORD"))
	{
		record = TRUE;
		if (FALSE == freeze)
		{
			util_out_print("The /RECORD qualifier is invalid with the /OFF qualifier", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	} else
		record = FALSE;
	if (CLI_PRESENT == cli_present("OVERRIDE"))
	{
		override = TRUE;
		if (freeze)
		{
			util_out_print("The /OVERRIDE qualifier is invalid with the /ON qualifier", TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
	} else
		override = FALSE;
	error_mupip = FALSE;
	mu_getlst("REG_NAME", sizeof(tp_region));
	if (error_mupip)
	{
		util_out_print("!/MUPIP cannot start freeze with above errors!/", TRUE);
		mupip_exit(ERR_MUNOACTION);
	}
	halt_ptr = grlist;
	ESTABLISH(mu_freeze_ch);
	for (rptr = grlist;  NULL != rptr;  rptr = rptr->fPtr)
	{
		if (mu_ctrly_occurred || mu_ctrlc_occurred)
			break;
		if ((dba_bg != rptr->reg->dyn.addr->acc_meth) && (dba_mm != rptr->reg->dyn.addr->acc_meth))
		{
			util_out_print("Can only FREEZE BG and MM databases", TRUE);
			continue;
		}
		if (reg_cmcheck(rptr->reg))
		{
			util_out_print("!/Can't FREEZE region !AD across network", TRUE, REG_LEN_STR(rptr->reg));
			continue;
		}
		gv_cur_region = rptr->reg;
		gvcst_init(gv_cur_region);
		if (gv_cur_region->was_open)	/* Already open under another name.  Region will not be marked open*/
		{
			gv_cur_region->open = FALSE;
			continue;
		}
		change_reg();
		assert(&FILE_INFO(gv_cur_region)->s_addrs == cs_addrs);
		/* Cannot flush for read-only data files */
		if (gv_cur_region->read_only)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, gv_cur_region->dyn.addr->fname_len,
				gv_cur_region->dyn.addr->fname);
			continue;
		}
		while (FALSE == region_freeze(gv_cur_region, freeze, override))
			hiber_start(1000);
		cs_addrs->persistent_freeze = freeze;	/* secshr_db_clnup() shouldn't clear the freeze up */
		if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH))
			rts_error(VARLSTCNT(6) ERR_BUFFLUFAILED, 4, LEN_AND_LIT("FREEZE"), DB_LEN_STR(gv_cur_region));
		halt_ptr = rptr->fPtr;
	}
	REVERT;
	if (mu_ctrly_occurred || mu_ctrlc_occurred)
	{
		for (rptr1 = grlist;  rptr1 != rptr;  rptr1 = rptr1->fPtr)
		{
			gv_cur_region = rptr1->reg;
			if (FALSE == gv_cur_region->open)
				continue;
			region_freeze(gv_cur_region, FALSE, FALSE);
		}
		gtm_putmsg(VARLSTCNT(1) ERR_FREEZECTRL);
		status = ERR_MUNOFINISH;
	} else  if (record)
	{
		for (rptr = grlist;  NULL != rptr;  rptr = rptr->fPtr)
		{
			gv_cur_region = rptr->reg;
			if (FALSE == gv_cur_region->open)
				continue;
			cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
			cs_addrs->hdr->last_rec_backup = cs_addrs->ti->curr_tn;
		}
	}
	mupip_exit(status);
}
