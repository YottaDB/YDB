/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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
#else
#include "zroutinessp.h"	/* need zro_ent typedef for i386 dummy argument */
#endif
boolean_t incr_link(int file_desc, zro_ent *zro_entry, uint4 fname_len, char *fname);

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
