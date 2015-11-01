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
#include "io.h"
#include "iosp.h"
#include "op.h"
#include "trans_log_name.h"

GBLREF io_log_name	*io_root_log_name;
GBLREF io_pair		io_curr_device;
GBLREF io_pair		io_std_device;
GBLREF io_desc		*active_device;

void op_close(mval *v, mval *p)
{
	char		buf[MAX_TRANS_NAME_LEN];  /* buffer to hold translated name */
	io_desc		*ciod;		/*  close io descriptor */
	io_log_name	*l;
	io_log_name	*prev;
	io_log_name	*tl;		/* logical record for translated name */
	int4		stat;	        /* status */
	mstr		tn;		/* translated name */

	error_def(ERR_TEXT);
	MV_FORCE_STR(v);
	MV_FORCE_STR(p);
	stat = trans_log_name(&v->str, &tn, buf);
	if (SS_NORMAL == stat)
	{
	        if (0 == (tl = get_log_name(&tn, NO_INSERT)))
			return;
		ciod = tl->iod;
		if (0 == ciod || TRUE == ciod->perm ||
		    (ciod->state != dev_open && tcp != ciod->type))
			return;

		for (prev = io_root_log_name, l = prev->next;  l != 0;  prev = l, l = l->next)
		{
		        if (l->iod == ciod && l != tl)
			{
			        prev->next = l->next;
				free(l);
				l = prev;
			}
		}
	}
	else if (stat == SS_NOLOGNAM || 0 == v->str.len)
	{
	        if (0 == (l = get_log_name(&v->str, NO_INSERT)))
			return;
		else if (0 == (ciod = l->iod) || TRUE == ciod->perm
			 || (dev_open != ciod->state && tcp != ciod->type))
			return;
	}
	else
		rts_error(VARLSTCNT(1) stat);

	active_device = ciod;
	if (io_curr_device.in == ciod)
		io_curr_device.in = io_std_device.in;
	if (io_curr_device.out == ciod)
		io_curr_device.out = io_std_device.out;

	if (DEFAULT_CODE_SET != ciod->in_code_set)
		ICONV_CLOSE_CD(ciod->input_conv_cd);
	if (DEFAULT_CODE_SET != ciod->out_code_set)
		ICONV_CLOSE_CD(ciod->output_conv_cd);

	(ciod->disp_ptr->close)(ciod, p);
	active_device = 0;
	if (ciod->type == rm)
	        remove_rms (ciod);
}
