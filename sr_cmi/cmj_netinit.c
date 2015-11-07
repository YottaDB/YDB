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

#include <descrip.h>
#include <dvidef.h>
#include "cmihdr.h"
#include "cmidef.h"

GBLDEF $DESCRIPTOR(cm_netname,"_NET:");
GBLDEF struct NTD *ntd_root;

/* CLEAN-UP: 1. replace CMI_CMICHECK with new message
	2. find correct size and location of MBX_SIZE
*/

#define MBX_SIZE 256

uint4 cmj_netinit()
{
	error_def(CMI_CMICHECK);
	uint4 status;
	int4 maxmsg,bufquo;	/* lib$asn_wth_mbx uses longwords by reference for these items */
	struct NTD *tsk;
	unsigned char mailbox_name_buffer[128];
	short unsigned mbx_name_length;
	uint4 long_mnl;

	if (ntd_root)
		return CMI_CMICHECK;
	lib$get_vm(&SIZEOF(*ntd_root), &ntd_root, 0);
	tsk = ntd_root;
	memset(tsk, 0, SIZEOF(*tsk));
	tsk->mbx.dsc$w_length = MBX_SIZE;
	tsk->mbx.dsc$b_dtype = DSC$K_DTYPE_T;
	tsk->mbx.dsc$b_class = DSC$K_CLASS_S;
	lib$get_vm(&MBX_SIZE, &tsk->mbx.dsc$a_pointer, 0);
	maxmsg = tsk->mbx.dsc$w_length;
	bufquo = maxmsg;
	status = lib$asn_wth_mbx(&cm_netname, &maxmsg, &bufquo, &(tsk->dch), &(tsk->mch));
	if ((status & 1) == 0)
		return status;
	tsk->mnm.dsc$w_length = SIZEOF(mailbox_name_buffer);
	tsk->mnm.dsc$b_dtype = DSC$K_DTYPE_T;
	tsk->mnm.dsc$b_class = DSC$K_CLASS_S;
	tsk->mnm.dsc$a_pointer = mailbox_name_buffer;
	status = lib$getdvi(&DVI$_FULLDEVNAM, &tsk->mch,0,0,&tsk->mnm,&mbx_name_length);
	if ((status & 1) == 0)
		return status;
	long_mnl = mbx_name_length;
	lib$get_vm(&long_mnl, &tsk->mnm.dsc$a_pointer, 0);
	tsk->mnm.dsc$w_length = mbx_name_length;
	memcpy(tsk->mnm.dsc$a_pointer, mailbox_name_buffer, mbx_name_length);
	return status;
}
