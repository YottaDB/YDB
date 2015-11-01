/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef COMPSWAP_H_INCLUDE
#define COMPSWAP_H_INCLUDE

#ifdef UNIX
boolean_t compswap(sm_global_latch_ptr_t lock, int compval, int newval1);
boolean_t compswap_secshr(sm_global_latch_ptr_t lock, int compval, int newval1);
#define COMPSWAP(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)	compswap(LCK, CMPVAL1, NEWVAL1)
#else
boolean_t compswap(sm_global_latch_ptr_t lock, int compval1, int compval2, int newval1, int newval2);
boolean_t compswap_secshr(sm_global_latch_ptr_t lock, int compval1, int compval2, int newval1, int newval2);
#define COMPSWAP(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)	compswap(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)
#endif

#endif
