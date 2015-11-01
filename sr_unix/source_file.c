/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_time.h"
#include "gtm_stat.h"

#include "compiler.h"
#include "parse_file.h"
#include "error.h"
#include "io.h"
#include "io_params.h"
#include "eintr_wrappers.h"
#include "op.h"
#include "source_file.h"
#include "zroutines.h"


GBLDEF unsigned short	source_name_len;
GBLDEF unsigned char	source_file_name[MAX_FBUFF + 1];
GBLDEF char		rev_time_buf[20];

GBLDEF unsigned char	routine_name[sizeof(mident) + 1];
GBLDEF unsigned char	module_name[sizeof(mident) + 1];
GBLREF unsigned char	*source_buffer;
GBLREF int4		dollar_zcstatus;
GBLREF io_pair          io_curr_device;

static bool	tt_so_do_once;
static io_pair	compile_src_dev;
static io_pair	dev_in_use;	/*	before opening source file	*/
static io_pair	tmp_list_dev;	/*	before reading source file	*/
				/*	it equal to dev_in_use if list file not open	*/

error_def(ERR_SRCFILERR);

void	compile_source_file(unsigned short flen, char *faddr)
{
	plength		plen;
	mval		fstr, ret;
	int		i;
	unsigned char	*p;
	error_def	(ERR_FILEPARSE);
	error_def	(ERR_FILENOTFND);
	error_def	(ERR_ERRORSUMMARY);

	if (MAX_FBUFF < flen)
	{
		dec_err(VARLSTCNT(4) ERR_FILEPARSE, 2, flen, faddr);
		dollar_zcstatus = ERR_ERRORSUMMARY;
	} else
	{
		fstr.mvtype = MV_STR;
		fstr.str.addr = faddr;
		fstr.str.len = flen;
		ESTABLISH(source_ch);
		tt_so_do_once = FALSE;
		for (i = 0 ;  ; i++)
		{
			plen.p.pint = op_fnzsearch(&fstr, 0, &ret);
			if (!ret.str.len)
			{
				if (!i)
				{
					dec_err(VARLSTCNT(4) ERR_FILENOTFND, 2, fstr.str.len, fstr.str.addr);
					dollar_zcstatus = ERR_ERRORSUMMARY;
				}
				break;
			}
			assert(ret.mvtype == MV_STR);
			assert(ret.str.len <= MAX_FBUFF);
			source_name_len = ret.str.len;
			memcpy(source_file_name, ret.str.addr, source_name_len);
			source_file_name[source_name_len] = 0;
			p = &source_file_name[plen.p.pblk.b_dir];
			if ((plen.p.pblk.b_dir >= sizeof("/dev/") - 1) && !MEMCMP_LIT(source_file_name, "/dev/"))
				tt_so_do_once = TRUE;
			else if (plen.p.pblk.b_ext != 2
				 || ('M' != p[plen.p.pblk.b_name + 1]  &&  'm' != p[plen.p.pblk.b_name + 1]))
			{
				dec_err(VARLSTCNT(4) ERR_FILEPARSE, 2, source_name_len, source_file_name);
				dollar_zcstatus = ERR_ERRORSUMMARY;
				continue;
			}

			if (compiler_startup())
				dollar_zcstatus = ERR_ERRORSUMMARY;

			if (tt_so_do_once)
				break;
		}
		REVERT;
	}
}


CONDITION_HANDLER(source_ch)
{
	int	dummy1, dummy2;
	error_def(ERR_ASSERT);
	error_def(ERR_ERRORSUMMARY);
	error_def(ERR_GTMASSERT);
	error_def(ERR_GTMCHECK);
	error_def(ERR_STACKOFLOW);

	START_CH;
	if (DUMP)
	{
		NEXTCH;
	}
	zsrch_clr(0);
	dollar_zcstatus = ERR_ERRORSUMMARY;
	UNWIND(dummy1, dummy2);
}


bool	open_source_file (void)
{

	static readonly unsigned char open_params_list[5] =
        {
		(unsigned char)iop_readonly,
		(unsigned char)iop_recordsize, (unsigned char)0x7F, (unsigned char)0x07F,
                (unsigned char)iop_eol
        };

	mstr		fstr;
	int		status, n,index;
	parse_blk	pblk;
	char		*p, buff[MAX_FBUFF + 1];
	time_t		clock;
	struct stat	statbuf;
	mval		val;
	mval		pars;
	error_def	(ERR_FILEPARSE);

	memset(&pblk, 0, sizeof(pblk));
	pblk.buffer = buff;
	pblk.buff_size = MAX_FBUFF;
	pblk.fop = F_SYNTAXO;
	fstr.addr = (char *)source_file_name;
	fstr.len = source_name_len;
	status = parse_file(&fstr, &pblk);
	if (!(status & 1))
		rts_error(VARLSTCNT(5) ERR_FILEPARSE, 2, fstr.len, fstr.addr, status);

	pars.mvtype = MV_STR;
	pars.str.len = sizeof(open_params_list);
	pars.str.addr = (char *)open_params_list;
	val.mvtype = MV_STR;
	val.str.len = source_name_len;
	val.str.addr = (char *)source_file_name;
	op_open(&val, &pars, 0, 0);
	dev_in_use = io_curr_device;	/*	save list file info in use if it is opened	*/
	op_use(&val, &pars);
	compile_src_dev = io_curr_device;

	if (tt_so_do_once)
	{
		clock = time(0);
		memcpy(&routine_name[0], "MDEFAULT", sizeof("MDEFAULT") - 1);
		memcpy(&module_name[0], "MDEFAULT", sizeof("MDEFAULT") - 1);
	} else
	{
		STAT_FILE((char *)&source_file_name[0], &statbuf, status);
		assert(status == 0);
		clock = statbuf.st_mtime;
		p = pblk.l_name;
		n = pblk.b_name;
		if (n > sizeof(mident))
			n = sizeof(mident);
		index = 0;
		if ('_' == *p)
		{
			routine_name[index] = '%';
			module_name[index] = '_';
			p++; index++;
		}
		for (; index < n; index++)
			routine_name[index] = module_name[index] = *p++;
		for (; index < (sizeof(mident)); index++ )
			routine_name[index] = module_name[index] = 0;
	}
	p = (char *)GTM_CTIME(&clock);
	memcpy (rev_time_buf, p + 4, sizeof(rev_time_buf));

	io_curr_device = dev_in_use;	/*	set it back to make open_list_file save the device	*/
	return TRUE;
}


/*
 *  Return:
 *	length of line read
 */

int4	read_source_file (void)
{
	unsigned char	*cp;
	mval		val;
	void		read_source_ch();

	errno = 0;
	tmp_list_dev = io_curr_device;
	io_curr_device = compile_src_dev;
	ESTABLISH_RET(read_source_ch, -1);
	op_readfl(&val, MAX_SRCLINE, 0);
	REVERT;
	memcpy((char *)source_buffer, val.str.addr, val.str.len);
	cp = source_buffer + val.str.len;
	*cp = '\0';
	/*	if there is a newline charactor (end of a line)	*/
	if (!(io_curr_device.in->dollar.x))
		*(cp+1) = '\0';
	if ( FALSE != io_curr_device.in->dollar.zeof )
		return -1;
	io_curr_device = tmp_list_dev;	/*	restore list file after reading	if it's opened	*/
	return cp - source_buffer;	/*	var.str.len	*/

}

CONDITION_HANDLER(read_source_ch)
{
	int	dummy1, dummy2;
	START_CH;
	UNWIND(dummy1, dummy2);
}

void	close_source_file (void)
{
	mval		val;
	mval		pars;
	unsigned char	no_param;

	no_param = (unsigned char)iop_eol;
	pars.mvtype = MV_STR;
	pars.str.len = sizeof(no_param);
	pars.str.addr = (char *)&no_param;
	val.mvtype = MV_STR;
	val.str.len = source_name_len;
	val.str.addr = (char *)source_file_name;
	op_close(&val, &pars);
	io_curr_device = tmp_list_dev;	/*	not dev_in_use to make sure list file works	*/
	return;
}
