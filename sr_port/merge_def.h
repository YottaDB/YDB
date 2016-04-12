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

#ifndef MERGE_DEF_DEFINED
#define MARG1_LCL 1
#define MARG1_GBL 2
#define MARG2_LCL 4
#define MARG2_GBL 8
#define IND1 0
#define IND2 1
#define MARG1_IS_LCL(arg) (arg & MARG1_LCL)
#define MARG1_IS_GBL(arg) (arg & MARG1_GBL)
#define MARG2_IS_LCL(arg) (arg & MARG2_LCL)
#define MARG2_IS_GBL(arg) (arg & MARG2_GBL)
#define MERGE_DEF_DEFINED
#endif
