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
#include "rc_oflow.h"
#include "gtcm.h"
#include "gvcst_protos.h"	/* for gvcst_put prototype */

GBLREF rc_oflow	*rc_overflow;
GBLREF gv_key 		*gv_currkey;
GBLREF gv_namehead 	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr		*gd_header;
GBLREF int		gv_keysize;
GBLREF sgmnt_data	*cs_data;

int rc_prc_set(rc_q_hdr *qhdr)
{
    rc_set	*req, *rsp;
    rc_swstr	*data;
    short	len;
    int		 i;
    mval	 v;
    char	*cp1;

    ESTABLISH_RET(rc_dbms_ch, RC_SUCCESS);
    if ((qhdr->a.erc.value = rc_fnd_file(&qhdr->r.xdsid)) != RC_SUCCESS)
    {
	REVERT;
#ifdef DEBUG
	gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"rc_fnd_file.");
#endif
	return -1;
    }
assert(rc_overflow->buff != 0);

    rsp = req = (rc_set *)qhdr;
    if (req->key.len.value > cs_data->max_key_size)
    {
	qhdr->a.erc.value = RC_KEYTOOLONG;
	REVERT;
#ifdef DEBUG
	gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"RC_KEYTOOLONG.");
#endif
	return -1;
    }
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
assert(rc_overflow->buff != 0);
    gv_currkey->end = req->key.len.value;
    gv_currkey->base[gv_currkey->end] = 0;
    for (i = gv_currkey->end - 2; i > 0; i--)
	if (!gv_currkey->base[i - 1])
	    break;
    gv_currkey->prev = i;
    data = (rc_swstr *)(req->key.key + req->key.len.value);
    GET_SHORT (len, &data->len.value);
    v.str.len = len;
    v.str.addr = data->str;
    v.mvtype = MV_STR;
    if (gv_currkey->end + 1 + v.str.len + SIZEOF(rec_hdr) > gv_cur_region->max_rec_size)
    {
	qhdr->a.erc.value = RC_KEYTOOLONG;
	REVERT;
#ifdef DEBUG
	gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"RC_KEYTOOLONG.");
#endif
	return -1;
    }

assert(rc_overflow->buff != 0);
    gvcst_put(&v);
assert(rc_overflow->buff != 0);

    REVERT;
    return 0;

}
