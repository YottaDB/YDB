/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "error.h"
#include "lv_val.h"
#include "mlkdef.h"
#include "zshow.h"
#include "compiler.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtm_maxstr.h"
#include "svnames.h"

GBLREF gv_key *gv_currkey;

void op_zshow(mval *func,int type,lv_val *lvn)
{
	const char	*ptr;
	boolean_t	do_all = FALSE,
			done_b = FALSE,
			done_c = FALSE,
			done_d = FALSE,
			done_g = FALSE,
			done_i = FALSE,
			done_l = FALSE,
			done_s = FALSE,
			done_v = FALSE;
	int		i;
  	zshow_out	output;

	error_def(ERR_ZSHOWBADFUNC);

	MAXSTR_BUFF_DECL(buff);

	MV_FORCE_STR(func);
	for (i = 0, ptr = func->str.addr; i < func->str.len; i++, ptr++)
	{
		switch (*ptr)
		{
			case 'B':
			case 'b':
			case 'C':
			case 'c':
			case 'D':
			case 'd':
			case 'G':
			case 'g':
			case 'I':
			case 'i':
			case 'L':
			case 'l':
			case 'S':
			case 's':
			case 'V':
			case 'v':
				continue;
			case '*':
				do_all = TRUE;
				break;
			default:
				rts_error(VARLSTCNT(1) ERR_ZSHOWBADFUNC);
		}
	}
	if (do_all)
	{
		ptr = ZSHOW_ALL;
		i = STR_LIT_LEN(ZSHOW_ALL);
	} else
	{
		ptr = func->str.addr;
		i = func->str.len;
	}
	memset(&output, 0, SIZEOF(output));
	if (type == ZSHOW_LOCAL)
		output.out_var.lv.lvar = lvn;
	else if (type == ZSHOW_GLOBAL)
	{
		output.out_var.gv.end = gv_currkey->end;
		output.out_var.gv.prev = gv_currkey->prev;
	}
	MAXSTR_BUFF_INIT;
	output.type = type;
	output.buff = &buff[0];
	output.size = SIZEOF(buff);
	output.ptr = output.buff;
	for ( ; i ; i--, ptr++)
	{
		output.line_num = 1;
		switch (*ptr)
		{	case 'B':
			case 'b':
				if (done_b)
					break;
				done_b = TRUE;
				output.code = 'B';
				zshow_zbreaks(&output);
				break;
			case 'C':
			case 'c':
				if (done_c)
					break;
				done_c = TRUE;
				output.code = 'C';
				zshow_zcalls(&output);
				break;
			case 'D':
			case 'd':
				if (done_d)
					break;
				done_d = TRUE;
				output.code = 'D';
				zshow_devices(&output);
				break;
			case 'G':
			case 'g':
				if (done_g)
					break;
				done_g = TRUE;
				output.code = 'G';
				output.line_num = 0;	/* G statistics start at 0 for <*,*> output and not 1 like the others */
				zshow_gvstats(&output);
				break;
			case 'I':
			case 'i':
				if (done_i)
					break;
				done_i = TRUE;
				output.code = 'I';
				zshow_svn(&output, SV_ALL);
				break;
			case 'L':
			case 'l':
				if (done_l)
					break;
				done_l = TRUE;
				output.code = 'L';
				output.line_num = 0;	/* L statistics start at 0 for <LUS,LUF> output and not 1 like the others */
				zshow_locks(&output);
				break;
			case 'S':
			case 's':
				if (done_s)
					break;
				done_s = TRUE;
				output.code = 'S';
				zshow_stack(&output);
				break;
			case 'V':
			case 'v':
				if (done_v)
					break;
				done_v = TRUE;
				output.code = 'V';
				zshow_zwrite(&output);
				break;
		}
	}
	output.code = 0;
	output.flush = TRUE;
	zshow_output(&output,0);
	MAXSTR_BUFF_FINI;
}
