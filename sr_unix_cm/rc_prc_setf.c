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

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "rc.h"
#include "copy.h"
#include "error.h"
#include "gtcm.h"
#include "gvcst_protos.h"	/* for gvcst_put prototype */

GBLREF gv_key 		*gv_currkey;
GBLREF gv_namehead 	*gv_target;
GBLREF int		rc_set_fragment;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr		*gd_header;

int rc_prc_setf(rc_q_hdr *qhdr)
{
    rc_set	*req, *rsp;
    short	 data_off, str_remain, *ptr;
    char	*cp1;
    int		 i;
    mval	 v;

    ESTABLISH_RET(rc_dbms_ch, RC_SUCCESS);
    if ((qhdr->a.erc.value = rc_fnd_file(&qhdr->r.xdsid)) != RC_SUCCESS)
    {
	REVERT;
#ifdef DEBUG
	gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"rc_fnd_file failed.");
#endif
	return -1;
    }
    rsp = req = (rc_set *)qhdr;
    v.mvtype = MV_STR;
    for (cp1 = req->key.key; *cp1; cp1++)
	;
    v.str.len = INTCAST(cp1 - req->key.key);
    v.str.addr = req->key.key;
	if (v.str.len > 8)	/* GT.M does not support global variables > 8 chars */
	{	qhdr->a.erc.value = RC_KEYTOOLONG;
		REVERT;
#ifdef DEBUG
	gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"RC_KEYTOOLONG.");
#endif
		return -1;
	}
	GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, &v.str);
    memcpy(gv_currkey->base, req->key.key, req->key.len.value);
    gv_currkey->end = req->key.len.value;
    gv_currkey->base[gv_currkey->end] = 0;
    for (i = gv_currkey->end - 2; i > 0; i--)
	if (!gv_currkey->base[i - 1])
	    break;
    gv_currkey->prev = i;

    ptr = (short*)(req->key.key + req->key.len.value);
    GET_SHORT(v.str.len, ptr);
    ptr++;
    GET_SHORT(data_off ,ptr);
    ptr++;
    GET_SHORT(str_remain ,ptr);
    ptr++;
    if (gv_currkey->end + 1 + v.str.len + SIZEOF(rec_hdr) > gv_cur_region->max_rec_size)
    {
	qhdr->a.erc.value = RC_KEYTOOLONG;
#ifdef DEBUG
	gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"RC_KEYTOOLONG.");
#endif
    }
    else  /* the total record will fit into a block */
    {
	if (rc_set_fragment = data_off) /* just send fragment */
	{
	    v.str.len = v.str.len - data_off - str_remain;
	    v.str.addr = (char*)ptr;
	}
	else			/* first fragment, put whole record, with zero filler */
	{
	    v.str.addr = (char*)malloc(v.str.len);
	    memset(v.str.addr, 0, v.str.len);
	    memcpy(v.str.addr + data_off, ptr, v.str.len - data_off - str_remain);
	}
	v.mvtype = MV_STR;
	gvcst_put(&v);

	if (rc_set_fragment)
	    rc_set_fragment = 0;
	else
	    free(v.str.addr);
    }

    REVERT;
    return ((qhdr->a.erc.value == RC_SUCCESS) ? 0 : -1);

}
