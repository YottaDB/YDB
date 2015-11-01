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

#include "gtm_fcntl.h"
#include "gtm_string.h"
#include <unistd.h>
#include <errno.h>

#include "gtm_stdio.h"
#include "io.h"
#include "iormdef.h"
#include "iosp.h"
#include "copy.h"
#include "error.h"
#include "gtmio.h"
#include "io_params.h"
#include "gtm_stat.h"
#include "op.h"
#include "mu_load_input.h"

#define BUFF_SIZE	65536

static char		*buff1_ptr;
static char		*buff1_end;
static char		buff1[BUFF_SIZE];
static int		load_fn_len;
static char		*load_fn_ptr;
GBLREF io_pair          io_curr_device;
GBLREF int		(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);


void mu_load_init(char *fn, short fn_len)
{
	mval            val;
	mval            pars;
	static readonly unsigned char open_params_list[3] =
	{
		(unsigned char)iop_readonly,
		(unsigned char)iop_rewind,
		(unsigned char)iop_eol
	};

	error_def(ERR_LOADFILERR);

	ESTABLISH(mupip_load_ch);
	pars.mvtype = MV_STR;
	pars.str.len = sizeof(open_params_list);
	pars.str.addr = (char *)open_params_list;
	val.mvtype = MV_STR;
	val.str.len = fn_len;
	val.str.addr = (char *)fn;
	(*op_open_ptr)(&val, &pars, 0, 0);
	op_use(&val, &pars);
	load_fn_ptr = fn;
	load_fn_len = fn_len;
	buff1_ptr = buff1;
	buff1_end = buff1;
	REVERT;
	return;
}

void mu_load_close(void)
{
	mval            val;
	mval            pars;
	unsigned char   no_param = (unsigned char)iop_eol;

	val.mvtype = pars.mvtype = MV_STR;
	val.str.addr = (char *)load_fn_ptr;
	val.str.len = load_fn_len;
	pars.str.len = sizeof(iop_eol);
	pars.str.addr = (char *)&no_param;
	op_close(&val, &pars);
}

short	mu_bin_get(char **in_ptr)
{
	short	s1s;
	int	s1;
	int	rd_len, ret_len;
	char	*ptr;

	error_def(ERR_PREMATEOF);
	error_def(ERR_LOADFILERR);

	ESTABLISH_RET(mupip_load_ch, 0);
	if (buff1_end - buff1_ptr < sizeof(short))
	{
		if ((rd_len = mu_bin_read()) <= 0)
		{
			if (buff1_end != buff1_ptr)
				rts_error(VARLSTCNT(1) ERR_PREMATEOF);
			else  if (-1 == rd_len)
				rts_error(VARLSTCNT(4) ERR_LOADFILERR, 2, load_fn_len, load_fn_ptr);
			else
			{
				REVERT;
				return 0;
			}
		}
		buff1_end += rd_len;
	}
	GET_SHORT(s1s, buff1_ptr);
	buff1_ptr += sizeof(short);
	s1 = s1s;
	if (buff1_end - buff1_ptr < s1)
	{
		/* not enough data in buffer, read additional bytes */
		rd_len = mu_bin_read();
		if (rd_len + (buff1_end - buff1_ptr) < s1)
		{
			if (-1 == rd_len)
				rts_error(VARLSTCNT(4) ERR_LOADFILERR, 2, load_fn_len, load_fn_ptr);
			rts_error(VARLSTCNT(1) ERR_PREMATEOF);
		}
		buff1_end += rd_len;
	}
	*in_ptr = buff1_ptr;
	buff1_ptr += s1;
	REVERT;
	return s1;
}

int mu_bin_read(void)
{
	int		s1, rdlen;
	io_desc		*iod;
	d_rm_struct     *d_rm;

	s1 = buff1_end - buff1_ptr;
	memmove(buff1, buff1_ptr, s1);
	buff1_end = buff1 + s1;
	buff1_ptr = buff1;
	iod = io_curr_device.in;
	d_rm = (d_rm_struct *)iod->dev_sp;
	assert(NULL != d_rm);
	DOREADRL(d_rm->fildes, buff1_end, BUFF_SIZE - s1, rdlen);
	return rdlen;
}

short	mu_load_get(char **in_ptr)
{
	int	s1;
	int	rd_len, ret_len;
	char	*ptr;

	mval            val;

	error_def(ERR_PREMATEOF);
	error_def(ERR_LOADFILERR);

	ESTABLISH_RET(mupip_load_ch, 0);
	buff1_ptr = buff1_end = buff1;
	for (;;)
	{
		op_read(&val, 0);
		if (!val.str.len && io_curr_device.in->dollar.zeof)
		{
			REVERT;
			if (io_curr_device.in->dollar.x)
				rts_error(VARLSTCNT(1) ERR_PREMATEOF);
			return -1;
		}
		memcpy(buff1_end, val.str.addr, val.str.len);
		buff1_end = buff1_end + val.str.len;
		if ( !(io_curr_device.in->dollar.x) )
		{
			*in_ptr = buff1_ptr;
			ret_len = buff1_end - buff1_ptr;
			REVERT;
			return ret_len;
		}
	}
}
