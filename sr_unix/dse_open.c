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

#include "gtm_string.h"
#include "gtm_fcntl.h"

#include "gtm_unistd.h"
#include <errno.h>
#include "gtm_stat.h"
#include "gtm_iconv.h"

#ifdef	__MVS__
#include "gtm_zos_io.h"
#endif
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "cli.h"
#include "dse.h"
#include "gtmio.h"
#include "io.h"
#include "iosp.h"
#include "io_params.h"
#include "stringpool.h"
#include "util.h"
#include "op.h"
#include "gtm_utf8.h"
#include "gtm_conv.h"

#ifdef	__osf__
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif

GBLREF int	(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF spdesc stringpool;
GBLREF gtm_chset_t	dse_over_chset;

#ifdef	__osf__
#pragma pointer_size (restore)
#endif

static int	patch_fd;
static char	patch_ofile[256];
static short	patch_len;
static char	ch_set_name[MAX_CHSET_NAME];

GBLREF enum dse_fmt	dse_dmp_format;

void	dse_open (void)
{
	unsigned short	cli_len;
	int4 	save_errno;

	mval		val;
	mval		open_pars, use_pars;
	mstr		chset_mstr;
	int		cnt;

	static readonly unsigned char open_params_list[] =
	{
		(unsigned char)iop_newversion,
		(unsigned char)iop_m,
		(unsigned char)iop_nowrap,
		(unsigned char)iop_eol
	};

	/*set iop_width=1MB(1*1024*1024)*/
	static readonly unsigned char use_params_list[] =
	{
		(unsigned char)iop_width,
#       ifdef BIGENDIAN
		(unsigned char)0x0, (unsigned char)0x10, (unsigned char)0x0, (unsigned char)0x0
#       else
		(unsigned char)0x0, (unsigned char)0x0, (unsigned char)0x10, (unsigned char)0x0
#       endif
	};

	if (cli_present("FILE") == CLI_PRESENT)
	{
		if (CLOSED_FMT != dse_dmp_format)
		{
			util_out_print("Error:  output file already open.",TRUE);
			util_out_print("Current output file:  !AD", TRUE, strlen(patch_ofile), &patch_ofile[0]);
			return;
		}
		cli_len = SIZEOF(patch_ofile);
		if (!cli_get_str("FILE", patch_ofile, &cli_len))
			return;
		if (0 == cli_len)
		{
			util_out_print("Error: must specify a file name.",TRUE);
			return;
		}
		patch_ofile[cli_len] = 0;
		patch_len = cli_len;

		val.mvtype = MV_STR;
		val.str.len = patch_len;
		val.str.addr = (char *)patch_ofile;
		open_pars.mvtype = MV_STR;
		open_pars.str.len = SIZEOF(open_params_list);
		open_pars.str.addr = (char *)open_params_list;
		(*op_open_ptr)(&val, &open_pars, 0, NULL);
		use_pars.mvtype = MV_STR;
		use_pars.str.len = SIZEOF(use_params_list);
		use_pars.str.addr = (char *)use_params_list;
		op_use(&val, &use_pars);

		if (CLI_PRESENT == cli_present("OCHSET"))
		{
			cli_len = SIZEOF(ch_set_name);
			if (cli_get_str("OCHSET", ch_set_name, &cli_len))
			{
				if (0 == cli_len)
				{
					util_out_print("Error: must specify a charactor set name.",TRUE);
					return;
				}
#ifdef KEEP_zOS_EBCDIC
                      		ch_set_name[cli_len] = 0;
                                ch_set_len = cli_len;
                                if ( (iconv_t)0 != dse_over_cvtcd )
                                {
                                 	ICONV_CLOSE_CD(dse_over_cvtcd);
                                }
                                ICONV_OPEN_CD(dse_over_cvtcd, INSIDE_CH_SET, ch_set_name);
#else
				chset_mstr.addr = ch_set_name;
				chset_mstr.len = cli_len;
				SET_ENCODING(dse_over_chset, &chset_mstr);
#endif
			}
		} else
#ifdef KEEP_zOS_EBCDIC
                      	if ( (iconv_t) 0 == dse_over_cvtcd )
                                ICONV_OPEN_CD(dse_over_cvtcd, INSIDE_CH_SET, OUTSIDE_CH_SET);
#else
			dse_over_chset = CHSET_M;
#endif
		dse_dmp_format = OPEN_FMT;
	} else
	{
		if (CLOSED_FMT != dse_dmp_format)
			util_out_print("Current output file:  !AD", TRUE, strlen(patch_ofile), &patch_ofile[0]);
		else
			util_out_print("No current output file.",TRUE);
	}
	return;

}

boolean_t dse_fdmp_output (void *addr, int4 len)
{
	mval		val;
	static char	*buffer = NULL;
	static int	bufsiz = 0;

	assert(len >= 0);
	if (len + 1 > bufsiz)
	{
		if (buffer)
			free(buffer);
		bufsiz = len + 1;
		buffer = (char *)malloc(bufsiz);
	}
	if (len)
	{
		memcpy(buffer, addr, len);
		buffer[len] = 0;
		val.mvtype = MV_STR;
		val.str.addr = (char *)buffer;
		val.str.len = len;
		op_write(&val);
	}
	op_wteol(1);
	return TRUE;
}

void	dse_close(void)
{
	mval		val;
	mval		pars;
	unsigned char	no_param = (unsigned char)iop_eol;

	if (CLOSED_FMT != dse_dmp_format)
	{
		util_out_print("Closing output file:  !AD",TRUE,LEN_AND_STR(patch_ofile));
		val.mvtype = pars.mvtype = MV_STR;
		val.str.addr = (char *)patch_ofile;
		val.str.len = patch_len;
		pars.str.len = SIZEOF(iop_eol);
		pars.str.addr = (char *)&no_param;
		op_close(&val, &pars);
		dse_dmp_format = CLOSED_FMT;
	} else
		util_out_print("Error:  no current output file.",TRUE);
	return;
}
