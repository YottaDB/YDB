/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef DOLLAR_QUIT_INCLUDE
#define DOLLAR_QUIT_INCLUDE

/* Note dollar_quit() examines the generated code at the return point of the caller. To do
 * this, it looks as the instruction stream. Add appropriate hardware includes below for
 * each platform as appropriate and define the number of bytes from the return point mpc
 * stored in the stack to the instruction we are interested in that would contain the
 * index into the xfer table.
 */

#if defined(__x86_64__)
#  define EXFUNRET_INST_OFFSET	4
#elif defined(__i386)
#  define EXFUNRET_INST_OFFSET	3
#elif defined(_AIX)
#  define EXFUNRET_INST_OFFSET	4
#elif defined(__alpha)	/* Applies to both VMS and Tru64 as have same codegen */
#  define EXFUNRET_INST_OFFSET	4
#elif defined(__sparc)
#  include "sparc.h"
#  define EXFUNRET_INST_OFFSET	4
#elif defined(__MVS__)
#  include "s390.h"
#  define EXFUNRET_INST_OFFSET	6
#elif defined(__hppa)
#  include "hppa.h"
#  define EXFUNRET_INST_OFFSET	8
#elif defined(__ia64)
#  include "ia64.h"
#  define EXFUNRET_INST_OFFSET	16
#endif

int dollar_quit(void);

#endif
