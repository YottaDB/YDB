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

#include "t_begin.h"
#include "t_end.h"
#include "t_qread.h"

GBLREF rc_oflow	*rc_overflow;
GBLREF int		rc_size_return;
GBLREF sgmnt_data	*cs_data;
GBLREF sgmnt_addrs	*cs_addrs;

int
rc_prc_getp(rc_q_hdr *qhdr)
{
    rc_req_getp	*req;
    rc_rsp_page	*rsp;
    int			 ret_size;
    short		 offset, dsid, size_return;
    int4		 pageaddr;
    srch_hist		 targ_hist;
    short		 bsiz;
    unsigned char	*buffaddr;
    error_def(ERR_DSEBLKRDFAIL);

    ESTABLISH_RET(rc_dbms_ch,0);
    req  = (rc_req_getp *)qhdr;
    rsp = (rc_rsp_page *)qhdr;
    rsp->hdr.a.len.value = (short)((char*)(&rsp->page[0]) - (char*)rsp);

    GET_LONG(pageaddr, req->pageaddr);
    offset      = req->offset.value;		/* oFragmentA */
    dsid        = qhdr->r.xdsid.dsid.value;

    if (offset)
    {
	    rsp->hdr.a.len.value = (short)((char*)(&rsp->page[0])
					   - (char*)rsp);
	    size_return = rc_size_return;

    /*  NOTE:  If the request is for a fragment (i.e. Not the start of a block)
     *  then the fragment must reside in the overflow structure for this
     *  connection.  If not, it is considered an error */
	    if ( (pageaddr != rc_overflow->page)
		|| (dsid != rc_overflow->dsid)
		|| !rc_overflow->size
		|| rc_overflow->buff == 0)
	    {
		    qhdr->a.erc.value = RC_NETERRRETRY;
		    ((rc_rsp_page*)qhdr)->size_return.value = 0;
		    REVERT;
#ifdef DEBUG
		    gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"RC_NETERRRETRY.");
#endif
		    return -1;
	    }

	    rsp->rstatus.value = 0;
	    if (size_return >= (rc_overflow->offset + rc_overflow->size) - offset) {
		    rsp->size_return.value = (rc_overflow->offset
					      + rc_overflow->size) - offset;
		    rsp->size_remain.value = 0;
	    }
	    else
	    {
		    rsp->size_return.value = size_return;
		    rsp->size_remain.value = (rc_overflow->offset +
			    rc_overflow->size) - (offset + size_return);
	    }

	    memcpy(rsp->page, rc_overflow->buff + (offset - rc_overflow->offset), size_return);
	    rsp->zcode.value = rc_overflow->zcode;
	    qhdr->a.erc.value = RC_SUCCESS;
	    rsp->hdr.a.len.value += rsp->size_return.value;

	    REVERT;
	    return 0;
    }
    else  /* offset == 0 */
    {
	    if ((qhdr->a.erc.value = rc_fnd_file(&qhdr->r.xdsid)) != RC_SUCCESS)
	    {
		    REVERT;
#ifdef DEBUG
		    gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"rc_fnd_file failed.");
#endif
		    return -1;
	    }

	    t_begin(ERR_DSEBLKRDFAIL, 0);
	    do
	    {
		    rsp->hdr.a.len.value = (short)((char*)(&rsp->page[0])
						   - (char*)rsp);
		    size_return = rc_size_return;

		    targ_hist.h[0].tn = cs_addrs->ti->curr_tn;
		    if (!(buffaddr = t_qread(pageaddr,&targ_hist.h[0].cycle,
					     &targ_hist.h[0].cr)))
		    {
			    /* read failure */
			    qhdr->a.erc.value = RC_GLOBERRPAGENOTFOUND;
			    return -1;
		    }

		    bsiz = (((blk_hdr *) buffaddr)->bsiz + RC_BLKHD_PAD);
		    if (bsiz > size_return)
			    rc_overflow->size = bsiz - size_return;
		    else
		    {
			    rc_overflow->size = 0;
			    size_return = bsiz;
		    }


		    /* block won't fit into response packet */
		    if (rc_size_return+SIZEOF(rc_rsp_page)
			< size_return + rsp->hdr.a.len.value)
		    {
			    qhdr->a.erc.value = RC_XBLKOVERFLOW;
			    return -1;
		    }

		    /* copy header block */
		    memcpy(rsp->page, buffaddr, SIZEOF(blk_hdr));
		    /* increase size field to include RC_BLKHD_PAD */
		    PUT_SHORT(&((blk_hdr*)rsp->page)->bsiz,bsiz);
		    memcpy(rsp->page + SIZEOF(blk_hdr) + RC_BLKHD_PAD,
			   buffaddr + SIZEOF(blk_hdr),
			   size_return - (SIZEOF(blk_hdr) + RC_BLKHD_PAD));
		    rsp->size_return.value = size_return;
		    rsp->hdr.a.len.value += rsp->size_return.value;

		    /* (2 ** zcode) == blk_size */
		    rsp->zcode.value = (cs_data->blk_size / 512);

		    if (rc_overflow->size)
		    {
			    memcpy(rc_overflow->buff,
				   buffaddr + size_return - RC_BLKHD_PAD,
				   rc_overflow->size);
			    rc_overflow->offset = size_return;
			    rc_overflow->dsid = qhdr->r.xdsid.dsid.value;
			    rc_overflow->page = pageaddr;
			    rc_overflow->zcode = rsp->zcode.value;
		    }
		    rsp->size_remain.value = rc_overflow->size;

		    targ_hist.depth = 0;
		    targ_hist.h[0].buffaddr = buffaddr;
		    targ_hist.h[1].blk_num = 0;
	    }
	    while ( !t_end( &targ_hist, NULL, TN_NOT_SPECIFIED) );

	    qhdr->a.erc.value = RC_SUCCESS;
	    return 0;
    }
}


