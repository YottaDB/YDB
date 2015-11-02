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

#ifdef VMS
#include <devdef.h>
#include <rms.h>
#endif

#include "io.h"
#include "iosp.h"
#include "op.h"
#include "trans_log_name.h"
#include "iormdef.h"

GBLREF io_log_name	*io_root_log_name;
GBLREF io_pair		io_curr_device;
GBLREF io_pair		io_std_device;
GBLREF io_desc		*active_device;

error_def(ERR_LOGTOOLONG);
error_def(ERR_TEXT);

void op_close(mval *v, mval *p)
{
	char		buf[MAX_TRANS_NAME_LEN];  /* buffer to hold translated name */
	io_desc		*ciod;		/*  close io descriptor */
	io_log_name	*l;
	io_log_name	*prev;
	io_log_name	*tl;		/* logical record for translated name */
	int4		stat;	        /* status */
	mstr		tn;		/* translated name */
	d_rm_struct 	*rm_ptr;

	MV_FORCE_STR(v);
	MV_FORCE_STR(p);
	stat = TRANS_LOG_NAME(&v->str, &tn, buf, SIZEOF(buf), dont_sendmsg_on_log2long);
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
	} else if ((SS_NOLOGNAM == stat) VMS_ONLY(|| (0 == v->str.len)))
	{
	        if (0 == (l = get_log_name(&v->str, NO_INSERT)))
			return;
		else if (0 == (ciod = l->iod) || TRUE == ciod->perm
			 || (dev_open != ciod->state && tcp != ciod->type))
			return;
	}
#	ifdef UNIX
	else if (SS_LOG2LONG == stat)
		rts_error(VARLSTCNT(5) ERR_LOGTOOLONG, 3, v->str.len, v->str.addr, SIZEOF(buf) - 1);
#	endif
	else
		rts_error(VARLSTCNT(1) stat);

	active_device = ciod;
	if (io_curr_device.in == ciod)
	{
		io_curr_device.in = io_std_device.in;
		/* On z/OS if is a fifo and it is read and write then need to set the current
		   device out to the std device out */
#		ifdef __MVS__
		if (ciod->type == rm)
		{
			rm_ptr = (d_rm_struct *) ciod->dev_sp;
			if (rm_ptr->fifo && (ciod->pair.out != ciod))
				io_curr_device.out = io_std_device.out;
		}
#		endif
	}
	if (io_curr_device.out == ciod)
		io_curr_device.out = io_std_device.out;

#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
	if (DEFAULT_CODE_SET != ciod->in_code_set)
		ICONV_CLOSE_CD(ciod->input_conv_cd);
	if (DEFAULT_CODE_SET != ciod->out_code_set)
		ICONV_CLOSE_CD(ciod->output_conv_cd);
#endif
	(ciod->disp_ptr->close)(ciod, p);
	active_device = 0;
}
