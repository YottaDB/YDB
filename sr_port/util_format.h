/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef UTIL_FORMAT_H_INCLUDED
#define UTIL_FORMAT_H_INCLUDED

#ifdef UNIX
caddr_t util_format(caddr_t message, va_list fao, caddr_t buff, ssize_t size, int faocnt);
#elif defined(VMS)
caddr_t util_format(caddr_t message, va_list fao, caddr_t buff, int4 size);
#else
#error Unsupported Platform
#endif

#endif
