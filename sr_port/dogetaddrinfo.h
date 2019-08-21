/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _DOGETADDRINFO_INCLUDED

int dogetaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);

#define _DOGETADDRINFO_INCLUDED
#endif
