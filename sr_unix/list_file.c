/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_time.h"

#include "io.h"
#include "io_params.h"
#include "cmd_qlf.h"
#include "parse_file.h"
#include "list_file.h"
#include "op.h"
#include "have_crit.h"

#define LISTEXT ".lis"

GBLREF char		rev_time_buf[];
GBLREF unsigned char	source_file_name[];
GBLREF command_qualifier cmd_qlf;
GBLREF int		(*op_open_ptr)(mval *v, mval *p, mval *t, mval *mspace);
GBLREF io_pair		io_curr_device;
GBLREF list_params 	lst_param;
GBLREF mident		module_name;
GBLREF unsigned short	source_name_len;

LITREF char             ydb_release_name[];
LITREF int4             ydb_release_name_len;
LITREF mval		literal_zero;

static char print_time_buf[20];
static io_pair dev_in_use;
static readonly struct
{
	unsigned char	newversion;
	unsigned char	wrap;
	unsigned char	width;
	unsigned char	v_width[SIZEOF(int4)];
#	ifdef __MVS__
	unsigned char	chsetebcdic[8];
#	else
	unsigned char	chsetm[3];
#	endif
	unsigned char	eol;
} open_params_list = {
	(unsigned char)iop_newversion,	(unsigned char)iop_wrap,
	(unsigned char)iop_recordsize,
#	ifdef BIGENDIAN
	{(unsigned char)0, (unsigned char)0, (unsigned char)0, (unsigned char)132},
#	else
	{(unsigned char)132, (unsigned char)0, (unsigned char)0, (unsigned char)0},
#	endif
#	ifdef __MVS__
	{(unsigned char)iop_chset, 6, 'E', 'B', 'C', 'D', 'I', 'C'},
#	else
	{(unsigned char)iop_chset, 1, 'M'},
#	endif
	(unsigned char)iop_eol
	};

void open_list_file(void)
{
	char		charspace, fname[MAX_FN_LEN + 1], list_name[MAX_MIDENT_LEN + STR_LIT_LEN(LISTEXT)], *p;
	unsigned char	cp;
	mstr		fstr;
	mval		file, parms;
	parse_blk	pblk;
	time_t		clock;
	uint4		status;

	lst_param.list_line = 1;
	lst_param.page = 0;

	memset(&pblk, 0, SIZEOF(pblk));
	for (cp = source_name_len - 1; cp && ('/' != source_file_name[cp]); cp--)
		;	/* scan back from the end to find the source file name */
	cp += cp ? 1 : 0;
	pblk.def1_size = source_name_len - cp;
	/* Check if this name ends in .m. If so, replace the extension. Otherwise append to it. */
	int ext_len = STR_LIT_LEN(DOTM);
	if (ext_len < pblk.def1_size && !STRCMP((char*) &source_file_name[source_name_len - ext_len], DOTM))
		pblk.def1_size -= ext_len;
	/* filenames longer than the max are truncated. */
	pblk.def1_size = MIN(pblk.def1_size, MAX_MIDENT_LEN);
	memcpy(list_name, &source_file_name[cp], pblk.def1_size);
	MEMCPY_LIT(&list_name[pblk.def1_size], LISTEXT);
	pblk.def1_size += STR_LIT_LEN(LISTEXT);
	pblk.def1_buf = list_name;
	pblk.buffer = &fname[0];
	pblk.buff_size = MAX_FN_LEN;
	pblk.fop = F_SYNTAXO;
	fstr.len = (MV_DEFINED(&cmd_qlf.list_file) ? cmd_qlf.list_file.str.len : 0);
	fstr.addr = cmd_qlf.list_file.str.addr;
	if (!(status = parse_file(&fstr, &pblk)) & 1)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);

	file.mvtype = parms.mvtype = MV_STR;
	file.str.len = pblk.b_esl;
	file.str.addr = &fname[0];
	parms.str.len = SIZEOF(open_params_list);
	parms.str.addr = (char *)&open_params_list;
	(*op_open_ptr)(&file, &parms, (mval *)&literal_zero, 0);
	parms.str.len = 1;
	charspace = (char)iop_eol;
	parms.str.addr = &charspace;
	dev_in_use = io_curr_device;
	op_use(&file,&parms);
	clock = time(0);
	GTM_CTIME(p, &clock);
	memcpy (print_time_buf, p + 4, SIZEOF(print_time_buf));
	list_head(0);
	return;
}

void close_list_file(void)
{
	mval		param,list_file;
	char		charspace;
	boolean_t	in_is_curr_device, out_is_curr_device;

	param.str.len = 1;
	charspace = (char) iop_eol;
	param.str.addr = &charspace;
	list_file.mvtype = param.mvtype = MV_STR;
	list_file.str.len = io_curr_device.in->trans_name->len;
	list_file.str.addr = &io_curr_device.in->trans_name->dollar_io[0];
	SAVE_IN_OUT_IS_CURR_DEVICE(dev_in_use, in_is_curr_device, out_is_curr_device);
	op_close(&list_file, &param);
	RESTORE_IO_CURR_DEVICE(dev_in_use, in_is_curr_device, out_is_curr_device);
}

void list_head(bool newpage)
{
	short col_2 = 70;
	static readonly unsigned char page_lit[] = "page ";
	unsigned char page_no_buf[10];
	mval head;

	return;
	if (newpage)
		op_wtff();

	head.mvtype = MV_STR;
	head.str.addr = (char *)&ydb_release_name[0];
	head.str.len = ydb_release_name_len;
	op_write (&head);

	op_wttab(col_2);
	head.str.addr = print_time_buf;
	head.str.len = 20;
	op_write(&head);

	op_wttab(100);
	lst_param.page++;
	head.str.addr = (char *)page_lit;
	head.str.len = SIZEOF(page_lit) - 1;
	op_write(&head);

	head.str.addr = (char *)page_no_buf;
	head.str.len = INTCAST(i2asc(page_no_buf, lst_param.page) - page_no_buf);
	op_write(&head);
	op_wteol(1);

	head.str.addr = (char *)source_file_name;
	head.str.len = source_name_len;
	op_write(&head);
	if (source_name_len >= col_2)
		op_wteol(1);
	op_wttab(col_2);
	head.str.addr = rev_time_buf;
	head.str.len = 20;
	op_write(&head);
	op_wteol(3);
}


#define BIG_PG 32
#define BIG_PG_BOT_SP 10
#define SMALL_PG_BOT_SP 3

void list_line(char *c)
{
	short n, c_len, space_avail;
	mval out;

	if (io_curr_device.out->dollar.y >= lst_param.lines_per_page -
	    ((lst_param.lines_per_page < BIG_PG) ? SMALL_PG_BOT_SP : BIG_PG_BOT_SP))
		list_head(1);

	out.mvtype = MV_STR;
	c_len = (short)strlen(c);

	while(c_len > 0)
	{
		if (c_len < (space_avail = PG_WID - io_curr_device.out->dollar.x))
			space_avail = c_len;
		out.str.len = space_avail;
		out.str.addr = c;
		op_write(&out);
		c_len -= space_avail;
		c += space_avail;
		if (c_len > 0)
		{
			assert(io_curr_device.out->dollar.x != 0);
			op_wteol(1);
		}
	}

	if ((n = lst_param.lines_per_page - io_curr_device.out->dollar.y) <
	    lst_param.space)
	{
		assert(n > 0);
		op_wteol(n);
	} else
		op_wteol(lst_param.space);
}

void list_line_number(void)
{
	unsigned char buf[8];
	int n, i, q;
	unsigned char *pt;
	mval out;

	assert(cmd_qlf.qlf & CQ_LIST);
	if (io_curr_device.out->dollar.y >= lst_param.lines_per_page -
	    ((lst_param.lines_per_page < BIG_PG) ? SMALL_PG_BOT_SP : BIG_PG_BOT_SP))
		list_head(1);

	n = lst_param.list_line++;
	pt = &buf[5];
	memset(&buf[0],SP,SIZEOF(buf));
	do
	{
		i = n / 10;
		q = n - (i * 10);
		*--pt = q  + '0';
		n = i;
	} while(i > 0);
	out.mvtype = MV_STR;
	out.str.addr = (char*)buf;
	out.str.len = SIZEOF(buf);
	op_write(&out);
}


void list_chkpage(void)
{
	if (io_curr_device.out->dollar.y >= lst_param.lines_per_page -
	    ((lst_param.lines_per_page < BIG_PG) ? SMALL_PG_BOT_SP : BIG_PG_BOT_SP))
		list_head(1);
}


void list_tab(void)
{
	op_wttab(LISTTAB);
	return;
}
