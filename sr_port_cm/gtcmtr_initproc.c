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

#include "copy.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "filestruct.h"
#include "jnl.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_find_proc.h"
#include "gtcmtr_protos.h"
#include "gtcm_protocol.h"
#include "gtcm_is_query_queryget.h"
#include "gtcm_err_compat.h"		/* for gtcm_err_compat() prototype */
#ifdef VMS
#include "jpv_v10to12.h"
#endif

GBLREF connection_struct *curr_entry;
GBLREF unsigned short procnum;
GBLDEF unsigned int total_process_init = 0;     /* so can look at w/ debugger */
GBLREF struct NTD *ntd_root;
GBLREF struct CLB *proc_to_clb[];	/* USHRT_MAX + 1 so procnum can wrap */
GBLREF jnl_process_vector *originator_prc_vec;

bool gtcmtr_initproc(void)
{
	unsigned char *reply;
        unsigned short beginprocnum;
        size_t jpv_size;
	protocol_msg myproto;

	error_def(CMERR_INVPROT);
        error_def(ERR_TOOMANYCLIENTS);

	reply = curr_entry->clb_ptr->mbf;
	assert(*reply == CMMS_S_INITPROC);
	reply++;
	gtcm_protocol(&myproto);
	if (!gtcm_protocol_match((protocol_msg *)reply, &myproto))
		rts_error(VARLSTCNT(1) CMERR_INVPROT);
	curr_entry->query_is_queryget = gtcm_is_query_queryget((protocol_msg *)reply, &myproto);
	curr_entry->err_compat = gtcm_err_compat((protocol_msg *)reply, &myproto);
	curr_entry->cli_supp_allowexisting_stdnullcoll = (0 <= memcmp(reply + CM_LEVEL_OFFSET, CMM_STDNULLCOLL_MIN_LEVEL, 3));
	curr_entry->client_supports_long_names = (0 <= memcmp(reply + CM_LEVEL_OFFSET, CMM_LONGNAMES_MIN_LEVEL, 3));
	originator_prc_vec = curr_entry->pvec = (jnl_process_vector *)malloc(SIZEOF(jnl_process_vector));
        jpv_size = SIZEOF(jnl_process_vector);
	assert(jpv_size >= curr_entry->clb_ptr->cbl - S_HDRSIZE - S_PROTSIZE &&
		S_HDRSIZE + S_PROTSIZE < curr_entry->clb_ptr->cbl);
        if (jpv_size > (curr_entry->clb_ptr->cbl - S_HDRSIZE - S_PROTSIZE))
	{	/* our jpv is larger than client so limit copy and pad */
                jpv_size = curr_entry->clb_ptr->cbl - S_HDRSIZE - S_PROTSIZE;
                memset((char *)originator_prc_vec + jpv_size, 0, SIZEOF(jnl_process_vector) - jpv_size);
        }
	reply = curr_entry->clb_ptr->mbf;
        memcpy((unsigned char *)originator_prc_vec, reply + S_HDRSIZE + S_PROTSIZE, jpv_size);
	*reply = CMMS_T_INITPROC;
	reply += S_HDRSIZE;
	if (UNIX_ONLY(TRUE) VMS_ONLY(0 < memcmp(&((protocol_msg *)reply)->msg[CM_LEVEL_OFFSET], CMM_MIN_PEER_LEVEL, 3)))
	{ /* note, protocol string in mbf hasn't been overwritten yet */
		memcpy(reply, &myproto, S_PROTSIZE);
#ifdef BIGENDIAN
		originator_prc_vec->jpv_pid =  GTM_BYTESWAP_32(originator_prc_vec->jpv_pid);
		originator_prc_vec->jpv_image_count =  GTM_BYTESWAP_32(originator_prc_vec->jpv_image_count);
		originator_prc_vec->jpv_time =  GTM_BYTESWAP_64(originator_prc_vec->jpv_time);
		originator_prc_vec->jpv_login_time =  GTM_BYTESWAP_64(originator_prc_vec->jpv_login_time);
#endif
	} else
	{ /* VMS client is "old" version, send "old" version protocol string */
		memcpy(reply, S_PROTOCOL, S_PROTSIZE);
		VMS_ONLY(jpv_v10to12((char *)originator_prc_vec, originator_prc_vec);)
	}
	reply += S_PROTSIZE;
        total_process_init++;		/* count attempts */
        beginprocnum = procnum;         /* so stop on wrap around */
        while (NULL != proc_to_clb[procnum])
        {
		procnum++;	/* OK to wrap since proc_to_clb is proper size */
		if (beginprocnum == procnum)
			rts_error(VARLSTCNT(1) ERR_TOOMANYCLIENTS);
	}
	curr_entry->procnum = procnum;
        proc_to_clb[procnum] = curr_entry->clb_ptr;
	PUT_SHORT(reply, procnum);
	procnum++;
	curr_entry->clb_ptr->cbl = S_HDRSIZE + S_PROTSIZE + 2;
	return CM_WRITE;
}
