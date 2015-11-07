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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include <rmsdef.h>
#include <fab.h>
#include <nam.h>
#include "filestruct.h"
#include "mu_cre_vms_structs.h"

void mu_cre_vms_structs(gd_region *reg)
{
	vms_gds_info	*mm_info;
	gd_segment	*seg;

	assert(reg->dyn.addr->acc_meth == dba_bg || reg->dyn.addr->acc_meth == dba_mm);
	seg = reg->dyn.addr;
	seg->file_cntl = malloc(SIZEOF(file_control));
	switch (reg->dyn.addr->acc_meth)
	{
		case dba_mm:
		case dba_bg:
			seg->file_cntl->file_info = malloc(SIZEOF(vms_gds_info));
			memset(seg->file_cntl->file_info,0,SIZEOF(vms_gds_info));
			mm_info = seg->file_cntl->file_info;
			mm_info->fab = malloc(SIZEOF(struct FAB));
			mm_info->nam = malloc(SIZEOF(struct NAM));
			*mm_info->fab = cc$rms_fab;
			*mm_info->nam = cc$rms_nam;
			mm_info->fab->fab$l_nam = mm_info->nam;
			mm_info->fab->fab$l_alq = seg->allocation;
			mm_info->fab->fab$l_fna = seg->fname;
			mm_info->fab->fab$l_dna = seg->defext;
			mm_info->fab->fab$b_fns = seg->fname_len;
			mm_info->fab->fab$b_dns = SIZEOF(seg->defext);
			mm_info->fab->fab$w_mrs = reg->max_rec_size;
			mm_info->fab->fab$w_bls = reg->max_rec_size;
			mm_info->fab->fab$w_deq = seg->ext_blk_count;
			mm_info->fab->fab$b_org = FAB$C_SEQ;
			mm_info->fab->fab$b_rfm = FAB$C_FIX;
			mm_info->fab->fab$l_fop = FAB$M_UFO | FAB$M_CIF | FAB$M_CBT;
			mm_info->fab->fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_BIO;
			mm_info->fab->fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
			break;
	}
	return;
}
