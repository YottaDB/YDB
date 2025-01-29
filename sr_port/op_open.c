/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2020 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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
#include "iotimer.h"
#include "iott_setterm.h"

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
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "change_reg.h"
#include "getzposition.h"
#include "mmemory.h"
#include "min_max.h"
#include "mvalconv.h"
#include "is_equ.h"		/* for MV_FORCE_NSTIMEOUT macro */
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

GBLREF uint4		dollar_trestart;
GBLREF io_log_name	*io_root_log_name;
GBLREF bool		licensed;
GBLREF int4		lkid, lid;
GBLREF io_log_name	*dollar_principal;
GBLREF mstr		dollar_zpin;			/* contains "< /" */
GBLREF mstr		dollar_zpout;			/* contains "> /" */

LITREF unsigned char io_params_size[];

error_def(ERR_LOGTOOLONG);
error_def(LP_NOTACQ);				/* bad license */
error_def(ERR_DEVOPENFAIL);
error_def(ERR_TEXT);
error_def(ERR_DEVNAMERESERVED);

#define OPENTIMESTR "OPEN"

int op_open(mval *device, mval *devparms, mval *timeout, mval *mspace)
{
	char		buf1[MAX_TRANS_NAME_LEN];	/* buffer to hold translated name */
	char		*c1;				/* used to compare $P name */
	uint8		nsec_timeout;			/* timeout converted to number of nanoseconds */
	int4		stat;				/* status */
	int		nlen;				/* len of $P name */
	io_log_name	*naml;				/* logical record for passed name */
	io_log_name	*tl;				/* logical record for translated name */
	io_log_name	*prev;				/* logical record for removal search */
	io_log_name	*tlp;				/* logical record for translated name for $principal */
	mstr		tn;				/* translated name */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(device);
	MV_FORCE_STR(devparms);
	if (mspace)
		MV_FORCE_STR(mspace);
	assert((unsigned char)*devparms->str.addr < n_iops);
	MV_FORCE_NSTIMEOUT(timeout, nsec_timeout, OPENTIMESTR);
	if (dollar_principal || io_root_log_name->iod)
	{
		/* make sure that dollar_principal is defined or iod has been defined for the root */
		/* log name before attempting to use it.  This is necessary as an attempt to open "0" done */
		/* during initialization occurs prior to io_root_log_name->iod being initialized. */
		/* if the device name is the value of $P followed by "< /" or "> /" issue an error */
		/* we have no way of knowing if this is a $P variant without checking this name */
		/* the device length has to be the length of $P + 3 for the special chars at the end */
		tlp = dollar_principal ? dollar_principal : io_root_log_name->iod->trans_name;
		nlen = tlp->len;
		assert(dollar_zpout.len == dollar_zpin.len);
		if ((nlen + dollar_zpin.len) == device->str.len)
		{
			/* passed the length test now compare the 2 pieces, the first one the length of $P and the second $ZPIN*/
			c1 = (char *)tlp->dollar_io;
			if (!memvcmp(c1, nlen, &(device->str.addr[0]), nlen))
			{
				if (!memvcmp(dollar_zpin.addr, dollar_zpin.len, &(device->str.addr[nlen]), dollar_zpin.len))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_DEVOPENFAIL, 2, device->str.len,
						device->str.addr, ERR_TEXT, 2,
						LEN_AND_LIT("The value of $P followed by \"< /\" is an invalid device name"));
				else if (!memvcmp(dollar_zpout.addr, dollar_zpout.len, &(device->str.addr[nlen]), dollar_zpout.len))
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_DEVOPENFAIL, 2, device->str.len,
						device->str.addr, ERR_TEXT, 2,
						LEN_AND_LIT("The value of $P followed by \"> /\" is an invalid device name"));
			}
		}
	}
	if (((SIZEOF(SOCKETPOOLNAME) - 1) == device->str.len) &&
			(0 == STRNCMP_LIT(device->str.addr, SOCKETPOOLNAME)))
	{
		if (!TREF(is_socketpool))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_DEVNAMERESERVED, 2, device->str.len, device->str.addr);
	}
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
		switch(stat = trans_log_name(&device->str, &tn, &buf1[0], SIZEOF(buf1), dont_sendmsg_on_log2long))
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
			if (SS_LOG2LONG == stat)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(5) ERR_LOGTOOLONG, 3, device->str.len,
					device->str.addr, SIZEOF(buf1) - 1);
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) stat);
		}
	}
	stat = io_open_try(naml, tl, devparms, nsec_timeout, mspace);
	return (stat);
}
