/****************************************************************
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "io.h"
#include "iottdef.h"
#include "jnl.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "svnames.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "error.h"
#include "op.h"
#include "mvalconv.h"
#include "zroutines.h"
#include "getstorage.h"
#include "get_command_line.h"
#include "getzposition.h"
#include "getzprocess.h"
#include "get_ret_targ.h"
#include "mv_stent.h"
#include "dollar_zlevel.h"
#include "gtmmsg.h"
#include "error_trap.h"
#include "setzdir.h"
#include "get_reference.h"

#define ESC_OFFSET		4
#define MAX_COMMAND_LINE_LENGTH	255

GBLDEF mval		dollar_zmode;
GBLDEF mstr		dollar_zroutines;
GBLDEF mstr		dollar_zcompile;

GBLREF mval		dollar_zproc;
GBLREF mval		dollar_zdir;
GBLREF stack_frame	*frame_pointer;
GBLREF mval		dollar_estack_delta;
GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;
GBLREF io_log_name	*io_root_log_name;
GBLREF io_log_name	*dollar_principal;
GBLREF mval		dollar_ztrap;
GBLREF mval		dollar_zgbldir;
GBLREF mval		dollar_job;
GBLREF uint4		dollar_zjob;
GBLREF mval		dollar_zstatus;
GBLREF mval		dollar_zstep;
GBLREF char		*zro_root;  /* ACTUALLY some other pointer type! */
GBLREF mstr		gtmprompt;
GBLREF int		dollar_zmaxtptime;
GBLREF mstr		dollar_zsource;
GBLREF int4		dollar_zsystem;
GBLREF int4		dollar_zcstatus;
GBLREF int4		dollar_zeditor;
GBLREF short		dollar_tlevel;
GBLREF short		dollar_trestart;
GBLREF mval		dollar_etrap;
GBLREF mval		dollar_zerror;
GBLREF mval		dollar_zyerror;
GBLREF mval		dollar_system;
GBLREF mval		dollar_zinterrupt;
GBLREF boolean_t	dollar_zininterrupt;
GBLREF int4		zdir_form;
GBLREF boolean_t	ztrap_explicit_null;		/* whether $ZTRAP was explicitly set to NULL in this frame */
GBLREF int4		zdate_form;
GBLREF mval		dollar_ztexit;

LITREF mval		literal_zero,literal_one;
LITREF char		gtm_release_name[];
LITREF int4		gtm_release_name_len;

void op_svget(int varnum, mval *v)
{
	io_log_name	*tl;
	int 		count;
	char		*c1, *c2;

	error_def(ERR_UNIMPLOP);
	error_def(ERR_TEXT);
	error_def(ERR_ZDIROUTOFSYNC);

	switch (varnum)
	{
	case SV_HOROLOG:
		op_horolog(v);
		break;
	case SV_ZGBLDIR:
		v->mvtype = MV_STR;
		v->str = dollar_zgbldir.str;
		break;
	case SV_PRINCIPAL:
		tl = dollar_principal ? dollar_principal : io_root_log_name->iod->trans_name;
		v->str.addr = tl->dollar_io;
		v->str.len = tl->len;
		/*** The following should be in the I/O code ***/
		if (ESC == *v->str.addr)
		{
			if (5 > v->str.len)
				v->str.len = 0;
			else
			{
				v->str.addr += ESC_OFFSET;
				v->str.len -= ESC_OFFSET;
			}
		}
		s2pool(&(v->str));
		v->mvtype = MV_STR;
		break;
	case SV_ZIO:
		v->mvtype = MV_STR;
		/* NOTE:	This is **NOT** equivalent to :
		 *		io_curr_log_name->dollar_io
		 */
		v->str.addr = io_curr_device.in->trans_name->dollar_io;
		v->str.len = io_curr_device.in->trans_name->len;
		if (ESC == *v->str.addr)
		{
			if (5 > v->str.len)
				v->str.len = 0;
			else
			{	v->str.addr += ESC_OFFSET;
				v->str.len -= ESC_OFFSET;
			}
		}
		s2pool(&(v->str));
		break;
	case SV_JOB:
		*v = dollar_job;
		break;
	case SV_REFERENCE:
		get_reference(v);
		break;
	case SV_SYSTEM:
		*v = dollar_system;
		break;
	case SV_STORAGE:
		i2mval(v, getstorage());
		break;
	case SV_TLEVEL:
		count = (int)dollar_tlevel;
		MV_FORCE_MVAL(v, count);
		break;
	case SV_TRESTART:
		MV_FORCE_MVAL(v, (int)((MAX_VISIBLE_TRESTART < dollar_trestart) ? MAX_VISIBLE_TRESTART : dollar_trestart));
		break;
	case SV_X:
		count = (int)io_curr_device.out->dollar.x;
		MV_FORCE_MVAL(v, count);
		break;
	case SV_Y:
		count = (int)io_curr_device.out->dollar.y;
		MV_FORCE_MVAL(v, count);
		break;
	case SV_ZA:
		count = (int)io_curr_device.in->dollar.za;
		MV_FORCE_MVAL(v, count);
		break;
	case SV_ZB:
		c1 = (char *)io_curr_device.in->dollar.zb;
		c2 = c1 + sizeof(io_curr_device.in->dollar.zb);
		if (sizeof(io_curr_device.in->dollar.zb) > stringpool.top - stringpool.free)
			stp_gcol(sizeof(io_curr_device.in->dollar.zb));
		v->mvtype = MV_STR;
		v->str.addr = (char *)stringpool.free;
		while (c1 < c2 && *c1)
			*stringpool.free++ = *c1++;
		v->str.len = (char *)stringpool.free - v->str.addr;
		break;
	case SV_ZC:	/****THESE ARE DUMMY VALUES ONLY!!!!!!!!!!!!!!!!!****/
		MV_FORCE_MVAL(v, 0);
		break;
	case SV_ZCMDLINE:
		get_command_line(v, TRUE);	/* TRUE to indicate we want $ZCMDLINE (i.e. processed not actual command line) */
		break;
	case SV_ZEOF:
		*v = io_curr_device.in->dollar.zeof ? literal_one : literal_zero;
		break;
	case SV_IO:
		v->str.addr = io_curr_device.in->name->dollar_io;
		v->str.len = io_curr_device.in->name->len;
		/*** The following should be in the I/O code ***/
		if (ESC == *v->str.addr)
		{
			if (5 > v->str.len)
				v->str.len = 0;
			else
			{	v->str.addr += ESC_OFFSET;
				v->str.len -= ESC_OFFSET;
			}
		}
		s2pool(&(v->str));
		v->mvtype = MV_STR;
		break;
	case SV_PROMPT:
		v->mvtype = MV_STR;
		v->str.addr = gtmprompt.addr;
		v->str.len = gtmprompt.len;
		s2pool(&v->str);
		break;
	case SV_ZCOMPILE:
		v->mvtype = MV_STR;
		v->str = dollar_zcompile;
		s2pool(&(v->str));
		break;
	case SV_ZDIR:
		setzdir(NULL, v);
		if (v->str.len != dollar_zdir.str.len || 0 != memcmp(v->str.addr, dollar_zdir.str.addr, v->str.len))
			gtm_putmsg(VARLSTCNT(6) ERR_ZDIROUTOFSYNC, 4, v->str.len, v->str.addr,
					dollar_zdir.str.len, dollar_zdir.str.addr);
		SKIP_DEVICE_IF_NOT_NEEDED(v);
		s2pool(&(v->str));
		break;
	case SV_ZSTEP:
		*v = dollar_zstep;
		break;
	case SV_ZMODE:
		*v = dollar_zmode;
		break;
	case SV_ZMAXTPTIME:
		i2mval(v, dollar_zmaxtptime);
		break;
	case SV_ZPOS:
		getzposition(v);
		break;
	case SV_ZPROC:
		getzprocess();
		*v = dollar_zproc;
		break;
	case SV_ZLEVEL:
		count = dollar_zlevel();
		MV_FORCE_MVAL(v, count);
		break;
	case SV_ZROUTINES:
		if (!zro_root)
			zro_init();
		v->mvtype = MV_STR;
		v->str = dollar_zroutines;
		s2pool(&(v->str));
		break;
	case SV_ZSOURCE:
		v->mvtype = MV_STR;
		v->str = dollar_zsource;
		break;
	case SV_ZSTATUS:
		*v = dollar_zstatus;
		s2pool(&(v->str));
		break;
	case SV_ZTRAP:
		v->mvtype = MV_STR;
		v->str = dollar_ztrap.str;
		assert(!v->str.len || !ztrap_explicit_null);
		s2pool(&(v->str));
		break;
	case SV_DEVICE:
		get_dlr_device(v);
		break;
	case SV_KEY:
		get_dlr_key(v);
		break;
	case SV_ZVERSION:
		v->mvtype = MV_STR;
		v->str.addr = (char *)&gtm_release_name[0];
		v->str.len = gtm_release_name_len;
		break;
	case SV_ZSYSTEM:
		MV_FORCE_MVAL(v, dollar_zsystem);
		break;
	case SV_ZCSTATUS:
		MV_FORCE_MVAL(v, dollar_zcstatus);
		break;
	case SV_ZEDITOR:
		MV_FORCE_MVAL(v, dollar_zeditor);
		break;
	case SV_QUIT:
		MV_FORCE_MVAL(v, (NULL == get_ret_targ()) ? 0 : 1);
		break;
	case SV_ECODE:
		ecode_get(-1, v);
		break;
	case SV_ESTACK:
		count = (dollar_zlevel() - 1) - dollar_estack_delta.m[0];
		MV_FORCE_MVAL(v, count);
		break;
	case SV_ETRAP:
		v->mvtype = MV_STR;
		v->str = dollar_etrap.str;
		assert(!v->str.len || !ztrap_explicit_null);
		s2pool(&(v->str));
		break;
	case SV_STACK:
		count = (dollar_zlevel() - 1);
		MV_FORCE_MVAL(v, count);
		break;
	case SV_ZERROR:
		v->mvtype = MV_STR;
		v->str = dollar_zerror.str;
		s2pool(&(v->str));
		break;
	case SV_ZYERROR:
		v->mvtype = MV_STR;
		v->str = dollar_zyerror.str;
		s2pool(&(v->str));
		break;
	case SV_ZINTERRUPT:
		v->mvtype = MV_STR;
		v->str = dollar_zinterrupt.str;
		s2pool(&(v->str));
		break;
	case SV_ZININTERRUPT:
		MV_FORCE_MVAL(v, dollar_zininterrupt);
		break;
        case SV_ZJOB:
		MV_FORCE_ULONG_MVAL(v, dollar_zjob);
		break;
	case SV_ZDATE_FORM:
		MV_FORCE_MVAL(v, zdate_form);
		break;
	case SV_ZTEXIT:
		*v = dollar_ztexit;
		break;
	default:
		GTMASSERT;
	}
}
