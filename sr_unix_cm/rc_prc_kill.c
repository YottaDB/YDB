/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "gvcst_protos.h"	/* for gvcst_kill prototype */
#include "hashtab_mname.h"

GBLREF gv_key 		*gv_currkey;
GBLREF gv_namehead 	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr		*gd_header;
GBLREF int		gv_keysize;
GBLREF sgmnt_data	*cs_data;

int rc_prc_kill(rc_q_hdr *qhdr)
{
    rc_kill	*req;
    rc_kill	*rsp;
    int		 i;
    mname_entry	gvname;
    char	*cp1;
    gvnh_reg_t	*gvnh_reg;

    ESTABLISH_RET(rc_dbms_ch, RC_SUCCESS);
    if ((qhdr->a.erc.value = rc_fnd_file(&qhdr->r.xdsid)) != RC_SUCCESS)
    {
	REVERT;
#ifdef DEBUG
	gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"rc_fnd_file failed.");
#endif
	return -1;
    }

    rsp = req = (rc_kill *)qhdr;
    if (req->key.len.value > cs_data->max_key_size)
    {	qhdr->a.erc.value = RC_KEYTOOLONG;
	REVERT;
#ifdef DEBUG
	gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"RC_KEYTOOLONG.");
#endif
	return -1;
    }
    for (cp1 = req->key.key; *cp1; cp1++)
	;
    gvname.var_name.len = INTCAST(cp1 - req->key.key);
    gvname.var_name.addr = req->key.key;
	if (gvname.var_name.len > 8)	/* GT.M does not support global variables > 8 chars */
	{	qhdr->a.erc.value = RC_KEYTOOLONG;
		REVERT;
#ifdef DEBUG
	gtcm_cpktdmp((char *)qhdr,qhdr->a.len.value,"RC_KEYTOOLONG.");
#endif
		return -1;
	}
	COMPUTE_HASH_MNAME(&gvname);
	GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, &gvname, gvnh_reg);
	assert(NULL == gvnh_reg->gvspan); /* so GV_BIND_SUBSNAME_IF_GVSPAN is not needed */
	memcpy(gv_currkey->base, req->key.key, req->key.len.value);
	gv_currkey->end = req->key.len.value;
	gv_currkey->base[gv_currkey->end] = 0;
	for (i = gv_currkey->end - 2; i > 0; i--)
	{
		if (!gv_currkey->base[i - 1])
			break;
	}
	gv_currkey->prev = i;
	if (gv_target->root)
	    gvcst_kill(TRUE);
	REVERT;
	return 0;
}
