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

#ifndef MLK_SHR_INIT_DEFINED

/* Declare parms for mlk_shr_init.c */

void mlk_shr_init(sm_uc_ptr_t base,
		  int4 size,
		  sgmnt_addrs *csa,
		  boolean_t read_write);


#define MLK_SHR_INIT_DEFINED

#endif
