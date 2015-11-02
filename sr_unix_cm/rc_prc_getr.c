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
#include "error.h"
#include "gtcm.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_protos.h"	/* for gvcst_search,gvcst_rtsib,gvcst_lftsib prototype */

GBLREF int		rc_size_return;
GBLREF gd_addr		*gd_header;
GBLREF gv_key 		*gv_currkey;
GBLREF gv_namehead 	*gv_target;
GBLREF sgmnt_data	*cs_data;
GBLREF sgmnt_addrs	*cs_addrs;
GBLDEF rc_oflow		*rc_overflow;
GBLREF gd_region	*gv_cur_region;
GBLREF trans_num	rc_read_stamp;
GBLREF int		gv_keysize;

error_def(ERR_GVGETFAIL);

int rc_prc_getr(rc_q_hdr *qhdr)
{
	int		key_size, data_len, i;
	bool		dollar_order, two_histories;
	char		*cp2, *cp1;
	mval		v;
	short		rsiz, bsiz, fmode, size_return;
	rec_hdr		*rp;
	blk_hdr		*bp;
	srch_hist	second_history;
	enum cdb_sc	status;
	rc_req_getr	*req;
	rc_rsp_page	*rsp;
	srch_blk_status	*bh;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH_RET(rc_dbms_ch,0);
	req = (rc_req_getr *)qhdr;
	rsp = (rc_rsp_page *)qhdr;
	rsp->hdr.a.len.value = (short)((char*)(&rsp->page[0]) - (char*)rsp);

	fmode = qhdr->r.fmd.value;
	if ((qhdr->a.erc.value = rc_fnd_file(&qhdr->r.xdsid)) != RC_SUCCESS)
	{
		REVERT;
		DEBUG_ONLY(gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"rc_fnd_file failed.");)
		return -1;
	}
	if (req->key.len.value > cs_data->max_key_size)
	{
		qhdr->a.erc.value = RC_KEYTOOLONG;
	 	rsp->size_return.value = 0;
		rsp->size_remain.value = 0;
		rsp->rstatus.value = 0;
		REVERT;
		return 0;
	}
	v.mvtype = MV_STR;
	cp2 = req->key.key + req->key.len.value;
	for (cp1 = req->key.key; *cp1 && cp1 < cp2; cp1++)
		;
	v.str.len = INTCAST(cp1 - req->key.key);
	v.str.addr = req->key.key;
	if (v.str.len > MAX_MIDENT_LEN)	/* GT.M does not support global variables > MAX_MIDENT_LEN chars */
	{
		if (!(v.str.len == (MAX_MIDENT_LEN + 1) && v.str.addr[MAX_MIDENT_LEN] == 0x01))
		{
		    qhdr->a.erc.value = RC_KEYTOOLONG;
		    rsp->size_return.value = 0;
		    rsp->size_remain.value = 0;
		    rsp->rstatus.value = 0;
		    REVERT;
		    DEBUG_ONLY(gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"RC_KEYTOOLONG.");)
		    return -1;
		}
	}
	GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, &v.str);
	memcpy(gv_currkey->base, req->key.key, req->key.len.value);
	gv_currkey->end = req->key.len.value;
	dollar_order = FALSE;
	if (gv_currkey->base[gv_currkey->end - 1] == 0xFF)
	{	TREF(gv_last_subsc_null) = TRUE;			/* Trailing null subscript has NO trailing zero */
		gv_currkey->base[gv_currkey->end] = 0;
		gv_currkey->end += 1;
	}else
	{	TREF(gv_last_subsc_null) = FALSE;
		if (gv_currkey->base[gv_currkey->end - 1] == 0x01)
		{	dollar_order = TRUE;
			gv_currkey->base[gv_currkey->end] = 0;
			gv_currkey->end += 1;
		}
	}
	if (!gv_target->root && (cp1 != cp2))
	{
		qhdr->a.erc.value = RC_MUMERRUNDEFVAR;
 		rsp->size_return.value = 0;
		rsp->size_remain.value = 0;
		rsp->rstatus.value = 0;
		REVERT;
		return 0;
	}
	gv_currkey->base[gv_currkey->end] = 0;
	for (i = gv_currkey->end - 2; i > 0; i--)
		if (!gv_currkey->base[i - 1])
			break;
	gv_currkey->prev = i;

	if (fmode & RC_MODE_NEXT)
	{
		if (TREF(gv_last_subsc_null))
			gv_currkey->base[gv_currkey->prev] = 01;
		else {
			if (dollar_order)
			{	gv_currkey->end++;
				gv_currkey->base[gv_currkey->end] = 0;
			}else
			{
				gv_currkey->base[gv_currkey->end] = 1;
				gv_currkey->base[gv_currkey->end + 1] = 0;
				gv_currkey->base[gv_currkey->end + 2] = 0;
				gv_currkey->end += 2;
			}
		}
	}
	else if (fmode & RC_MODE_PREV)
	{
		if (TREF(gv_last_subsc_null))
		{
			*(&gv_currkey->base[0] + gv_currkey->prev + 1) = 0xFF;
			*(&gv_currkey->base[0] + gv_currkey->end + 1) = 0;
			gv_currkey->end += 1;
		}
	}
	rsp->size_return.value = 0;
	if (dollar_order && (cp1 == cp2))
	{
		rc_gbl_ord(rsp);
		REVERT;
		return 0;
	}
	if (!gv_target->root)
	{
		qhdr->a.erc.value = RC_BADXBUF;
 		rsp->size_return.value = 0;
		rsp->size_remain.value = 0;
		rsp->rstatus.value = 0;
		REVERT;
		return 0;
	}

	t_begin(ERR_GVGETFAIL, 0);
	for (;;)
	{
		rsp->hdr.a.len.value = (short)((char*)(&rsp->page[0]) - (char*)rsp);
		size_return = rc_size_return;
		two_histories = FALSE;
		if ((status = gvcst_search(gv_currkey, NULL)) == cdb_sc_normal)
		{
			bh = gv_target->hist.h;
			if (fmode & RC_MODE_NEXT)	/* if getnext */
			{
				rp = (rec_hdr *)(bh->buffaddr + bh->curr_rec.offset);
				bp = (blk_hdr *)bh->buffaddr;
				if (rp >= (rec_hdr *)CST_TOB(bp))
				{
					two_histories = TRUE;
					status = gvcst_rtsib(&second_history, 0);
					if (status == cdb_sc_normal)
						bh = second_history.h;
					else
						if (status == cdb_sc_endtree)
							two_histories = FALSE;	/* second history not valid */
						else
							goto restart;
				}
			}
			else
				if (fmode & RC_MODE_PREV)	/* if get previous */
				{
					if (bh->prev_rec.offset == 0)
					{
						two_histories = TRUE;
						status = gvcst_lftsib(&second_history);
						if (status == cdb_sc_normal)
							bh = second_history.h;
						else
							if (status == cdb_sc_endtree)
								two_histories = FALSE;	/* second history not valid */
							else
								goto restart;
					}
				}
			if ((bsiz = ((blk_hdr *)(bh->buffaddr))->bsiz + RC_BLKHD_PAD) > SIZEOF(blk_hdr) + RC_BLKHD_PAD)
			{	/* Non-empty block, global exists */
				if (bsiz > size_return)
					rc_overflow->size = bsiz - size_return;
				else
				{
					rc_overflow->size = 0;
					size_return = bsiz;
				}

				memcpy(rsp->page, bh->buffaddr, SIZEOF(blk_hdr));
				PUT_SHORT(&((blk_hdr*)rsp->page)->bsiz,bsiz);
				memcpy(rsp->page + SIZEOF(blk_hdr) + RC_BLKHD_PAD, bh->buffaddr + SIZEOF(blk_hdr),
					  size_return - (SIZEOF(blk_hdr) + RC_BLKHD_PAD));
				rsp->size_return.value = size_return;
				rsp->hdr.a.len.value += rsp->size_return.value;
				assert(rsp->hdr.a.len.value <= rc_size_return+SIZEOF(rc_rsp_page));
				rsp->zcode.value = (cs_data->blk_size / 512);	/* (2 ** zcode) == blk_size */
				if (rc_overflow->size)
				{
					memcpy(rc_overflow->buff, bh->buffaddr + size_return - RC_BLKHD_PAD, rc_overflow->size);
					rc_overflow->offset = size_return;
					rc_overflow->dsid = qhdr->r.xdsid.dsid.value;
					rc_overflow->page = bh->blk_num;
					rc_overflow->zcode = rsp->zcode.value;
				}
				rsp->size_remain.value = rc_overflow->size;
			}

			if (0 == (rc_read_stamp = t_end(&gv_target->hist, two_histories ? &second_history : NULL,
				TN_NOT_SPECIFIED)))
				continue;
			if (bsiz == SIZEOF(blk_hdr) + RC_BLKHD_PAD)	/* Empty block, global does not exist */
			{
				qhdr->a.erc.value = RC_MUMERRUNDEFVAR;
		 		rsp->size_return.value = 0;
				rsp->size_remain.value = 0;
				rsp->rstatus.value = 0;
				REVERT;
				return 0;
			}
			if (status == cdb_sc_endtree)
			{
				if (fmode & RC_MODE_NEXT)
				{	rsp->after.value = (unsigned short)-1;
					rsp->before.value = 0;
				}else
				{	rsp->after.value = 0;
					rsp->before.value = (unsigned short)-1;
				}
				rsp->xcc.value = 0;
			}else
			{
				if (two_histories)
				{
					rsp->after.value  = 0;
					rsp->before.value = 0;
					rsp->xcc.value	= 0;
				}else
				{
					if (fmode & RC_MODE_PREV)		/* getprev */
					{	rsp->before.value = bh->prev_rec.offset;
						if (rsp->before.value)
							rsp->before.value += RC_BLKHD_PAD;
						rsp->xcc.value	= bh->prev_rec.match;
						rsp->after.value  = bh->curr_rec.offset + RC_BLKHD_PAD;
					}else				/* if getnext or just get */
					{			/* If key not found, use prev and curr, else use curr and next */
						if (gv_currkey->end + 1 != bh->curr_rec.match)	/* not found */
						{	rsp->before.value = bh->prev_rec.offset;
							if (rsp->before.value)
								rsp->before.value += RC_BLKHD_PAD;
							rsp->xcc.value	= bh->prev_rec.match;
							rsp->after.value  = bh->curr_rec.offset + RC_BLKHD_PAD;
						}else	/* found */
						{	rsp->before.value = bh->curr_rec.offset;
							if (rsp->before.value)
								rsp->before.value += RC_BLKHD_PAD;
							rsp->xcc.value	= bh->curr_rec.match;
							rp = (rec_hdr *)(bh->buffaddr + bh->curr_rec.offset);
							GET_SHORT(rsiz, &rp->rsiz);
							rsp->after.value  = bh->curr_rec.offset + rsiz + RC_BLKHD_PAD;
						}
					}
				}
			}
			PUT_LONG(rsp->pageaddr, bh->blk_num);
			rsp->frag_offset.value   = 0;
			rsp->rstatus.value = 0;

			REVERT;
			return 0;
		}
restart:	t_retry(status);
	}

}
