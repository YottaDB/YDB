/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef INCR_LINK_INCLUDED
#define INCR_LINK_INCLUDED

NON_USHBIN_ONLY(bool incr_link(int file_desc);)
USHBIN_ONLY(bool incr_link(int file_desc, zro_ent *zro_entry);)

#endif /* INCR_LINK_INCLUDED */
