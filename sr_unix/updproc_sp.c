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
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"

#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "filestruct.h"
#include "jnl.h"
#include "gdscc.h"
#include "gtmio.h"
#include "gdskill.h"
#include "muprec.h"
#include "tp.h"
#include "parse_file.h"
#include "is_raw_dev.h"
#include "updproc.h"

upd_proc_ctl *read_db_files_from_gld(gd_addr *addr)
{
	parse_blk 		pblk;
	mstr			file;
	gd_segment 		*seg;
	char			fbuff[MAX_FBUFF + 1];
	int			status;
	gd_region		*map_region;
	gd_region		*map_region_top;
	upd_proc_ctl		head, *ctl = &head;


	head.next = NULL;

	for (map_region = addr->regions, map_region_top = map_region + addr->n_regions; map_region < map_region_top; map_region++)
	{
		assert (map_region < map_region_top);
		seg = (gd_segment *)map_region->dyn.addr;
		if (NULL == seg->file_cntl)
		{
			seg->file_cntl = (file_control *)malloc(sizeof(*seg->file_cntl));
			memset(seg->file_cntl, 0, sizeof(*seg->file_cntl));
		}
		if (NULL == seg->file_cntl->file_info)
		{
			seg->file_cntl->file_info = (void *)malloc(sizeof(GDS_INFO));
			memset(seg->file_cntl->file_info, 0, sizeof(GDS_INFO));
		}

		file.addr = (char *)seg->fname;
		file.len  = seg->fname_len;

		memset(&pblk, 0, sizeof(pblk));
		pblk.buffer 	= fbuff;
		pblk.buff_size	= MAX_FBUFF;
		pblk.fop 	= (F_SYNTAXO | F_PARNODE);
		memcpy(fbuff, file.addr, file.len);
		*(fbuff + file.len) = '\0';
		if (is_raw_dev(fbuff))
		{
			pblk.def1_buf = DEF_NODBEXT;
			pblk.def1_size = sizeof(DEF_NODBEXT) - 1;
		}else
		{
			pblk.def1_buf = DEF_DBEXT;
			pblk.def1_size = sizeof(DEF_DBEXT) - 1;
		}
		status = parse_file(&file, &pblk);

		memcpy(seg->fname, pblk.buffer, pblk.b_esl);
		pblk.buffer[pblk.b_esl] = 0;
		seg->fname[pblk.b_esl] = 0;
		seg->fname_len = pblk.b_esl;

		ctl = ctl->next
		    = (upd_proc_ctl *)malloc(sizeof(upd_proc_ctl));
	        memset(ctl, 0, sizeof(upd_proc_ctl));
		map_region->stat.addr = (void *)ctl;
		ctl->gd = map_region;
	}
	return head.next;
}
