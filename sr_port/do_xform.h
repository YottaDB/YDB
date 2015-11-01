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

#ifndef DO_XFORM_INCLUDED
#define DO_XFORM_INCLUDED

void do_xform(collseq *csp, int fc_type, mstr *input, mstr *output, int *length);
/*
 * fc_type would be either XFORM (0) or XBACK (1)
 */
#endif /* DO_XFORM_INCLUDED */
