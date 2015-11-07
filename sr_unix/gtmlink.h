/****************************************************************
 *								*
 *	Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTMLINK_H_INCLUDED
#define GTMLINK_H_INCLUDED

enum gtm_link_type
{
	LINK_NORECURSIVE = 0,
	LINK_RECURSIVE,
	LINK_MAXTYPE
};

void init_relink_allowed(mstr *keyword);
#ifdef DEBUG
void check_max_keyword_len(void);
#endif

#endif /* GTMLINK_H_INCLUDED */
