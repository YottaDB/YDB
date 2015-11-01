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

#ifndef T_QREAD_DEFINED

/* Declare parms for t_qread.c */

sm_uc_ptr_t t_qread (block_id blk, sm_int_ptr_t cycle, cache_rec_ptr_ptr_t cr_out);

/* cycle is used in t_end to detect if the buffer has been refreshed since the t_qread */

#define T_QREAD_DEFINED

#endif
