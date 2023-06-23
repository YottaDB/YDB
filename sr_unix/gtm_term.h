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

/* gtm_term.h - interlude to <term.h> system header file.  */
#ifndef GTM_TERMH
#define GTM_TERMH

#ifdef __CYGWIN__
#include <ncurses/term.h>
#else
#include <term.h>
#endif

#define SETUPTERM	setupterm

#endif
