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

#ifndef INIT_SEC_INCLUDED
#define INIT_SEC_INCLUDED

uint4 init_sec(uint4 *retadr, struct dsc$descriptor_s *gsdnam, uint4 chan, uint4 pagcnt, uint4 flags);

#endif /* INIT_SEC_INCLUDED */
