/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __JNL_WRITE_PBLK_H__
#define __JNL_WRITE_PBLK_H__

/* We do not put this in jnl.h, because it needs all including jnl.h must include gdsblk.h */
void jnl_write_pblk(sgmnt_addrs *csa, cw_set_element *cse, uint4 com_csum);

#endif

