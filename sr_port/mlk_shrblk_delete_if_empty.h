/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MLK_SHRBLK_DELETE_IF_EMPTY_INCLUDED
#define MLK_SHRBLK_DELETE_IF_EMPTY_INCLUDED

#include "mmrhash.h"

boolean_t mlk_shrblk_delete_if_empty(mlk_pvtctl_ptr_t pctl, mlk_shrblk_ptr_t d);
void mlk_shrhash_val_build(mlk_shrblk_ptr_t d, uint4 *total_len, mlk_subhash_state_t *hs);

#endif /* MLK_SHRBLK_DELETE_IF_EMPTY_INCLUDED */
