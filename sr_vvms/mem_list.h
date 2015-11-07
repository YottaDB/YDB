/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __MEM_LIST_H__
#define __MEM_LIST_H__

void	add_link(int4 size, int4 inadr);
boolean_t is_va_free(uint4 outaddrs);
uint4	gtm_expreg(uint4 size, uint4 *inadr, uint4 acmode, uint4 region);
uint4	gtm_expreg_noaccess_check(uint4 size, uint4 *inadr, uint4 acmode, uint4 region);
uint4	gtm_deltva(uint4 *outaddrs, uint4 *retadr, uint4 acmode);

#endif
