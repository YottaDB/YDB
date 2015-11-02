/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "rc.h"
#include "cdb_sc.h"
#include "rc_oflow.h"
#include "copy.h"
#include "gvcst_protos.h"	/* for gvcst_order,gvcst_search prototype */
#include "t_begin.h"
#include "t_retry.h"
#include "t_end.h"

GBLREF int		rc_size_return;
GBLREF gd_addr		*gd_header;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_data	*cs_data;
GBLREF sgmnt_addrs	*cs_addrs;
GBLDEF rc_oflow	*rc_overflow;
GBLREF gd_region	*gv_cur_region;
GBLREF trans_num	rc_read_stamp;


void rc_gbl_ord(rc_rsp_page *rsp)
{
	blk_hdr		*bp;
	bool		found;
	enum cdb_sc	status;
	mstr		name;
	short		bsiz, size_return;
	srch_blk_status	*bh;
	error_def(ERR_GVGETFAIL);

	for (;;)
	{
		gv_target = cs_addrs->dir_tree;
		found = gvcst_order();
		if (!found)
		{
			rsp->hdr.a.erc.value = RC_NETERRDBEDGE;
			rsp->size_return.value = 0;
			rsp->size_remain.value = 0;
			rsp->rstatus.value = 0;
			return;
		}
		name.addr = (char *)&gv_altkey->base[0];
		name.len = gv_altkey->end - 1;
		GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, &name);
		if (gv_target->root != 0)
		{
			/* Look to see if key exists */
			t_begin(ERR_GVGETFAIL, 0);
			for (;;)
			{
				rsp->hdr.a.len.value = (short)((char *)(&rsp->page[0]) - (char *)rsp);
				size_return = rc_size_return;
				if ((status = gvcst_search(gv_currkey, NULL)) != cdb_sc_normal)
				{
					t_retry(status);
					continue;
				}
				bh = gv_target->hist.h;
				if ((bsiz = ((blk_hdr *)(bh->buffaddr))->bsiz + RC_BLKHD_PAD) == SIZEOF(blk_hdr) + RC_BLKHD_PAD)
					found = FALSE;	/* Empty block, global does not exist */
				else
				{
					if (bsiz > size_return)
						rc_overflow->size = bsiz - size_return;
					else
					{
						rc_overflow->size = 0;
						size_return = bsiz;
					}
					memcpy(rsp->page, bh->buffaddr, SIZEOF(blk_hdr));
					PUT_SHORT(&((blk_hdr *)rsp->page)->bsiz, bsiz);
					memcpy(rsp->page + SIZEOF(blk_hdr) + RC_BLKHD_PAD,
					       bh->buffaddr + SIZEOF(blk_hdr),
					       size_return - (SIZEOF(blk_hdr) + RC_BLKHD_PAD));
					rsp->size_return.value = size_return;
					rsp->hdr.a.len.value += rsp->size_return.value;
					assert(rsp->hdr.a.len.value <= rc_size_return + SIZEOF(rc_rsp_page));
					rsp->zcode.value = (cs_data->blk_size / 512);	/* (2 ** zcode) == blk_size */
					if (rc_overflow->size)
					{
						memcpy(rc_overflow->buff,
						       bh->buffaddr + size_return - RC_BLKHD_PAD,
						       rc_overflow->size);
						rc_overflow->offset = size_return;
						rc_overflow->dsid = rsp->hdr.r.xdsid.dsid.value;
						rc_overflow->page = bh->blk_num;
						rc_overflow->zcode = rsp->zcode.value;
					}
					rsp->size_remain.value = rc_overflow->size;
				}
				if (0 != (rc_read_stamp = t_end(&gv_target->hist, NULL, TN_NOT_SPECIFIED)))
					break;
			}
			if (found)
				break;
		}
		*(&gv_currkey->base[0] + gv_currkey->end - 1) = 1;
		*(&gv_currkey->base[0] + gv_currkey->end + 1) = 0;
		gv_currkey->end += 1;
	}
	PUT_LONG(rsp->pageaddr, bh->blk_num);
	rsp->frag_offset.value = 0;
	rsp->rstatus.value = 0;
	rsp->after.value = 0;
	rsp->xcc.value = 0;
	rsp->before.value = 0;
	return;
}
