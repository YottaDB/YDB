/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_unistd.h"

#include "compiler.h"
#include "parse_file.h"
#include "error.h"
#include "io.h"
#include "io_params.h"
#include "eintr_wrappers.h"
#include "op.h"
#include "source_file.h"
#include "zroutines.h"
#include "gtmio.h"
#include "iotimer.h"
#include "cmd_qlf.h"
#include "min_max.h"
#include "cli.h"
#include "have_crit.h"
#include "util.h"
#include "op_fnzsearch.h"

GBLREF char			object_file_name[], rev_time_buf[];
GBLREF command_qualifier	cmd_qlf;
GBLREF int			object_file_des;
GBLREF int4			dollar_zcstatus;
GBLREF io_pair			io_curr_device, io_std_device;
GBLREF mident			routine_name, module_name, int_module_name;
GBLREF short			object_name_len;
GBLREF stack_frame		*frame_pointer;
GBLREF uint4			dollar_tlevel;
GBLREF unsigned char		source_file_name[];
GBLREF unsigned short		source_name_len;
GBLREF short int		source_line;

LITREF	mval		literal_null;
LITREF	mval		literal_notimeout;
LITREF	mval		literal_zero;

static bool	tt_so_do_once;
static io_pair	compile_src_dev;
static io_pair	dev_in_use;	/*	before opening source file	*/
static io_pair	tmp_list_dev;	/*	before reading source file	*/
				/*	it equal to dev_in_use if list file not open	*/
static readonly unsigned char open_params_list[] =
{
	(unsigned char)iop_readonly,
	(unsigned char)iop_m,
	(unsigned char)iop_eol
};

error_def(ERR_ASSERT);
error_def(ERR_ERRORSUMMARY);
error_def(ERR_FILENOTFND);
error_def(ERR_FILEPARSE);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OBJFILERR);
error_def(ERR_SRCFILERR);
error_def(ERR_STACKOFLOW);
error_def(ERR_LSINSERTED);

void	compile_source_file(unsigned short flen, char *faddr, boolean_t MFtIsReqd)
{
	plength		plen;
	mval		fstr, ret;
	int		i, rc;
	unsigned char	*p;
	boolean_t	wildcarded, dm_action;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (MAX_FBUFF < flen)
	{
		dec_err(VARLSTCNT(4) ERR_FILEPARSE, 2, flen, faddr);
		TREF(dollar_zcstatus) = ERR_ERRORSUMMARY;
	} else
	{
		object_file_des = FD_INVALID;
		fstr.mvtype = MV_STR;
		fstr.str.addr = faddr;
		fstr.str.len = flen;
		ESTABLISH(source_ch);
		tt_so_do_once = FALSE;
		zsrch_clr(STRM_COMP_SRC);	/* Clear any existing search cache */
		for (i = 0; ; i++)
		{
			plen.p.pint = op_fnzsearch(&fstr, STRM_COMP_SRC, 0, &ret);
			if (!ret.str.len)
			{
				if (!i)
				{
					dec_err(VARLSTCNT(4) ERR_FILENOTFND, 2, fstr.str.len, fstr.str.addr);
					TREF(dollar_zcstatus) = ERR_ERRORSUMMARY;
				}
				break;
			}
			assert(ret.mvtype == MV_STR);
			assert(ret.str.len <= MAX_FBUFF);
			source_name_len = ret.str.len;
			memcpy(source_file_name, ret.str.addr, source_name_len);
			source_file_name[source_name_len] = 0;
			p = &source_file_name[plen.p.pblk.b_dir];
			if ((plen.p.pblk.b_dir >= SIZEOF("/dev/") - 1) && !MEMCMP_LIT(source_file_name, "/dev/"))
				tt_so_do_once = TRUE;
			else if (MFtIsReqd && (plen.p.pblk.b_ext != 2 || ('M' != p[plen.p.pblk.b_name + 1]
									  &&  'm' != p[plen.p.pblk.b_name + 1])))
			{	/* M filetype is required but not present */
				dec_err(VARLSTCNT(4) ERR_FILEPARSE, 2, source_name_len, source_file_name);
				TREF(dollar_zcstatus) = ERR_ERRORSUMMARY;
				continue;
			}
			if (compiler_startup())
				TREF(dollar_zcstatus) = ERR_ERRORSUMMARY;
			if (FD_INVALID != object_file_des)
			{
				CLOSEFILE_RESET(object_file_des, rc);	/* resets "object_file_des" to FD_INVALID */
				if (-1 == rc)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_OBJFILERR, 2,
							object_name_len, object_file_name, errno);
			}
			if (tt_so_do_once)
				break;
		}
		REVERT;
	}
}

CONDITION_HANDLER(source_ch)
{
	int	dummy1, dummy2;

	START_CH(TRUE);
	if (DUMP)
	{
		NEXTCH;
	}
	zsrch_clr(0);
	TREF(dollar_zcstatus) = ERR_ERRORSUMMARY;
	UNWIND(dummy1, dummy2);
}


bool	open_source_file (void)
{
	mstr		fstr;
	int		status, n;
	parse_blk	pblk;
	char		*p, buff[MAX_FBUFF + 1];
	time_t		clock;
	struct stat	statbuf;
	mval		val;
	mval		pars;
	unsigned short	clen;

	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = buff;
	pblk.buff_size = MAX_FBUFF;
	pblk.fop = F_SYNTAXO;
	fstr.addr = (char *)source_file_name;
	fstr.len = source_name_len;
	status = parse_file(&fstr, &pblk);
	if (!(status & 1))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, fstr.len, fstr.addr, status);
	pars.mvtype = MV_STR;
	pars.str.len = SIZEOF(open_params_list);
	pars.str.addr = (char *)open_params_list;
	val.mvtype = MV_STR;
	val.str.len = source_name_len;
	val.str.addr = (char *)source_file_name;
	op_open(&val, &pars, (mval *)&literal_zero, 0);
	dev_in_use = io_curr_device;	/*	save list file info in use if it is opened	*/
	op_use(&val, &pars);
	compile_src_dev = io_curr_device;
	if (tt_so_do_once)
	{
		clock = time(0);
		p = "MDEFAULT";
		n = STR_LIT_LEN("MDEFAULT");
	} else
	{
		STAT_FILE((char *)source_file_name, &statbuf, status);
		assert(status == 0);
		clock = statbuf.st_mtime;
		p = pblk.l_name;
		n = pblk.b_name;
		if (n > MAX_MIDENT_LEN)
			n = MAX_MIDENT_LEN;
	}
	/* routine_name is the internal name of the routine (with '%' translated to '_') which can be
	 * different from module_name if the NAMEOFRTN parm is used (by trigger compilation code).
	 * module_name is the external file name of the module (file.m, file.o).
	 * int_module_name is the external symbol that gets exposed (in the GTM context) and is normally
	 * the same as module_name except when NAMEOFRTN is specified in which case it takes on the
	 * untranslated value of routine_name.
	 */
	memcpy(module_name.addr, p, n);
	module_name.len = n;
	if (!(cmd_qlf.qlf & CQ_NAMEOFRTN))
	{
		memcpy(routine_name.addr, p, n);
		routine_name.len = n;
	} else
	{	/* Routine name specified */
		clen = MAX_MIDENT_LEN;
		cli_get_str("NAMEOFRTN", routine_name.addr, &clen);
		routine_name.len = MIN(clen, MAX_MIDENT_LEN);
		cmd_qlf.qlf &= ~CQ_NAMEOFRTN;	/* Can only be used for first module in list */
	}
	memcpy(int_module_name.addr, routine_name.addr, routine_name.len);
	int_module_name.len = routine_name.len;
	if ('_' == *routine_name.addr)
		routine_name.addr[0] = '%';
	GTM_CTIME(p, &clock);
	memcpy(rev_time_buf, p + 4, REV_TIME_BUFF_LEN);
	io_curr_device = dev_in_use;	/*	set it back to make open_list_file save the device	*/
	return TRUE;
}


/*
 *  Return:
 *	length of line read
 */
int4	read_source_file (void)
{
	static char	extra_ch;
	unsigned char	*cp;
	int4		read_len;
	mval		val;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	errno = 0;
	tmp_list_dev = io_curr_device;
	io_curr_device = compile_src_dev;
	ESTABLISH_RET(read_source_ch, -1);
	read_len = MAX_SRCLINE + (extra_ch ? 0 : 1);			/* read up to 1 extra character in case of line > 8k */
	op_readfl(&val, read_len, (mval *)(dollar_tlevel ? &literal_zero : &literal_notimeout));
	REVERT;
	if (extra_ch)
		*((TREF(source_buffer)).addr++) = extra_ch;		/* start with the overflow character from the last readfl */
	memcpy((TREF(source_buffer)).addr, val.str.addr, val.str.len);
	if (extra_ch)
	{	/* we had an overflow character from the last readfl */
		extra_ch = '\0';
		(TREF(source_buffer)).addr--;
		val.str.len++;
	}
	cp = (unsigned char *)((TREF(source_buffer)).addr + val.str.len);
	(TREF(source_buffer)).len = val.str.len;
	if (MAX_SRCLINE < val.str.len)
	{	/* Emit a warning */
		extra_ch = *(--cp);					/* save the overflow character */
		dec_err(VARLSTCNT(4) ERR_LSINSERTED, 3, source_line, source_name_len, source_file_name);
		if (1 < source_line)
			source_line--;
	} else
		(TREF(source_buffer)).len++;
	*cp = '\n';							/* insert \n needed in checksum calculation */
	*(++cp) = '\0';							/* UNIX string terminator */
	if (FALSE != io_curr_device.in->dollar.zeof)
		return -1;
	io_curr_device = tmp_list_dev;					/* restore list file after reading in case it's opened */
	return (int4)((TREF(source_buffer)).len);
}

CONDITION_HANDLER(read_source_ch)
{
	int	dummy1, dummy2;

	START_CH(TRUE);
	UNWIND(dummy1, dummy2);
}

void	close_source_file (void)
{
	mval		val;
	mval		pars;
	unsigned char	no_param;

	no_param = (unsigned char)iop_eol;
	pars.mvtype = MV_STR;
	pars.str.len = SIZEOF(no_param);
	pars.str.addr = (char *)&no_param;
	val.mvtype = MV_STR;
	val.str.len = source_name_len;
	val.str.addr = (char *)source_file_name;
	op_close(&val, &pars);
	io_curr_device = tmp_list_dev;	/*	not dev_in_use to make sure list file works	*/
	return;
}
