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

#include <ssdef.h>
#include <descrip.h>
#include <rms.h>

#include "io.h"
#include "io_params.h"
#include "cmd_qlf.h"
#include "list_file.h"
#include "op.h"

#define LISTEXT ".LIS"

GBLREF int			(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);
GBLREF command_qualifier  	cmd_qlf;
GBLREF mident			module_name;
GBLREF io_pair			io_curr_device;
GBLREF list_params 		lst_param;

static char print_time_buf[20];
static io_pair dev_in_use;

void open_list_file(void)
{
	char charspace;
	uint4 status;
	unsigned char	list_name[MAX_MIDENT_LEN + STR_LIT_LEN(LISTEXT)], fname[255];
	struct FAB	fab;
	struct NAM	nam;

#ifdef __ALPHA
# pragma member_alignment save
# pragma nomember_alignment
#endif
	static readonly struct{
	unsigned char	newversion;
	unsigned char	wrap;
	unsigned char	width;
	int4		v_width;
	unsigned char	eol;
	}open_params_list = {
	(unsigned char)iop_newversion,	(unsigned char)iop_wrap,
	(unsigned char)iop_recordsize,	(int4)132,
	(unsigned char)iop_eol
	};
#ifdef __ALPHA
# pragma member_alignment restore
#endif

	mval params;
	mval file;
	struct dsc$descriptor_s print_time_d
		= { SIZEOF(print_time_buf), DSC$K_DTYPE_T, DSC$K_CLASS_S, print_time_buf };

	lst_param.list_line = 1;
	lst_param.page = 0;

	fab = cc$rms_fab;
	nam = cc$rms_nam;
	fab.fab$l_dna = &list_name[0];
	assert(module_name.len <= MAX_MIDENT_LEN);
	fab.fab$b_dns = module_name.len;
	memcpy(&list_name[0], module_name.addr, fab.fab$b_dns);
	MEMCPY_LIT(&list_name[fab.fab$b_dns], LISTEXT);
	fab.fab$b_dns += STR_LIT_LEN(LISTEXT);
	if (MV_DEFINED(&cmd_qlf.list_file))
	{
		fab.fab$b_fns = cmd_qlf.list_file.str.len;
		fab.fab$l_fna = cmd_qlf.list_file.str.addr;
	}
	nam.nam$l_esa = &fname[0];
	nam.nam$b_ess = SIZEOF(fname);
	nam.nam$b_nop = (NAM$M_SYNCHK);
	fab.fab$l_nam = &nam;
	fab.fab$l_fop = FAB$M_NAM;
	if ((status = sys$parse(&fab,0,0)) != RMS$_NORMAL)
	{	rts_error(VARLSTCNT(1) status);
	}

	file.mvtype = params.mvtype = MV_STR;
	file.str.len = nam.nam$b_esl;
	file.str.addr = &fname[0];
	params.str.len = SIZEOF(open_params_list);
	params.str.addr = &open_params_list;
	(*op_open_ptr)(&file, &params, 30, 0);
	params.str.len = 1;
	charspace = (char) iop_eol;
	params.str.addr = &charspace;
	dev_in_use = io_curr_device;
	op_use(&file,&params);
	lib$date_time(&print_time_d);
	list_head(0);
	return;
}

void close_list_file(void)
{
	mval param,list_file;
	unsigned char charspace;

	param.str.len = 1;
	charspace = (char) iop_eol;
	param.str.addr = &charspace;
	list_file.mvtype = param.mvtype = MV_STR;
	list_file.str.len = io_curr_device.in->trans_name->len;
	list_file.str.addr = &io_curr_device.in->trans_name->dollar_io[0];
	op_close(&list_file, &param);
	io_curr_device = dev_in_use;
}


void list_cmd(void)
{
	unsigned short cmd_len;
	unsigned char cmd_line[256];
	static readonly unsigned char command_line[] =		"COMMAND LINE";
	static readonly unsigned char command_line_under[] =	"-----------------";
	$DESCRIPTOR(d_cmd,cmd_line);

	if (lib$get_foreign(&d_cmd, 0, &cmd_len) == SS$_NORMAL)
	{
		list_line(command_line);
		list_line(command_line_under);
		cmd_line[cmd_len]='\0';
		list_line(cmd_line);
	}
}


LITREF char gtm_release_name[];
LITREF int4 gtm_release_name_len;

GBLREF char source_file_name[];
GBLREF unsigned short source_name_len;
GBLREF char rev_time_buf[];

void list_head(bool newpage)
{
	short col_2 = 70;
	static readonly unsigned char page_lit[] = "page ";
	unsigned char page_no_buf[10];
	mval head;

	if (newpage)
		op_wtff();

	head.mvtype = MV_STR;
	head.str.addr = &gtm_release_name[0];
	head.str.len = gtm_release_name_len;
	op_write (&head);

	op_wttab(col_2);
	head.str.addr = print_time_buf;
	head.str.len = 20;
	op_write(&head);

	op_wttab(100);
	lst_param.page++;
	head.str.addr = page_lit;
	head.str.len = SIZEOF(page_lit) - 1;
	op_write(&head);

	head.str.addr = page_no_buf;
	head.str.len = i2asc(page_no_buf, lst_param.page) - page_no_buf;
	op_write(&head);
	op_wteol(1);

	head.str.addr = source_file_name;
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
	}
	else
		op_wteol(lst_param.space);
}

void list_line_number(void)
{
	void op_write();
	unsigned char buf[8];
	int n,m, i, q;
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
	out.str.addr = buf;
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
