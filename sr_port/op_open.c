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

#include "io.h"
#include "iosp.h"
#include "io_params.h"
#include "cryptdef.h"
#include "op.h"
#include "trans_log_name.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "change_reg.h"
#include "setterm.h"
#include "getzposition.h"
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

GBLREF uint4		dollar_trestart;
GBLREF io_log_name	*io_root_log_name;
GBLREF bool		licensed;
GBLREF int4		lkid, lid;

LITREF unsigned char io_params_size[];

error_def(ERR_LOGTOOLONG);
error_def(LP_NOTACQ);				/* bad license */

#define OPENTIMESTR "OPEN time too long"

int op_open(mval *device, mval *devparms, int timeout, mval *mspace)
{
	char		buf1[MAX_TRANS_NAME_LEN];	/* buffer to hold translated name */
	io_log_name	*naml;				/* logical record for passed name */
	io_log_name	*tl;				/* logical record for translated name */
	io_log_name	*prev;				/* logical record for removal search */
	int4		stat;				/* status */
	mstr		tn;				/* translated name */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(device);
	MV_FORCE_STR(devparms);
	if (mspace)
		MV_FORCE_STR(mspace);
	if (timeout < 0)
		timeout = 0;
	else if (TREF(tpnotacidtime) < timeout)
		TPNOTACID_CHECK(OPENTIMESTR);
	assert((unsigned char)*devparms->str.addr < n_iops);
	naml = get_log_name(&device->str, INSERT);
	if (naml->iod != 0)
		tl = naml;
	else
	{
#		ifdef	NOLICENSE
		licensed= TRUE;
#		else
		CRYPT_CHKSYSTEM;
		if (!licensed || LP_CONFIRM(lid, lkid)==LP_NOTACQ)
			licensed= FALSE;
#		endif
		switch(stat = TRANS_LOG_NAME(&device->str, &tn, &buf1[0], SIZEOF(buf1), dont_sendmsg_on_log2long))
		{
		case SS_NORMAL:
			tl = get_log_name(&tn, INSERT);
			break;
		case SS_NOLOGNAM:
			tl = naml;
			break;
		default:
			for (prev = io_root_log_name, tl = prev->next;  tl != 0;  prev = tl, tl = tl->next)
			{
				if (naml == tl)
				{
					prev->next = tl->next;
					free(tl);
					break;
				}
			}
#			ifdef UNIX
			if (SS_LOG2LONG == stat)
				rts_error(VARLSTCNT(5) ERR_LOGTOOLONG, 3, device->str.len, device->str.addr, SIZEOF(buf1) - 1);
			else
#			endif
				rts_error(VARLSTCNT(1) stat);
		}
	}
	stat = io_open_try(naml, tl, devparms, timeout, mspace);
	return (stat);
}
