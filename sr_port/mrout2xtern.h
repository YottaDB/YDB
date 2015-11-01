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

#ifndef MROUT2XTERN_INCLUDED
#define MROUT2XTERN_INCLUDED

#ifdef VMS
#	define MROUT2XTERN(src, dst, len)	lower_to_upper((uchar_ptr_t)(dst), (uchar_ptr_t)(src), len)
#else
#	define MROUT2XTERN(src, dst, len)	memcpy(dst, src, len)
#endif

#endif /* MROUT2XTERN_INCLUDED */
