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

#include <descrip.h>
#include <jpidef.h>
#include <prvdef.h>
#include <ssdef.h>
#include "gtm_inet.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "ccp.h"
#include "ladef.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "filestruct.h"
#include "jnl.h"
#include "gdscc.h"
#include "locks.h"
#include "gdskill.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "getjobnum.h"
#include "lmdef.h"
#include "dfntmpmbx.h"
#include "init_secshr_addrs.h"

typedef struct{
int4	link;
int4	*exit_hand;
int4	arg_cnt;
int4	*cond_val;
} desblk;

GBLDEF int4		ccp_exi_condition;
GBLDEF desblk		ccp_exi_blk;
GBLDEF unsigned short	ccp_channel;
GBLDEF ccp_db_header	*ccp_reg_root = 0;

GBLREF bool		checkdb_timer;
GBLREF bool		ccp_dump_on;
GBLREF bool		licensed;
GBLREF int4		lkid, lid;

LITREF int4		ccp_prd_len;
LITREF char		ccp_prd_name[];
LITREF char		ccp_ver_name[];
LITREF int4		ccp_ver_len;
OS_PAGE_SIZE_DECLARE

static unsigned char	old_name_str[15];
static uint4		name_status;
static $DESCRIPTOR(old_proc_name, old_name_str);

void
ccp_init(void)
{
	uint4			status, grp_num, prvadr[2], prvprv[2];
	mstr			lnm$system = {10, "LNM$SYSTEM"};
	unsigned short		out_len;
	$DESCRIPTOR(proc_name, CCP_PRC_NAME);
	$DESCRIPTOR(desc, "GTM$CLSTLK");
	$DESCRIPTOR(lognam, CCP_MBX_NAME);
	error_def(ERR_CCPGRP);
	error_def(ERR_CCPNAME);
	error_def(ERR_CCPMBX);
	error_def(ERR_WILLEXPIRE);
	error_def(LP_NOCNFDB);
	error_def(LP_INVCSM);
	int4	lic_status;			/* license status	*/
	int4	inid = 0;			/* initial node number	*/
	int4	mdl = 0;			/* hw. nodel		*/
	int4	nid = 0;			/* node number		*/
	int4	days = 0;			/* days to expiration	*/
	int4	lic_x;				/* license value	*/
	char	*h = NULL;			/* license data base	*/
	char	*pak = NULL;			/* license pak		*/
	struct dsc$descriptor_s dprd;
	struct dsc$descriptor_s dver;
	int4	thirty_sec[2] = { -300000000 , -1};

        ccp_dump_on = 1;
	dprd.dsc$w_length = ccp_prd_len;
	dprd.dsc$b_dtype = DSC$K_DTYPE_T;
	dprd.dsc$b_class = DSC$K_CLASS_S;
	dprd.dsc$a_pointer = ccp_prd_name;
	dver.dsc$w_length = ccp_ver_len;
	dver.dsc$b_dtype = DSC$K_DTYPE_T;
	dver.dsc$b_class = DSC$K_CLASS_S;
	dver.dsc$a_pointer = ccp_ver_name;
	licensed = TRUE;
	lic_status = SS$_NORMAL;
	lkid = 2;
#ifndef NOLICENSE
	lic_status = ((NULL == (h = la_getdb(LMDB))) ? LP_NOCNFDB : SS$_NORMAL);
#endif
#ifndef DEBUG
	status = lib$getjpi(&JPI$_GRP, 0, 0, &grp_num, 0, 0);
	if (0 == (status & 1))
		lib$signal(ERR_CCPGRP, 0, status);
	if (1 != grp_num)
		lib$signal(ERR_CCPGRP);
#endif
	name_status = lib$getjpi(&JPI$_PRCNAM, 0, 0, 0, &old_proc_name, &out_len);
	old_proc_name.dsc$w_length = out_len;
	status = sys$setprn(&proc_name);
	if (0 == (status & 1))
		lib$signal(ERR_CCPNAME, 0, status);
#ifndef NOLICENSE
	lic_status = ((1 == (lic_status & 1)) ? lm_mdl_nid(&mdl, &nid, &inid) : lic_status);
#endif
	getjobnum();
	INVOKE_INIT_SECSHR_ADDRS;
	prvadr[1] = 0;
	prvadr[0] = PRV$M_SYSLCK | PRV$M_SYSNAM | PRV$M_OPER;
	status = sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv[0]);
	if (0 == (status & 1))
		lib$signal(status);
#ifdef	NOLICENSE
	lid = 1;
	lic_x = 32767;
#else
	lic_status = ((1 == (lic_status & 1)) ? lp_licensed(h, &dprd, &dver, mdl, nid, &lid, &lic_x, &days, pak) : lic_status);
#endif
	dfntmpmbx(lnm$system.len, lnm$system.addr);
	status = sys$crembx(0, &ccp_channel, 0, 0, 0, 0, &lognam);
	if (0 == (status & 1))
		lib$signal(ERR_CCPMBX, 0, status);
	ccp_act_init();
#ifndef NOLICENSE
	if (LP_NOCNFDB != lic_status)
		la_freedb(h);
	if (1 == (lic_status & 1))
	{
		licensed = TRUE;
		if (days < 14)
			rts_error(VARLSTCNT(1) ERR_WILLEXPIRE);
	} else
	{
		licensed = FALSE;
		/* if (LP_INVCSM != lic_status) */
			rts_error(VARLSTCNT(1) lic_status);
	}
#endif
	ccp_mbx_start();
	ccp_exi_blk.exit_hand = &ccp_rundown;
	ccp_exi_blk.arg_cnt = 1;
	ccp_exi_blk.cond_val = &ccp_exi_condition;
	sys$dclexh(&ccp_exi_blk);
	sys$setimr(0, &thirty_sec[0], ccp_tr_checkdb, 0, 0);
	checkdb_timer = TRUE;
	return;
}

void
ccp_exit(void)
{
	error_def(ERR_OPRCCPSTOP);

	if (name_status & 1)
		sys$setprn(&old_proc_name);
	lib$signal(ERR_OPRCCPSTOP);
}
