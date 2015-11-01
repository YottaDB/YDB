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


#include "gtm_stat.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "parse_file.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "is_raw_dev.h"
#include "gtmmsg.h"

GBLDEF bool in_backup;

mstr *mupfndfil(gd_region *reg)
{
	char		fbuff[MAX_FBUFF + 1];
	int		status;
	mstr		file, *tmp;
	int		stat_res;
	struct stat	stat_buf;
	parse_blk	pblk;

	error_def(ERR_DBFILERR);

	switch(reg->dyn.addr->acc_meth)
	{
	case dba_bg:
	case dba_mm:
		break;
	case dba_usr:
		util_out_print("REGION !AD maps to a non-GTC database.  Specified function does not apply to a non-GTC database.",
			TRUE, REG_LEN_STR(reg));
		return NULL;
	default:
		util_out_print("REGION !AD has an unrecognized access method.", TRUE, REG_LEN_STR(reg));
		return NULL;
	}
	file.addr = (char *)reg->dyn.addr->fname;
	file.len = reg->dyn.addr->fname_len;
	memset(&pblk, 0, sizeof(pblk));
	pblk.buffer = fbuff;
	pblk.buff_size = MAX_FBUFF;
	pblk.fop = (F_SYNTAXO | F_PARNODE);
	memcpy(fbuff, file.addr, file.len);
	*(fbuff + file.len) = '\0';
	if (is_raw_dev(fbuff))
	{
		pblk.def1_buf = DEF_NODBEXT;
		pblk.def1_size = sizeof(DEF_NODBEXT) - 1;
	} else
	{
		pblk.def1_buf = DEF_DBEXT;
		pblk.def1_size = sizeof(DEF_DBEXT) - 1;
	}
	status = parse_file(&file, &pblk);
	if (!(status & 1))
	{
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
		return NULL;
	}
	assert(pblk.b_esl < sizeof(reg->dyn.addr->fname));
	memcpy(reg->dyn.addr->fname, pblk.buffer, pblk.b_esl);
	pblk.buffer[pblk.b_esl] = 0;
	reg->dyn.addr->fname[pblk.b_esl] = 0;
	reg->dyn.addr->fname_len = pblk.b_esl;
	STAT_FILE((char *)reg->dyn.addr->fname, &stat_buf, stat_res);
	if (-1 == stat_res)
	{
		util_out_print("Region !AD's file !AD cannot be found.", TRUE, REG_LEN_STR(reg), DB_LEN_STR(reg));
		return NULL;
	}
	if (!in_backup)
		return (mstr *)&in_backup; 	/* caller only testing for existence, and any non-NULL address will serve */
	tmp = (mstr *)malloc(sizeof(mstr));
	tmp->len = reg->dyn.addr->fname_len;
	tmp->addr = (char *)malloc(tmp->len + 1);
	memcpy(tmp->addr, reg->dyn.addr->fname, tmp->len + 1);
	return tmp;
}
