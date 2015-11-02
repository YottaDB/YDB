/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "op.h"
#include "trans_log_name.h"

GBLREF io_pair		io_curr_device;
GBLREF io_desc		*active_device;
GBLREF io_log_name	*dollar_principal;
GBLREF io_log_name	*io_root_log_name;	/* root of linked list	*/

void op_use(mval *v, mval *p)
{
	char		buf1[MAX_TRANS_NAME_LEN];  /* buffer to hold translated name */
	io_log_name	*nl;		/* logical record for passed name */
	io_log_name	*tl;		/* logical record for translated name */
	int4		stat;		/* status */
	mstr		tn;		/* translated name */
	error_def(ERR_IONOTOPEN);

	MV_FORCE_STR(v);
	MV_FORCE_STR(p);
	nl = get_log_name(&v->str, NO_INSERT);
	if (!nl)
	{
		stat = TRANS_LOG_NAME(&v->str, &tn, buf1, SIZEOF(buf1), do_sendmsg_on_log2long);
		if (stat != SS_NORMAL)
			rts_error(VARLSTCNT(1) ERR_IONOTOPEN);
		else
		{
			if ((tl = get_log_name(&tn, NO_INSERT)) == 0)
				rts_error(VARLSTCNT(1) ERR_IONOTOPEN);
			if (!tl->iod)
				rts_error(VARLSTCNT(1) ERR_IONOTOPEN);
			nl = get_log_name(&v->str, INSERT);
			nl->iod = tl->iod;
		}
	}
	if (nl->iod->state != dev_open)
		rts_error(VARLSTCNT(1) ERR_IONOTOPEN);

	if (dollar_principal && nl->iod == dollar_principal->iod)
	{	/* if device is a GTM_PRINCIPAL synonym */
		nl = dollar_principal;
	}
	else
	{
		/* special case U "" and U 0 to be equivalent to U $P */
		/* note: "" is always the root */
		if (nl == io_root_log_name || (nl->len == 1 && nl->dollar_io[0] == '0'))
			nl = nl->iod->trans_name;
	}

	active_device = nl->iod;
	io_curr_device = nl->iod->pair;
	io_curr_device.in->name = nl;
	(nl->iod->disp_ptr->use)(nl->iod, p);
	if (nl->iod->pair.in != nl->iod->pair.out)
		(nl->iod->pair.out->disp_ptr->use)(nl->iod->pair.out, p);
	active_device = 0;
	return;
}
