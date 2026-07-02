/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TARG_ALLOC_INCLUDED
#define TARG_ALLOC_INCLUDED

gv_namehead	*targ_alloc(int keysize, unmanaged_mname_entry *gvent, gd_region *reg);
void		targ_free(gv_namehead *gvt);

#endif /* TARG_ALLOC_INCLUDED */
