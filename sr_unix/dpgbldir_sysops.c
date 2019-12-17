/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
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
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"

#include <errno.h>

#ifdef __MVS__
#include "gtm_zos_io.h"
#endif
#include "parse_file.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gbldirnam.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "stringpool.h"
#include "is_file_identical.h"
#include "dpgbldir.h"
#include "dpgbldir_sysops.h"
#include "ydb_logicals.h"
#include "trans_log_name.h"
#include "gtm_env_xlate_init.h"
#include "gtmdbglvl.h"

char LITDEF gde_labels[GDE_LABEL_NUM][GDE_LABEL_SIZE] =
{
	GDE_LABEL_LITERAL
};

GBLREF uint4	ydbDebugLevel;
GBLREF mstr	env_ydb_gbldir_xlate;
GBLREF mval	dollar_zgbldir;
GBLREF gd_addr	*gd_header;

error_def(ERR_ZGBLDIRACC);
error_def(ERR_IOEOF);
#ifdef __MVS__
/* Need the ERR_BADTAG and ERR_TEXT  error_defs for the TAG_POLICY macro warning */
error_def(ERR_TEXT);
error_def(ERR_BADTAG);
#endif

mstr *get_name(mstr *ms)
{
	int4	status;
	char	c[MAX_FN_LEN + 1];
	parse_blk pblk;
	mstr	*new;

	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = c;
	pblk.buff_size = MAX_FN_LEN;
	pblk.def1_buf = DEF_GDR_EXT;
	pblk.def1_size = SIZEOF(DEF_GDR_EXT) - 1;
	status = parse_file(ms,&pblk);
	if (!(status & 1))
		rts_error_csa(CSA_ARG(NULL)
			VARLSTCNT(9) ERR_ZGBLDIRACC, 6, ms->len, ms->addr, LEN_AND_LIT(""), LEN_AND_LIT(""), status);
	new = (mstr *)malloc(SIZEOF(mstr));
	new->len = pblk.b_esl;
	new->addr = (char *)malloc(pblk.b_esl);
	memcpy(new->addr, pblk.buffer, pblk.b_esl);
	return new;
}

void *open_gd_file(mstr *v)

{
	file_pointer	*fp;
	mstr		temp;
	ZOS_ONLY(int	realfiletag;)

	fp = (file_pointer*)malloc(SIZEOF(*fp));
	fp->v.len = v->len;
	fp->v.addr = (char *)malloc(v->len + 1);
	memcpy(fp->v.addr, v->addr, v->len);
	*((char*)((char*)fp->v.addr + v->len)) = 0;	/* Null terminate string */
	if (FD_INVALID == (fp->fd = OPEN(fp->v.addr, O_RDONLY)))
	{	/* v gets passed down through a few levels, but should be freed */
		/* Copy the values into the stringpool so they get cleaned up later */
		ENSURE_STP_FREE_SPACE(fp->v.len);
		memcpy(stringpool.free, fp->v.addr, fp->v.len);
		temp.addr = (char*)stringpool.free;
		temp.len = fp->v.len;
		stringpool.free += fp->v.len;
		free(v->addr);
		free(v);
		free(fp->v.addr);
		free(fp);
		if (!dollar_zgbldir.str.len || ((dollar_zgbldir.str.len == temp.len)
							&& !memcmp(dollar_zgbldir.str.addr, temp.addr, temp.len)))
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZGBLDIRACC, 6, temp.len, temp.addr,
				LEN_AND_LIT(".  Cannot continue"), LEN_AND_LIT(""), errno);
			assert(FALSE);
		}
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZGBLDIRACC, 6, temp.len, temp.addr, LEN_AND_LIT(".  Retaining "),
			dollar_zgbldir.str.len, dollar_zgbldir.str.addr, errno);
	}
#ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(fp->fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_SEND_MSG(fp->v.addr, errno, realfiletag, TAG_BINARY);
#endif
	return (void *)fp;
}

bool comp_gd_addr(gd_addr *gd_ptr, file_pointer *file_ptr)
{
	int fstat_res;
	struct stat buf;

	FSTAT_FILE(file_ptr->fd, &buf, fstat_res);
	if (-1 == fstat_res)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZGBLDIRACC, 6, file_ptr->v.len, file_ptr->v.addr,
			LEN_AND_LIT(""), LEN_AND_LIT(""), errno);
	return is_gdid_stat_identical(gd_ptr->id, &buf);
}

void fill_gd_addr_id(gd_addr *gd_ptr, file_pointer *file_ptr)
{
	int fstat_res;
	struct stat buf;

	gd_ptr->id = (gd_id *)malloc(SIZEOF(gd_id));
	FSTAT_FILE(file_ptr->fd, &buf, fstat_res);
	if (-1 == fstat_res)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZGBLDIRACC, 6, file_ptr->v.len, file_ptr->v.addr,
			LEN_AND_LIT(""), LEN_AND_LIT(""), errno);
	set_gdid_from_stat(gd_ptr->id, &buf);
	return;
}
void close_gd_file(file_pointer *file_ptr)
{
	int	rc;

	CLOSEFILE_RESET(file_ptr->fd, rc);	/* resets "file_ptr->fd" to FD_INVALID */
	free(file_ptr->v.addr);
	free(file_ptr);
	return;
}

void file_read(file_pointer *file_ptr, int4 size, uchar_ptr_t buff, int4 pos)
{
	int4	save_errno;

	LSEEKREAD(file_ptr->fd, (off_t)(pos - 1 ) * DISK_BLOCK_SIZE, buff, size, save_errno);
	if (0 != save_errno)
		if (-1 == save_errno)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_IOEOF);
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_ZGBLDIRACC, 6, file_ptr->v.len, file_ptr->v.addr,
				LEN_AND_LIT(""), LEN_AND_LIT(""), save_errno);
	return;
}

void dpzgbini(void)
{
	mstr		gbldirenv_mstr, trnlnm_mstr, *tran_mstr;
	mval		tran_mval, temp_mval;
	char		temp_buff1[MAX_FN_LEN + 1];
	char		temp_buff2[MAX_FN_LEN + 1];
	uint4		status;
	parse_blk	pblk;

	gbldirenv_mstr.addr = (char *)YDB_GBLDIR;
	gbldirenv_mstr.len = STRLEN(YDB_GBLDIR);

	dollar_zgbldir.mvtype = MV_STR;
	dollar_zgbldir.str.addr = (char *)YDB_GBLDIR;
	dollar_zgbldir.str.len = STRLEN(YDB_GBLDIR);

	/* Translate the logical name (environment variable) ydb_gbldir/gtmgbldir
	 * which would be done in parse_file but we need to know the its value
	 * so that we can attempt to go global directory translation
	 */
	status = trans_log_name(&gbldirenv_mstr, &trnlnm_mstr, temp_buff1, MAX_FN_LEN + 1, dont_sendmsg_on_log2long);
	YDB_GBLENV_XLATE_DEBUG("dpzgbini: lnm=%.*s status=%d trnlnm=%.*s",
		(int) gbldirenv_mstr.len, gbldirenv_mstr.addr,
		status,
		(int) trnlnm_mstr.len, trnlnm_mstr.addr);
	if (status != SS_LOG2LONG)
	{
		temp_mval.str = trnlnm_mstr;
		temp_mval.mvtype = MV_STR;
		tran_mstr = env_ydb_gbldir_xlate.len?
			(&ydb_gbldir_translate(&temp_mval, &tran_mval)->str) : &trnlnm_mstr;
		YDB_GBLENV_XLATE_DEBUG("dpzgbini: translate=%d tran_mstr=%.*s",
			env_ydb_gbldir_xlate.len,
			(int) tran_mstr->len, tran_mstr->addr);
		memset(&pblk, 0, SIZEOF(pblk));
		pblk.buffer = temp_buff2;
		pblk.buff_size = MAX_FN_LEN;
		pblk.def1_buf = DEF_GDR_EXT;
		pblk.def1_size = SIZEOF(DEF_GDR_EXT) - 1;
		status = parse_file(tran_mstr, &pblk);
		if (status & 1)
		{
			dollar_zgbldir.str.len = env_ydb_gbldir_xlate.len? trnlnm_mstr.len : pblk.b_esl;
			dollar_zgbldir.str.addr = env_ydb_gbldir_xlate.len? trnlnm_mstr.addr : pblk.buffer;
		}
	}

	s2pool(&dollar_zgbldir.str);
	gd_header = NULL;
}
