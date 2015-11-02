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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "parse_file.h"
#include "is_raw_dev.h"

#define MAX_NODE_NAME 32

bool reg_cmcheck(gd_region *reg)
{
	gd_segment	*seg;
	char		fbuff[MAX_FBUFF + 1];
	parse_blk	pblk;
	mstr		file;
	int		status;
	error_def(ERR_DBFILERR);

	seg = reg->dyn.addr;
	file.addr = (char *)seg->fname;
	file.len = seg->fname_len;
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = fbuff;
	pblk.buff_size = MAX_FBUFF;
	pblk.fop = (F_SYNTAXO | F_PARNODE);
	strncpy(fbuff,file.addr,file.len);
	*(fbuff + file.len) = '\0';
	if (is_raw_dev(fbuff))
	{
		pblk.def1_buf = DEF_NODBEXT;
		pblk.def1_size = SIZEOF(DEF_NODBEXT) - 1;
	}
	else
	{
		pblk.def1_buf = DEF_DBEXT;
		pblk.def1_size = SIZEOF(DEF_DBEXT) - 1;
	}

	status = parse_file(&file, &pblk);
	if (!(status & 1))
		rts_error(VARLSTCNT(5) ERR_DBFILERR,2, seg->fname_len, seg->fname, status);

	assert((int)pblk.b_esl + 1 <= SIZEOF(seg->fname));
	memcpy(seg->fname, pblk.buffer, pblk.b_esl);
	pblk.buffer[pblk.b_esl] = 0;
	seg->fname[pblk.b_esl] = 0;
	seg->fname_len = pblk.b_esl;
	if (pblk.fnb & F_HAS_NODE)
	{
		assert(pblk.b_node && ':' == pblk.l_node[pblk.b_node - 1]);
		return TRUE;
	}
	return FALSE;
}
