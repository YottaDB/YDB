/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_stdlib.h - interlude to <stdlib.h> system header file.  */
#ifndef GTM_STDLIBH
#define GTM_STDLIBH

#include <stdlib.h>

#define ATOI	atoi
#define ATOL	atol
#define ATOF	atof

#ifdef UNIX
/* If interrupted, this function has previously caused hangs to do a subsequent syslog() invocation from generic_signal_handler(),
 * so just defer interrupts to be safe. UNIX is a GT.M-specific compiler switch, which we expect to be undefined for any non-GT.M
 * compilation that might include this file.
 */
#define PUTENV(VAR, ARG)							\
{										\
	intrpt_state_t		prev_intrpt_state;				\
										\
	DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);	\
	VAR = putenv(ARG);							\
	ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);	\
}
#else
#  define PUTENV(VAR, ARG)				\
{							\
	VAR = putenv(ARG);				\
}
#endif
#define STRTOL		strtol
#define STRTOLL		strtoll
#define STRTOUL		strtoul
# if INT_MAX < LONG_MAX	/* like Tru64 */
#  define STRTO64L	strtol
#  define STRTOU64L	strtoul
# else
#  define STRTO64L	strtoll
#  define STRTOU64L	strtoull
# endif
#define MKSTEMP(template,mkstemp_res)					\
{									\
	intrpt_state_t		prev_intrpt_state;			\
									\
	DEFER_INTERRUPTS(INTRPT_IN_MKSTEMP, prev_intrpt_state);		\
	mkstemp_res = mkstemp(template);				\
	ENABLE_INTERRUPTS(INTRPT_IN_MKSTEMP, prev_intrpt_state);	\
}
#define MKDTEMP		mkdtemp
# if defined(STATIC_ANALYSIS)
#  define SYSTEM	system
# else
#  define SYSTEM	gtm_system
   int gtm_system(const char *cmdline);
# endif
int gtm_system_internal(const char *sh, const char *opt, const char *rtn, const char *cmdline);

void gtm_image_exit(int status) __attribute__ ((noreturn));

#define	EXIT(x)		gtm_image_exit(x)

#endif
