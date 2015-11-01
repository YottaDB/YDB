/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ASWP_H_INCLUDED
#define ASWP_H_INCLUDED

#ifdef __hppa

int aswp3(sm_int_ptr_t /* location */, int4 /* value */, sm_global_latch_ptr_t /* latch */);
#define ASWP(A,B,C)     aswp3((sm_int_ptr_t)(A), B, C)


#else

int aswp(sm_int_ptr_t /* location */, int4 /* value */);
int aswp_secshr(sm_int_ptr_t /* location */, int4 /* value */);
#define ASWP(A,B,C)     aswp((sm_int_ptr_t)(A), B)

#endif

#endif

