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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "filestruct.h"
#include "jnl.h"
#include "cmidef.h"
#include "cmmdef.h"

GBLREF connection_struct *curr_entry;
GBLREF unsigned short procnum;
GBLDEF unsigned int total_process_init = 0;     /* so can look at w/ debugger */
GBLREF struct NTD *ntd_root;
GBLREF struct CLB *proc_to_clb[];	/* USHRT_MAX + 1 so procnum can wrap */

bool gtcmtr_initproc()
{
	unsigned char *reply;
        unsigned short beginprocnum;
        struct CLB *gtcm_find_proc(struct NTD *, unsigned short);
        size_t jpv_size;

	error_def(CMERR_INVPROT);
        error_def(ERR_TOOMANYCLIENTS);

	reply = curr_entry->clb_ptr->mbf;
	assert(*reply == CMMS_S_INITPROC);
	reply++;
	if (memcmp(reply,S_PROTOCOL, sizeof(S_PROTOCOL) - 1))
		rts_error(VARLSTCNT(1) CMERR_INVPROT);

	curr_entry->pvec = (jnl_process_vector *)malloc(sizeof(jnl_process_vector));
        jpv_size = sizeof(jnl_process_vector);
        if (jpv_size > (curr_entry->clb_ptr->cbl - S_HDRSIZE - S_PROTSIZE))
	{	/* our jpv is larger than client so limit copy and pad */
                jpv_size = curr_entry->clb_ptr->cbl - S_HDRSIZE - S_PROTSIZE;
		assert(0 <= jpv_size);	/* just to catch an ACCVIO in the line below, remove this assert once it is resolved */
                memset((char *)curr_entry->pvec + jpv_size, 0, sizeof(jnl_process_vector) - jpv_size);
        }
	reply = curr_entry->clb_ptr->mbf;
        memcpy(curr_entry->pvec, reply + S_HDRSIZE + S_PROTSIZE, jpv_size);
	*reply = CMMS_T_INITPROC;
	reply += S_HDRSIZE + S_PROTSIZE;
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
	*(short*)reply = procnum++;
	curr_entry->clb_ptr->cbl = S_HDRSIZE + S_PROTSIZE + 2;
	return CM_WRITE;
}
