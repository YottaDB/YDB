/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __UTIL_FORMAT_H__
#define __UTIL_FORMAT_H__

#ifdef UNIX
caddr_t util_format(caddr_t message, va_list fao, caddr_t buff, ssize_t size, int faocnt);
#elif defined(VMS)
caddr_t util_format(caddr_t message, va_list fao, caddr_t buff, int4 size);
#else
#error Unsupported Platform
#endif

#endif
