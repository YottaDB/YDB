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

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "gtmio.h"
#include "io.h"
#include "io_params.h"
#include "op.h"

error_def(ERR_JNLEXTR);

GBLREF  char            *mur_extract_buff;
GBLREF	mur_opt_struct	mur_options;
GBLREF io_pair          io_curr_device;
static io_pair		dev_in_use;
static mval		op_val, op_pars;
static readonly unsigned char open_params_list[7] =
{
	(unsigned char)iop_noreadonly,
	(unsigned char)iop_nowrap,
	(unsigned char)iop_stream,
	(unsigned char)iop_recordsize, (unsigned char)0x07F,(unsigned char)0x07F,
	(unsigned char)iop_eol
};


void	jnlext_write(char *buffer, int length)
{
	int status;
	fi_type	*fi = (fi_type *)mur_options.extr_file_info;
	fi_type *lfi = (fi_type *)mur_options.losttrans_file_info;
	fi_type *bfi = (fi_type *)mur_options.brktrans_file_info;

	buffer[length - 1] = '\n';
	dev_in_use = io_curr_device;
	if (fi != NULL)
	{
		op_val.mvtype = MV_STR;
		op_val.str.addr = (char *)fi->fn;
		op_val.str.len = fi->fn_len;
		op_pars.mvtype = MV_STR;
		op_pars.str.len = sizeof(open_params_list);
		op_pars.str.addr = (char *)open_params_list;
		op_use(&op_val, &op_pars);
		op_val.mvtype = MV_STR;
		op_val.str.addr = (char *)buffer;
		op_val.str.len = length;
		op_write(&op_val);
	}
	if (lfi != NULL)
	{
		op_val.mvtype = MV_STR;
		op_val.str.addr = (char *)lfi->fn;
		op_val.str.len = lfi->fn_len;
		op_pars.mvtype = MV_STR;
		op_pars.str.len = sizeof(open_params_list);
		op_pars.str.addr = (char *)open_params_list;
		op_use(&op_val, &op_pars);
		op_val.mvtype = MV_STR;
		op_val.str.addr = (char *)buffer;
		op_val.str.len = length;
		op_write(&op_val);
	}
	if (bfi != NULL)
	{
		op_val.mvtype = MV_STR;
		op_val.str.addr = (char *)bfi->fn;
		op_val.str.len = bfi->fn_len;
		op_pars.mvtype = MV_STR;
		op_pars.str.len = sizeof(open_params_list);
		op_pars.str.addr = (char *)open_params_list;
		op_use(&op_val, &op_pars);
		op_val.mvtype = MV_STR;
		op_val.str.addr = (char *)buffer;
		op_val.str.len = length;
		op_write(&op_val);
	}

	io_curr_device = dev_in_use;
}

void	jnlext1_write(ctl_list *ctl)
{
	char		ext_buff[100];
	int 		status;

	SPRINTF(ext_buff, "0x%08x [0x%04x] :: ", ctl->rab->dskaddr, ctl->rab->reclen);
	strcpy(mur_extract_buff, ext_buff);
}
