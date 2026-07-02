/****************************************************************
 *								*
 * Copyright (c) 2026 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef DEVICE_CLEANUP_H_INCLUDED
#define DEVICE_CLEANUP_H_INCLUDED
#include "gtm_common_defs.h"
#include "gcol_list.h"
#include "mdef.h"
#include "io.h"
#include "iormdef.h"

static inline void cleanup_device_stp_residents(io_desc *iod)
{
	GBLREF io_pair	io_std_device;
	d_rm_struct *rm_ptr;

	glist_unprotect_str(&iod->error_handler);
	iod->error_handler.len = 0;
	iod->error_handler.addr = NULL;
	rm_ptr = (rm == iod->type) ? (d_rm_struct *)iod->dev_sp : NULL;
	if (!rm_ptr)
		return;
	if (rm_ptr->input_encrypted)
	{
		glist_unprotect_str(&rm_ptr->input_iv);
		glist_unprotect_str(&rm_ptr->input_key);
	} else
	{
		assert(!glist_str_protected(&rm_ptr->input_iv));
		assert(!glist_str_protected(&rm_ptr->input_key));
	}
	if (rm_ptr->output_encrypted)
	{
		glist_unprotect_str(&rm_ptr->output_iv);
		glist_unprotect_str(&rm_ptr->output_key);
	} else
	{
		assert(!glist_str_protected(&rm_ptr->output_iv));
		assert(!glist_str_protected(&rm_ptr->output_key));
	}
}
#endif
