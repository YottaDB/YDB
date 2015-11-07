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
#include <rms.h>
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include <syidef.h>
#include <efndef.h>

#include "vmsdtype.h"

typedef struct {
	item_list_3	ilist;
	int4		terminator;
} syistruct;

GBLREF cm_region_head	*reglist;

cm_region_head *gtcmd_ini_reg(connection_struct *cnx)
{
	cm_region_head	*rh, *ptr, *last;
	struct FAB	tst;
	struct NAM	name;
	unsigned char	*fname,dummy[MAX_FN_LEN];
	unsigned short	len;
	uint4		status;
	unsigned short	retlen;
	char		node[15];
	syistruct	syi_list;
	short		iosb[4];
	gds_file_id	dbid;
	gd_region	*gv_match();
	file_control	*file_cntl;

	error_def (ERR_DBOPNERR);

	ptr = 0;
	fname = cnx->clb_ptr->mbf;
	fname++;
	len = *((unsigned short *) fname)++;
	if (len > MAX_FN_LEN)
		rts_error(VARLSTCNT(4) ERR_DBOPNERR, 2, len, fname);
	tst = cc$rms_fab;
	tst.fab$l_nam = &name;
	name = cc$rms_nam;
	name.nam$b_ess = MAX_FN_LEN;
	name.nam$l_esa = dummy;
	tst.fab$l_fna = fname;
	tst.fab$b_fns = len;
	status = sys$parse(&tst);
	if (status != RMS$_NORMAL)
		rts_error(VARLSTCNT(6) ERR_DBOPNERR, 2, len, fname, status, tst.fab$l_stv);
	status = sys$search(&tst);
	if (status != RMS$_NORMAL)
		rts_error(VARLSTCNT(6) ERR_DBOPNERR, 2, len, fname, status, tst.fab$l_stv);
	memcpy(dbid.dvi, name.nam$t_dvi, SIZEOF(name.nam$t_dvi));
	memcpy(dbid.did, name.nam$w_did, SIZEOF(name.nam$w_did));
	memcpy(dbid.fid, name.nam$w_fid, SIZEOF(name.nam$w_fid));
	last = reglist;
	for (ptr = reglist; ptr; ptr = ptr->next)
	{
		file_cntl = ptr->reg->dyn.addr->file_cntl;
		if ((NULL != file_cntl) && (NULL != file_cntl->file_info)
				&& is_gdid_gdid_identical(&dbid, &((vms_gds_info *)file_cntl->file_info)->file_id))
			break;
		last = ptr;
	}
	/* All open regions should be stored in a manner accessible to stop processing in case of the server being VMS stopped */
	if (!ptr)
	{	/* open region */
		ptr = malloc(SIZEOF(*ptr));
		ptr->next = 0;
		ptr->last = 0;
		ptr->head.fl = ptr->head.bl = 0;
		if (last)
		{
			last->next = ptr;
			ptr->last = last;
		} else
			reglist = ptr;
		ptr->reg = malloc(SIZEOF(struct gd_region_struct) + SIZEOF(struct gd_segment_struct));
		memset(ptr->reg, 0,SIZEOF(struct gd_region_struct) + SIZEOF(struct gd_segment_struct));
		ptr->refcnt = 0;
		ptr->reg->open = FALSE;
		ptr->reg->dyn.addr = (unsigned char *) ptr->reg + SIZEOF(struct gd_region_struct);
		ptr->reg->dyn.addr->acc_meth = dba_bg;
		memcpy(ptr->reg->dyn.addr->fname,fname,len);
		ptr->reg->dyn.addr->fname_len = len;
		ptr->reg_hash = malloc(SIZEOF(hash_table_mname));
		syi_list.ilist.buffer_length = SIZEOF(node);
		syi_list.ilist.item_code = SYI$_NODENAME;
		syi_list.ilist.buffer_address = node;
		syi_list.ilist.return_length_address = &retlen;
		syi_list.terminator = 0;
		status = sys$getsyiw(EFN$C_ENF, 0, 0, &syi_list, &iosb[0], 0, 0);
		if ((status & 1) && (iosb[0] & 1))
		{
			memcpy(ptr->reg->rname, node, retlen);
			if (retlen < SIZEOF(node))
			{
				ptr->reg->rname[retlen] = ':';
				if (retlen < (SIZEOF(node) - 1))
					ptr->reg->rname[retlen + 1] = ':';
			}
		}
	}
	if (!ptr->reg->open)
	{
		ptr->wakeup = 0;	/* Init EACH time (re)open region */
		gtcmd_cst_init(ptr);
	}
	return ptr;
}
