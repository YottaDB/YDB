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

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "buddy_list.h"
#include "jnl.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "io.h"
#include "io_params.h"
#include "op.h"

GBLREF io_pair          io_curr_device;
static readonly unsigned char open_params_list[] =
{
	(unsigned char)iop_noreadonly,
	(unsigned char)iop_nowrap,
	(unsigned char)iop_stream,
	(unsigned char)iop_eol
};


void	jnlext_write(fi_type *file_info, char *buffer, int length)
{
	int status;
	io_pair		dev_in_use;
	mval		op_val, op_pars;

	buffer[length - 1] = '\n';
	dev_in_use = io_curr_device;
	op_val.mvtype = MV_STR;
	op_val.str.addr = (char *)file_info->fn;
	op_val.str.len = file_info->fn_len;
	op_pars.mvtype = MV_STR;
	op_pars.str.len = SIZEOF(open_params_list);
	op_pars.str.addr = (char *)open_params_list;
	op_use(&op_val, &op_pars);
	op_val.mvtype = MV_STR;
	op_val.str.addr = (char *)buffer;
	op_val.str.len = length;
	op_write(&op_val);
	io_curr_device = dev_in_use;
}

