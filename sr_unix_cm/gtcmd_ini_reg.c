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

#include <sys/types.h>
#include <errno.h>

#include "gtm_stat.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "hashdef.h"
#include "cmidef.h"
#include "cmmdef.h"

GBLDEF cm_region_head	*reglist;
GBLREF gd_region	*gv_cur_region;

cm_region_head *gtcmd_ini_reg(connection_struct *cnx)
{
	cm_region_head  *ptr,*last;
	struct stat	stat_buf;
	unsigned char	*fname, buff[256];
	unsigned short	len;
	uint4		status, retlen;
	char		node[15];
	error_def (ERR_DBOPNERR);

	ptr = 0;
	fname = cnx->clb_ptr->mbf;
	fname++;
	len = *((unsigned short *)fname);
	fname += sizeof(unsigned short);
	buff[len] = 0;
	memcpy(buff,fname,len);
	if (stat((const char *)buff, &stat_buf) < 0)
		rts_error(VARLSTCNT(6) ERR_DBOPNERR, 2, len, fname, errno, 0);
	last = reglist;
	for (ptr = reglist ; ptr ; ptr = ptr->next)
	{
		if (((unix_db_info*)(ptr->reg->dyn.addr->file_cntl->file_info))->fileid.inode == stat_buf.st_ino)
			break;
		last = ptr;
	}
	if (!ptr)
	{	/* open region */
#ifdef DP
fprintf(stderr,"gtcmd_ini_reg: new region\n");
#endif
		ptr = (cm_region_head*)malloc(sizeof(*ptr));
		ptr->next = 0;
		ptr->last = 0;
		ptr->head.fl = ptr->head.bl = 0;
		if (!last)
			reglist = ptr;
		else
		{	last->next = ptr;
			ptr->last = last;
		}
		ptr->reg = (gd_region*)malloc(sizeof(struct gd_region_struct) + sizeof(struct gd_segment_struct) +
			sizeof(struct file_control_struct) + sizeof(struct unix_db_info_struct));
		memset(ptr->reg, 0,sizeof(struct gd_region_struct) + sizeof(struct gd_segment_struct)  +
				sizeof(struct file_control_struct) + sizeof(struct unix_db_info_struct));
		ptr->refcnt = 0;
		ptr->wakeup = 0;
		ptr->reg->open = FALSE;
		ptr->reg->dyn.addr = (gd_segment*)((unsigned char *) ptr->reg + sizeof(struct gd_region_struct));
		ptr->reg->dyn.addr->file_cntl = (file_control*)
				((unsigned char *)ptr->reg + sizeof(struct gd_region_struct) + sizeof(struct gd_segment_struct));
		ptr->reg->dyn.addr->file_cntl->file_info = (void*)((unsigned char *)ptr->reg->dyn.addr->file_cntl +
				sizeof(struct file_control_struct));
		((unix_db_info *)(ptr->reg->dyn.addr->file_cntl->file_info))->s_addrs.now_crit = 0;
		ptr->reg->dyn.addr->acc_meth = dba_bg;
		memcpy(ptr->reg->dyn.addr->fname,fname,len);
		ptr->reg->dyn.addr->fname_len = len;
		((unix_db_info*)(ptr->reg->dyn.addr->file_cntl->file_info))->fileid.inode = stat_buf.st_ino;
		gv_cur_region = ptr->reg;
		ptr->reg_hash = (struct htab_desc *)malloc(sizeof(htab_desc));

		if (gethostname(node, sizeof(node)) != -1)
		{	retlen = strlen(node);
			memcpy(ptr->reg->rname,node,retlen);
			if (retlen < sizeof(node))
			{	ptr->reg->rname[retlen] = ':';
				if (retlen < (sizeof(node) - 1))
					ptr->reg->rname[retlen + 1] = ':';
			}
		}
		gtcmd_cst_init(ptr);
	}
	else if (!ptr->reg->open)
	{
#ifdef DP
fprintf(stderr,"gtcmd_ini_reg: old region, not open\n");
#endif
		gv_cur_region = ptr->reg;
		gtcmd_cst_init(ptr);
	}else
	{	gv_cur_region = ptr->reg;
#ifdef DP
fprintf(stderr,"gtcmd_ini_reg: old region, already open\n");
#endif
	}
	return ptr;
}
