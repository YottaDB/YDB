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
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "svnames.h"
#include "io.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "indir_enum.h"
#include "error.h"
#include "stringpool.h"
#include "op.h"
#include "mvalconv.h"
#include "zroutines.h"
#include "dpgbldir.h"
#include "dpgbldir_sysops.h"
#include "underr.h"
#include "gtmmsg.h"
#include "ztrap_save_ctxt.h"
#include "dollar_zlevel.h"
#include "error_trap.h"
#include "gtm_ctype.h"
#include "setzdir.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "getzdir.h"
#include "gtm_newintrinsic.h"

#define PROMPTBUF "GTM>           "

GBLDEF char		prombuf[] = PROMPTBUF;
GBLDEF MSTR_DEF(gtmprompt, 4, &prombuf[0]);

GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF gd_addr		*gd_header;
GBLREF io_pair		io_curr_device;
GBLREF mval		dollar_ztrap;
GBLREF mval		dollar_zstatus;
GBLREF mval		dollar_zgbldir;
GBLREF mval		dollar_zstep;
GBLREF mstr		dollar_zcompile;
GBLREF mstr		dollar_zroutines;
GBLREF mstr		dollar_zsource;
GBLREF int		dollar_zmaxtptime;
GBLREF int		ztrap_form;
GBLREF mval		dollar_etrap;
GBLREF mval		dollar_zerror;
GBLREF mval		dollar_zyerror;
GBLREF mval		dollar_system;
GBLREF mval		dollar_zinterrupt;
GBLREF boolean_t	ztrap_new;
GBLREF stack_frame	*error_frame;
GBLREF boolean_t	ztrap_explicit_null;		/* whether $ZTRAP was explicitly set to NULL in this frame */
GBLREF int 		zdate_form;

void op_svput(int varnum, mval *v)
{
	int		i, ok, state;
	error_def(ERR_UNIMPLOP);
	error_def(ERR_TEXT);
	error_def(ERR_INVECODEVAL);
	error_def(ERR_SETECODE);
	error_def(ERR_SYSTEMVALUE);

	switch (varnum)
	{
	case SV_X:
		MV_FORCE_NUM(v);
		io_curr_device.out->dollar.x = (short)MV_FORCE_INT(v);
		if ((short)(io_curr_device.out->dollar.x) < 0)
			io_curr_device.out->dollar.x = 0;
		break;
	case SV_Y:
		MV_FORCE_NUM(v);
		io_curr_device.out->dollar.y = (short)MV_FORCE_INT(v);
		if ((short)(io_curr_device.out->dollar.y) < 0)
			io_curr_device.out->dollar.y = 0;
		break;
	case SV_ZCOMPILE:
		MV_FORCE_STR(v);
		if (dollar_zcompile.addr)
			free (dollar_zcompile.addr);
		dollar_zcompile.addr = (char *)malloc(v->str.len);
		memcpy (dollar_zcompile.addr, v->str.addr, v->str.len);
		dollar_zcompile.len = v->str.len;
		break;
	case SV_ZSTEP:
		MV_FORCE_STR(v);
		op_commarg(v,indir_linetail);
		op_unwind();
		dollar_zstep = *v;
		break;
	case SV_ZGBLDIR:
		MV_FORCE_STR(v);
		if (!(dollar_zgbldir.str.len == v->str.len &&
		    !memcmp(dollar_zgbldir.str.addr, v->str.addr,
		    dollar_zgbldir.str.len)))
		{
			if(v->str.len == 0)
			{
			        /* set $zgbldir="" */
	   			dpzgbini();
				gd_header = NULL;
	       		} else
			{
	   			gd_header = zgbldir(v);
	   			dollar_zgbldir.str.len = v->str.len;
	   			dollar_zgbldir.str.addr = v->str.addr;
	  			s2pool(&dollar_zgbldir.str);
			}
		   	if (gv_currkey)
				gv_currkey->base[0] = 0;
		   	if (gv_target)
				gv_target->clue.end = 0;
		}
		break;
	case SV_ZMAXTPTIME:
		dollar_zmaxtptime = mval2i(v);
		break;
	case SV_ZROUTINES:
		MV_FORCE_STR(v);
		/* The string(v) should be parsed and loaded before setting $zroutines
		 * to retain the old value in case errors occur while loading */
		zro_load(&v->str);
		if (dollar_zroutines.addr)
			free (dollar_zroutines.addr);
		dollar_zroutines.addr = (char *)malloc(v->str.len);
		memcpy (dollar_zroutines.addr, v->str.addr, v->str.len);
		dollar_zroutines.len = v->str.len;
		break;
	case SV_ZSOURCE:
		MV_FORCE_STR(v);
		dollar_zsource = v->str;
		break;
	case SV_ZTRAP:
		MV_FORCE_STR(v);
		if (ztrap_new)
			op_newintrinsic(SV_ZTRAP);
		dollar_ztrap.mvtype = MV_STR;
		dollar_ztrap.str = v->str;
		/* Setting either $ZTRAP or $ETRAP to empty causes any current error trapping to be canceled */
		if (!v->str.len)
		{
			dollar_etrap.mvtype = MV_STR;
			dollar_etrap.str = v->str;
			ztrap_explicit_null = TRUE;
		} else /* Ensure that $ETRAP and $ZTRAP are not both active at the same time */
		{
			ztrap_explicit_null = FALSE;
			if (dollar_etrap.str.len > 0)
				gtm_newintrinsic(&dollar_etrap);
		}
		if (ztrap_form & ZTRAP_POP)
			ztrap_save_ctxt();
		break;
	case SV_ZSTATUS:
		MV_FORCE_STR(v);
		dollar_zstatus.mvtype = MV_STR;
		dollar_zstatus.str = v->str;
		break;
	case SV_PROMPT:
		MV_FORCE_STR(v);
		gtmprompt.len = v->str.len < sizeof(prombuf) ? v->str.len : sizeof(prombuf);
		memcpy(gtmprompt.addr,v->str.addr,gtmprompt.len);
		break;
	case SV_ECODE:
		MV_FORCE_STR(v);
		if (v->str.len)
		{
			/* Format must be like ,Mnnn,Mnnn,Zxxx,Uxxx,
			 * Mnnn are ANSI standard error codes
			 * Zxxx are implementation-specific codes
			 * Uxxx are end-user defined codes
			 * Note that there must be commas at the start and at the end
			 */
			for (state = 2, i = 0; (i < v->str.len) && (state <= 2); i++)
			{
				switch(state)
				{
				case 2: state = (v->str.addr[i] == ',') ? 1 : 101;
					break;
				case 1: state = ((v->str.addr[i] == 'M') ||
				                 (v->str.addr[i] == 'U') ||
					         (v->str.addr[i] == 'Z')) ? 0 : 101;
					break;
				case 0: state = (v->str.addr[i] == ',') ? 1 : 0;
					break;
				}
			}
			/* The above check would pass strings like ","
			 * so double-check that there are at least three characters
			 * (starting comma, ending comma, and something in between)
			 */
			if ((state != 1) || (v->str.len < 3))
			{
				/* error, ecode = M101 */
				rts_error(VARLSTCNT(4) ERR_INVECODEVAL, 2, v->str.len, v->str.addr);
			}
		}
		if (v->str.len > 0)
		{
			ecode_add(&v->str);
			rts_error(VARLSTCNT(2) ERR_SETECODE, 0);
		} else
		{
			NULLIFY_DOLLAR_ECODE;	/* reset $ECODE related variables to correspond to $ECODE = NULL state */
			NULLIFY_ERROR_FRAME;	/* we are no more in error-handling mode */
		}
		break;
	case SV_ETRAP:
		MV_FORCE_STR(v);
		dollar_etrap.mvtype = MV_STR;
		dollar_etrap.str = v->str;
		/* Setting either $ZTRAP or $ETRAP to empty causes any current error trapping to be canceled */
		if (!v->str.len)
		{
			dollar_ztrap.mvtype = MV_STR;
			dollar_ztrap.str = v->str;
		} else if (dollar_ztrap.str.len > 0)
		{	/* Ensure that $ETRAP and $ZTRAP are not both active at the same time */
			assert(FALSE == ztrap_explicit_null);
			gtm_newintrinsic(&dollar_ztrap);
		}
		ztrap_explicit_null = FALSE;
		break;
	case SV_ZERROR:
		MV_FORCE_STR(v);
		dollar_zerror.mvtype = MV_STR;
		dollar_zerror.str = v->str;
		break;
	case SV_ZYERROR:
		MV_FORCE_STR(v);
		dollar_zyerror.mvtype = MV_STR;
		dollar_zyerror.str = v->str;
		break;
	case SV_SYSTEM:
		ok = 1;
		if (!(v->mvtype & MV_STR))
			ok = 0;
		if (ok && v->str.addr[0] != '4')
			ok = 0;
		if (ok && v->str.addr[1] != '7')
			ok = 0;
		if ((' ' != v->str.addr[2]) && !ispunct(v->str.addr[2]))
			ok = 0;
		if (ok)
			dollar_system.str = v->str;
		else
			rts_error(VARLSTCNT(4) ERR_SYSTEMVALUE, 2, v->str.len, v->str.addr);
		break;
	case SV_ZDIR:
		setzdir(v, NULL); /* change directory to v */
		getzdir(); /* update dollar_zdir with current working directory */
		break;
	case SV_ZINTERRUPT:
		MV_FORCE_STR(v);
		dollar_zinterrupt.mvtype = MV_STR;
		dollar_zinterrupt.str = v->str;
		break;
	case SV_ZDATE_FORM:
		MV_FORCE_NUM(v);
		zdate_form = (short)MV_FORCE_INT(v);
		break;
	default:
		GTMASSERT;
	}
	return;
}
