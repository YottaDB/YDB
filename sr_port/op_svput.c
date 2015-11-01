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
#include "underr.h"
#include "gtmmsg.h"

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
GBLREF mval           dollar_ecode;
GBLREF mval           dollar_etrap;
GBLREF mval           dollar_zerror;
GBLREF mval           dollar_zyerror;

void op_svput(int varnum, mval *v)
{
	error_def(ERR_UNIMPLOP);
	error_def(ERR_TEXT);

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
		gd_header = zgbldir(v);
		dollar_zgbldir.str.len = v->str.len;
		dollar_zgbldir.str.addr = v->str.addr;
		s2pool(&dollar_zgbldir.str);
		if (gv_currkey)
			gv_currkey->base[0] = 0;
		if (gv_target)
			gv_target->clue.end = 0;
		break;
	case SV_ZMAXTPTIME:
		dollar_zmaxtptime = mval2i(v);
		break;
	case SV_ZROUTINES:
		MV_FORCE_STR(v);
		if (dollar_zroutines.addr)
			free (dollar_zroutines.addr);
		dollar_zroutines.addr = (char *)malloc(v->str.len);
		memcpy (dollar_zroutines.addr, v->str.addr, v->str.len);
		dollar_zroutines.len = v->str.len;
		zro_load (&dollar_zroutines);
		break;
	case SV_ZSOURCE:
		MV_FORCE_STR(v);
		dollar_zsource = v->str;
		break;
	case SV_ZTRAP:
		MV_FORCE_STR(v);
		dollar_ztrap.mvtype = MV_STR;
		dollar_ztrap.str = v->str;
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
		dollar_ecode.mvtype = MV_STR;
		dollar_ecode.str = v->str;
		gtm_putmsg(VARLSTCNT(1) MAKE_MSG_WARNING(ERR_UNIMPLOP));
		gtm_putmsg(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("$ECODE"));
		break;
	case SV_ETRAP:
		MV_FORCE_STR(v);
		dollar_etrap.mvtype = MV_STR;
		dollar_etrap.str = v->str;
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
	default:
		GTMASSERT;
	}
	return;
}
