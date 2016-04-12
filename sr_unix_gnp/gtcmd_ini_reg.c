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

#include <sys/types.h>
#include <errno.h>

#include "gtm_unistd.h"

#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "lockconst.h"
#include "hashtab_mname.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "eintr_wrappers.h"
#include "copy.h"
#include "is_file_identical.h"
#include "gtcmd.h"
#include "min_max.h"
#include "mu_gv_cur_reg_init.h"

GBLREF cm_region_head	*reglist;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;

cm_region_head *gtcmd_ini_reg(connection_struct *cnx)
{
	cm_region_head  *ptr,*last;
	struct stat	stat_buf;
	unsigned char	*fname, buff[256];
	unsigned short	len;
	uint4		status, retlen;
	unsigned char	node[MAX_HOST_NAME_LEN];
	sgmnt_addrs	*csa;
	error_def (ERR_DBOPNERR);

	ptr = 0;
	fname = cnx->clb_ptr->mbf;
	fname++;
	GET_USHORT(len, fname); 	/* len = *((unsigned short *)fname); */
	fname += SIZEOF(unsigned short);
	buff[len] = 0;
	memcpy(buff, fname, len);
	STAT_FILE((char *)buff, &stat_buf, status);
	if ((uint4)-1 == status)
		rts_error(VARLSTCNT(5) ERR_DBOPNERR, 2, len, fname, errno);
	last = reglist;
	for (ptr = reglist ; ptr ; ptr = ptr->next)
	{
		if (is_gdid_stat_identical(&FILE_INFO(ptr->reg)->fileid, &stat_buf))
			break;
		last = ptr;
	}
	if (!ptr)
	{
		/* open region */
		ptr = (cm_region_head*)malloc(SIZEOF(*ptr));
		ptr->next = NULL;
		ptr->last = NULL;
		ptr->head.fl = ptr->head.bl = 0;
		SET_LATCH_GLOBAL(&ptr->head.latch, LOCK_AVAILABLE);
		if (!last)
			reglist = ptr;
		else
		{
			last->next = ptr;
			ptr->last = last;
		}
		mu_gv_cur_reg_init();
		ptr->reg = gv_cur_region;
		ptr->refcnt = 0;
		ptr->wakeup = 0;
		ptr->reg->open = FALSE;
		csa = &FILE_INFO(ptr->reg)->s_addrs;
		csa->now_crit = FALSE;
		csa->nl = (node_local_ptr_t)malloc(SIZEOF(node_local));
		assert(MAX_FN_LEN > len);
		memcpy(ptr->reg->dyn.addr->fname, fname, len);
		ptr->reg->dyn.addr->fname_len = len;
		set_gdid_from_stat(&FILE_INFO(ptr->reg)->fileid, &stat_buf);
		ptr->reg_hash = (hash_table_mname *)malloc(SIZEOF(hash_table_mname));
		if (-1 != gethostname((char *)node, SIZEOF(node)))
		{
			retlen = USTRLEN((char *)node);
			retlen = MIN(retlen, MAX_RN_LEN - 1);
			memcpy(ptr->reg->rname, node, retlen);
		} else
			retlen = 0;
		ptr->reg->rname[retlen] = ':';
		ptr->reg->rname_len = retlen + 1;
		gtcmd_cst_init(ptr);
	} else if (!ptr->reg->open)
	{
		gv_cur_region = ptr->reg;
		ptr->wakeup = 0;		/* Because going to reinit ctl->wakeups when open region */
		gtcmd_cst_init(ptr);
	} else
		gv_cur_region = ptr->reg;

	return ptr;
}
