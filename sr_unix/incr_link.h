/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef INCR_LINK_INCLUDED
#define INCR_LINK_INCLUDED

#define IL_DONE		1	/* calling convention descended from VMS */
#define IL_RECOMPILE	0

#ifdef USHBIN_SUPPORTED
#include <incr_link_sp.h>
#endif
#include "zroutinessp.h"	/* need zro_ent typedef */

/* If autorelink is supported, the additional recent_zhist parameter (if non-NULL) points to a malloc'd search history buffer
 * which should be* released for any error. This macro provides the release capability.
 */
#ifdef AUTORELINK_SUPPORTED
#  define RECENT_ZHIST recent_zhist
#  define RELEASE_RECENT_ZHIST 					\
{								\
	if (NULL != recent_zhist)				\
	{							\
		free(recent_zhist);				\
		recent_zhist = NULL;				\
	}							\
}
#else
#  define RECENT_ZHIST NULL
#  define RELEASE_RECENT_ZHIST
#endif

#ifdef AUTORELINK_SUPPORTED
# define INCR_LINK(FD, ZRO_ENT, ZRO_HIST, FNLEN, FNAME) incr_link(FD, ZRO_ENT, ZRO_HIST, FNLEN, FNAME)
boolean_t incr_link(int *file_desc, zro_ent *zro_entry, zro_hist *recent_zhist_ptr, uint4 fname_len, char *fname);
#else
# define INCR_LINK(FD, ZRO_ENT, ZRO_HIST, FNLEN, FNAME) incr_link(FD, ZRO_ENT, FNLEN, FNAME)
boolean_t incr_link(int *file_desc, zro_ent *zro_entry, uint4 fname_len, char *fname);
#endif

#ifdef __MVS__
#define ZOS_FREE_TEXT_SECTION 		\
	if (NULL != text_section)	\
	{				\
		free(text_section);	\
		text_section = NULL;	\
	}
#else
#define ZOS_FREE_TEXT_SECTION
#endif

#endif /* INCR_LINK_INCLUDED */
