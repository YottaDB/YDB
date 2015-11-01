/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef INCR_LINK_INCLUDED
#define INCR_LINK_INCLUDED

#ifdef USHBIN_SUPPORTED
#include "incr_link_sp.h"
bool incr_link(int file_desc, zro_ent *zro_entry);
#else
bool incr_link(int file_desc);
#endif

#endif /* INCR_LINK_INCLUDED */
