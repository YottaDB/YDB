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

#ifndef T_BEGIN_DEFINED
#define T_BEGIN_DEFINED

void t_begin (uint4 err, boolean_t update_transaction);	/* should be called only via T_BEGIN_xxx_NONTP_OR_TP macros if in TP */

#endif
