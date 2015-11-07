/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "mmemory.h"

GBLREF io_pair		io_curr_device;
GBLREF io_desc		*active_device;
GBLREF io_log_name	*dollar_principal;
GBLREF io_log_name	*io_root_log_name;	/* root of linked list	*/
GBLREF io_pair		*io_std_device;
GBLREF mstr		dollar_prin_log;
GBLREF mstr	dollar_zpin;			/* contains "< /" */
GBLREF mstr	dollar_zpout;			/* contains "> /" */

error_def(ERR_IONOTOPEN);

void op_use(mval *v, mval *p)
{
	char		buf1[MAX_TRANS_NAME_LEN];  /* buffer to hold translated name */
	io_log_name	*nl;		/* logical record for passed name */
	io_log_name	*tl;		/* logical record for translated name */
	int4		stat;		/* status */
	mstr		tn;		/* translated name */
	int		dollar_zpselect;	/* 0 - both, 1 - input only, 2 - output only */
	char		*c1;		/* used to compare $P name */
	int		nlen;		/* len of $P name */
	io_log_name	*tlp;		/* logical record for translated name for $principal */

	MV_FORCE_STR(v);
	MV_FORCE_STR(p);

	dollar_zpselect = 0;
	if (io_std_device->in != io_std_device->out)
	{
		/* if there is a split $P then determine from the name if it is the value of "$P< /" or "$P> /"
		   if the first then it is $ZPIN so set dollar_zpselect to 1
		   if the second then it is $ZPOUT so set dollar_zpselect to 2
		   else set dollar_zpselect to 0
		   if it is $ZPIN or $ZPOUT get the log_name for $P into nl else use the mval v passed in
		*/
		tlp = dollar_principal ? dollar_principal : io_root_log_name->iod->trans_name;
		nlen = tlp->len;
		assert(dollar_zpout.len == dollar_zpin.len);
		if ((nlen + dollar_zpin.len) == v->str.len)
		{
			/* passed the length test now compare the 2 pieces, the first one the length of $P and the
			   second $ZPIN or $ZPOUT */
			c1 = (char *)tlp->dollar_io;
			if (!memvcmp(c1, nlen, &(v->str.addr[0]), nlen))
			{
				if (!memvcmp(dollar_zpin.addr, dollar_zpin.len, &(v->str.addr[nlen]), dollar_zpin.len))
					dollar_zpselect = 1;
				else if (!memvcmp(dollar_zpout.addr, dollar_zpout.len, &(v->str.addr[nlen]), dollar_zpout.len))
					dollar_zpselect = 2;
			}
		}
	}
	if (0 == dollar_zpselect)
		nl = get_log_name(&v->str, NO_INSERT);
	else
		nl = get_log_name(&dollar_prin_log, NO_INSERT);
	if (!nl)
	{
		stat = TRANS_LOG_NAME(&v->str, &tn, buf1, SIZEOF(buf1), do_sendmsg_on_log2long);
		if (stat != SS_NORMAL)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IONOTOPEN);
		else
		{
			if ((tl = get_log_name(&tn, NO_INSERT)) == 0)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IONOTOPEN);
			if (!tl->iod)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IONOTOPEN);
			nl = get_log_name(&v->str, INSERT);
			nl->iod = tl->iod;
		}
	}
	if (nl->iod->state != dev_open)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IONOTOPEN);

	if (dollar_principal && nl->iod == dollar_principal->iod)
	{	/* if device is a GTM_PRINCIPAL synonym */
		nl = dollar_principal;
	}
	else
	{
		/* special case U "" , U 0, U $ZPIN, U $ZPOUT to be equivalent to U $P */
		/* $ZPIN or $ZPOUT force nl to "0" */
		/* note: "" is always the root */
		if (nl == io_root_log_name || ((1 == nl->len) && ('0' == nl->dollar_io[0])))
			nl = nl->iod->trans_name;
	}
	active_device = nl->iod;
	io_curr_device = nl->iod->pair;
	io_curr_device.in->name = nl;
	if (nl->iod->pair.in == nl->iod->pair.out)
		(nl->iod->disp_ptr->use)(nl->iod, p);
	else
	{
		if (2 != dollar_zpselect)
			(nl->iod->disp_ptr->use)(nl->iod, p);
		if (1 != dollar_zpselect)
			(nl->iod->pair.out->disp_ptr->use)(nl->iod->pair.out, p);
	}
	active_device = 0;
	return;
}
