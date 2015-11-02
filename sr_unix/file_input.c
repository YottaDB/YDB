/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
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
#include "gtm_unistd.h"
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
#include "file_input.h"
#include "iotimer.h"

#define BUFF_SIZE	65535

GBLREF int		(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF	uint4		dollar_tlevel;
GBLREF io_pair		io_curr_device;

error_def(ERR_LOADFILERR);
error_def(ERR_PREMATEOF);

static char		buff1[BUFF_SIZE];
static char		*buff1_end;
static char		*buff1_ptr;
static ssize_t		buff1_ptr_file_offset;
static char		*load_fn_ptr;
static int		load_fn_len;
static readonly unsigned char open_params_list[] =
{
	(unsigned char)iop_recordsize,		/* 64K - 2 - big enough for MAX_BLK_SZ */
#	ifdef BIGENDIAN
	(unsigned char)0, (unsigned char)0, (unsigned char)255, (unsigned char)255,
#	else
	(unsigned char)255, (unsigned char)255, (unsigned char)0, (unsigned char)0,
#	endif
	(unsigned char)iop_readonly,
	(unsigned char)iop_rewind,
	(unsigned char)iop_m,
	(unsigned char)iop_nowrap,
	(unsigned char)iop_eol
};

void file_input_init(char *fn, short fn_len)
{
	int		status;
	mval		pars, val;
	unsigned char	no_param = (unsigned char)iop_eol;

	ESTABLISH(mupip_load_ch);
	pars.mvtype = MV_STR;
	pars.str.len = SIZEOF(open_params_list);
	pars.str.addr = (char *)open_params_list;
	val.mvtype = MV_STR;
	val.str.len = fn_len;
	val.str.addr = (char *)fn;
	/* The mode will be set to M for reads */
	status = (*op_open_ptr)(&val, &pars, 0, 0);
	pars.str.len = SIZEOF(iop_eol);
	pars.str.addr = (char *)&no_param;
	op_use(&val, &pars);
	load_fn_ptr = fn;
	load_fn_len = fn_len;
	buff1_ptr = buff1;
	buff1_ptr_file_offset = 0;
	buff1_end = buff1;
	REVERT;
	return;
}

void file_input_close(void)
{
	mval            pars, val;
	unsigned char   no_param = (unsigned char)iop_eol;

	val.mvtype = pars.mvtype = MV_STR;
	val.str.addr = (char *)load_fn_ptr;
	val.str.len = load_fn_len;
	pars.str.len = SIZEOF(iop_eol);
	pars.str.addr = (char *)&no_param;
	op_close(&val, &pars);
}

int	file_input_bin_get(char **in_ptr, ssize_t *file_offset, char **buff_base)
{
	char	*ptr;
	int	rd_cnt, rd_len, s1;
	unsigned short	s1s;

	ESTABLISH_RET(mupip_load_ch, 0);
	if (SIZEOF(short) > (buff1_end - buff1_ptr))
	{
		if (0 >= (rd_len = file_input_bin_read()))	/* NOTE assignment */
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
	GET_USHORT(s1s, buff1_ptr);
	buff1_ptr += SIZEOF(short);
	s1 = s1s;
	assert(0 < s1);
	assert(BUFF_SIZE >= s1);
	if ((buff1_end - buff1_ptr) < s1)
	{
		/* not enough data in buffer, read additional bytes */
		rd_len = file_input_bin_read();
		if ((rd_len + buff1_end - buff1_ptr) < s1)
		{
			if (-1 == rd_len)
				rts_error(VARLSTCNT(4) ERR_LOADFILERR, 2, load_fn_len, load_fn_ptr);
			rts_error(VARLSTCNT(1) ERR_PREMATEOF);
		}
		buff1_end += rd_len;
	}
	*in_ptr = buff1_ptr;
	buff1_ptr += s1;
	*file_offset = buff1_ptr_file_offset;
	*buff_base = buff1;
	REVERT;
	return s1;
}

int file_input_bin_read(void)
{
	d_rm_struct     *d_rm;
	int		s1, rdlen;
	io_desc		*iod;

	s1 = (int)(buff1_end - buff1_ptr);
	memmove(buff1, buff1_ptr, s1);
	iod = io_curr_device.in;
	d_rm = (d_rm_struct *)iod->dev_sp;
	assert(NULL != d_rm);
	assert(BUFF_SIZE - s1);
	buff1_end = buff1 + s1;
	buff1_ptr_file_offset += (buff1_ptr - buff1);
	buff1_ptr = buff1;
	DOREADRL(d_rm->fildes, buff1_end, BUFF_SIZE - s1, rdlen);
#	ifdef DEBUG_FO_BIN
	PRINTF("int file_input_bin_read:\t\tread(%d, %x, %d) = %d\n", d_rm->fildes, buff1,  BUFF_SIZE, s1);
	if (BUFF_SIZE - s1 > rdlen)
		rts_error(VARLSTCNT(1) errno);
#	endif
	return rdlen;
}

int file_input_get(char **in_ptr)
{
	char	*ptr, *tmp_ptr;
	int	rd_len, s1;
	mval	val;
	static unsigned int mbuff_len = BUFF_SIZE;
	unsigned int new_mbuff_len, ret_len;
	static char *mbuff = buff1;

	ESTABLISH_RET(mupip_load_ch, 0);
	ret_len = 0;
	for (;;)
	{	/* one-time only reads if in TP to avoid TPNOTACID, otherwise use untimed reads */
		op_read(&val, dollar_tlevel ? 0: NO_M_TIMEOUT);
		rd_len = val.str.len;
		if ((0 == rd_len) && io_curr_device.in->dollar.zeof)
		{
			REVERT;
			if (io_curr_device.in->dollar.x)
				rts_error(VARLSTCNT(1) ERR_PREMATEOF);
			return -1;
		}
		if (mbuff_len < ret_len + rd_len)
		{
			new_mbuff_len = MAX(ret_len,(2 * mbuff_len));
			tmp_ptr = (char *)malloc(new_mbuff_len);
			if (NULL == tmp_ptr)
			{
				REVERT;
				return -1;
			}
			if (mbuff != buff1)
			{
				memcpy(tmp_ptr, mbuff, (ret_len));
				free (mbuff);
			}
			else
				memcpy(tmp_ptr, buff1, (ret_len));

			mbuff = tmp_ptr;
			mbuff_len = new_mbuff_len;

		}
		memcpy((unsigned char *) (mbuff + ret_len), val.str.addr, rd_len);
		ret_len += rd_len;
		if ( !(io_curr_device.in->dollar.x) )
		{
			*in_ptr = mbuff;
			REVERT;
			return ret_len;
		}
	}
}
