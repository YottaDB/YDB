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
#include <iodef.h>
#include <efndef.h>
#ifdef __Alpha_AXP
#include nfbdef
#else
#include <nfbdef.h>
#endif
#include "cmihdr.h"
#include "cmidef.h"

GBLREF struct NTD *ntd_root;

uint4 cmi_init(cmi_descriptor *tnd, unsigned char tnr, void (*err)(), void (*crq)(), bool (*acc)())
{
	uint4 status;

#ifdef __Alpha_AXP
# pragma member_alignment save
# pragma nomember_alignment
#endif

	struct
	{
		unsigned char operation;
		int4 object_number;
	} nfb;

#ifdef __Alpha_AXP
# pragma member_alignment restore
#endif

	struct dsc$descriptor_s nfb_desc;
	qio_iosb iosb;
	error_def(CMI_CMICHECK);

	status = cmj_netinit();
	if ((status & 1) == 0)
		return status;
	nfb_desc.dsc$w_length = SIZEOF(nfb);
	nfb_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	nfb_desc.dsc$b_class = DSC$K_CLASS_S;
	nfb_desc.dsc$a_pointer = &nfb;
	if (tnr != 0)
	{
		nfb.operation = NFB$C_DECLOBJ;
		nfb.object_number = tnr;
		if (0 != tnd)
			return CMI_CMICHECK;
	} else
	{
		if (0 == tnd)
			return CMI_CMICHECK;
		nfb.operation = NFB$C_DECLNAME;
		nfb.object_number = 0;
	}
        status = sys$qiow(EFN$C_ENF, ntd_root->dch, IO$_ACPCONTROL, &iosb, 0, 0, &nfb_desc, tnd, 0, 0, 0, 0);
	if (status & 1)
		status = iosb.status;
	if ((status & 1) == 0)
		return status;
	ntd_root->err = err;
	ntd_root->crq = crq;
	ntd_root->acc = acc;
	status = cmj_mbx_read_start(ntd_root);
	return status;
}
