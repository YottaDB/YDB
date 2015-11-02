/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "gtm_logicals.h"

char LITDEF gde_labels[GDE_LABEL_NUM][GDE_LABEL_SIZE] =
{
	GDE_LABEL_LITERAL
};

GBLREF mval dollar_zgbldir;

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
	char	c[MAX_FBUFF + 1];
	parse_blk pblk;
	mstr	*new;

	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = c;
	pblk.buff_size = MAX_FBUFF;
	pblk.def1_buf = DEF_GDR_EXT;
	pblk.def1_size = SIZEOF(DEF_GDR_EXT) - 1;
	status = parse_file(ms,&pblk);
	if (!(status & 1))
		rts_error(VARLSTCNT(9) ERR_ZGBLDIRACC, 6, ms->len, ms->addr, LEN_AND_LIT(""), LEN_AND_LIT(""), status);
	new = (mstr *)malloc(SIZEOF(mstr));
	new->len = pblk.b_esl;
	new->addr = (char *)malloc(pblk.b_esl);
	memcpy(new->addr, pblk.buffer, pblk.b_esl);
	return new;
}

void *open_gd_file(mstr *v)

{
	file_pointer	*fp;
	ZOS_ONLY(int	realfiletag;)

	fp = (file_pointer*)malloc(SIZEOF(*fp));
	fp->v.len = v->len;
	fp->v.addr = (char *)malloc(v->len + 1);
	memcpy(fp->v.addr, v->addr, v->len);
	*((char*)((char*)fp->v.addr + v->len)) = 0;	/* Null terminate string */
	if (FD_INVALID == (fp->fd = OPEN(fp->v.addr, O_RDONLY)))
	{
		if (!dollar_zgbldir.str.len || ((dollar_zgbldir.str.len == fp->v.len)
							&& !memcmp(dollar_zgbldir.str.addr, fp->v.addr, fp->v.len)))
		{
			rts_error(VARLSTCNT(9) ERR_ZGBLDIRACC, 6, fp->v.len, fp->v.addr,
				LEN_AND_LIT(".  Cannot continue"), LEN_AND_LIT(""), errno);
			assert(FALSE);
		}
		rts_error(VARLSTCNT(9) ERR_ZGBLDIRACC, 6, fp->v.len, fp->v.addr, LEN_AND_LIT(".  Retaining "),
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
		rts_error(VARLSTCNT(9) ERR_ZGBLDIRACC, 6, file_ptr->v.len, file_ptr->v.addr,
			LEN_AND_LIT(""), LEN_AND_LIT(""), errno);
	return is_gdid_stat_identical(gd_ptr->id, &buf);
}

void fill_gd_addr_id(gd_addr *gd_ptr, file_pointer *file_ptr)
{
	int fstat_res;
	struct stat buf;

	gd_ptr->id = (gd_id *) malloc(SIZEOF(gd_id));	/* Need to convert to gd_id_ptr_t during the 64-bit port */
	FSTAT_FILE(file_ptr->fd, &buf, fstat_res);
	if (-1 == fstat_res)
		rts_error(VARLSTCNT(9) ERR_ZGBLDIRACC, 6, file_ptr->v.len, file_ptr->v.addr,
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
			rts_error(VARLSTCNT(1) ERR_IOEOF);
		else
			rts_error(VARLSTCNT(9) ERR_ZGBLDIRACC, 6, file_ptr->v.len, file_ptr->v.addr,
				LEN_AND_LIT(""), LEN_AND_LIT(""), save_errno);
	return;
}

void dpzgbini(void)
{
	mstr	temp_mstr;
	char	temp_buff[MAX_FBUFF + 1];
	uint4 status;
	parse_blk pblk;

	temp_mstr.addr = GTM_GBLDIR;
	temp_mstr.len = SIZEOF(GTM_GBLDIR) - 1;
	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = temp_buff;
	pblk.buff_size = MAX_FBUFF;
	pblk.def1_buf = DEF_GDR_EXT;
	pblk.def1_size = SIZEOF(DEF_GDR_EXT) - 1;
	status = parse_file(&temp_mstr, &pblk);

	dollar_zgbldir.mvtype = MV_STR;
	dollar_zgbldir.str.len = SIZEOF(GTM_GBLDIR) - 1;
	dollar_zgbldir.str.addr = GTM_GBLDIR;
	if (status & 1)
	{
		dollar_zgbldir.str.len = pblk.b_esl;
		dollar_zgbldir.str.addr = pblk.buffer;
	}
	s2pool(&dollar_zgbldir.str);
}
