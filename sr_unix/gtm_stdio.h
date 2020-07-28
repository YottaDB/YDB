/****************************************************************
 *								*
 * Copyright (c) 2010-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_stdio.h - gtm interface to stdio.h */

#ifndef GTM_STDIOH
#define GTM_STDIOH

/* This header is split between sr_unix and sr_vvms because there are several test system and standalone modules
 * that do not #define UNIX or VMS for us to know which defines to proceed with. So now this split makes
 * that determination unnecessary. Note we still use the definition of UNIX or not in THIS header to indicate the
 * compilation of a YottaDB source file or a standalone file not needing (or able to get to) libyottadb wrappers.
 */

#include <stdio.h>

#ifdef UNIX
/* If interrupted, the following functions have previously caused hangs, so defer interrupts for their duration to be safe. However,
 * since gtm_stdio.h may be included in a non-GT.M compilation, we define the macros differently based on the UNIX compiler switch,
 * which should only be defined within GT.M.
 */
#  define FDOPEN(VAR, FILE_DES, MODE)					\
{									\
	intrpt_state_t		prev_intrpt_state;			\
									\
	DEFER_INTERRUPTS(INTRPT_IN_FDOPEN, prev_intrpt_state);		\
	VAR = fdopen(FILE_DES, MODE);					\
	ENABLE_INTERRUPTS(INTRPT_IN_FDOPEN, prev_intrpt_state);		\
}

/* Fopen() is not fully capitalized because there is an FOPEN() macro on AIX. */
#  define Fopen(VAR, PATH, MODE)						\
{										\
	intrpt_state_t		prev_intrpt_state;				\
										\
	DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);	\
	VAR = fopen(PATH, MODE);						\
	ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);	\
}
#else
#  define FDOPEN(VAR, FILE_DES, MODE)			\
{							\
	VAR = fdopen(FILD_DES, MODE);			\
}

#  define Fopen(VAR, PATH, MODE)			\
{							\
	VAR = fopen(PATH, MODE);			\
}
#endif

#define FGETS(strg, n, strm, fgets_res)	(fgets_res = fgets(strg,n,strm))
#define GETS(buffer, gets_res)		syntax error
#define PERROR				perror
#define	POPEN				popen
#define TEMPNAM				tempnam
#ifndef P_tmpdir
#define P_tmpdir			"/tmp"
#endif
#define	DEFAULT_GTM_TMP			P_tmpdir
#define RENAME				rename
#define SETVBUF				setvbuf

#ifdef UNIX
   /* We are compiling a GTM source module if UNIX is defined */
#  define FPRINTF			gtm_fprintf
#  define PRINTF			gtm_printf
#  define SPRINTF(STR, ...)		assert(FALSE);	/* don't want to use invocation prone to buffer overruns */
#  define SNPRINTF			gtm_snprintf
   int	gtm_printf(const char *format, ...);
   int	gtm_fprintf(FILE *stream, const char *format, ...);
   int	gtm_snprintf(char *str, size_t size, const char *format, ...);
#  define SCANF				gtm_scanf
#  define SSCANF			gtm_sscanf
#  define FSCANF			gtm_fscanf
   int	gtm_scanf(const char *format, ...);
   int	gtm_fscanf(FILE *stream, const char *format, ...);
   int	gtm_sscanf(char *str, const char *format, ...);
#else
   /* We are compiling a standalone or test system module so no override (This is NOT VMS)  */
#  define FPRINTF			fprintf
#  define PRINTF			printf
#  define SPRINTF			sprintf
#  define SNPRINTF			snprintf
#  define SCANF				scanf
#  define SSCANF			sscanf
#  define FSCANF			fscanf
#  ifndef UNIX_ONLY
     /* Define UNIX_ONLY macro in case it is not already defined (just like it is defined in mdef.h) for use below.
      * Note: No need to define this macro in the "#ifdef UNIX" case above as it should be already defined by "mdef.h".
      */
#    define UNIX_ONLY(X)
#  endif
#endif


/* Note: eintr_handling_check() is used below inside a UNIX_ONLY macro. This is so if the caller is a
 * standalone or test system module (UNIX macro is not defined in that case as mdef.h would not be included)
 * we do not use this macro which would cause a compile time error. This macro should only be used inside the
 * YottaDB runtime. Hence the UNIX_ONLY macro use.
 */
#define VPRINTF(FORMAT, VALUE, RC)				\
{								\
	do							\
	{							\
		RC = vprintf(FORMAT, VALUE);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		UNIX_ONLY(eintr_handling_check());		\
	} while (TRUE);						\
}
#define VFPRINTF(STREAM, FORMAT, VALUE, RC)			\
{								\
	do							\
	{							\
		RC = vfprintf(STREAM, FORMAT, VALUE);		\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		UNIX_ONLY(eintr_handling_check());		\
	} while (TRUE);						\
}
#define VSPRINTF(STRING, FORMAT, VALUE, RC)			\
{								\
	do							\
	{							\
		RC = vsprintf(STRING, FORMAT, VALUE);		\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		UNIX_ONLY(eintr_handling_check());		\
	} while (TRUE);						\
}
#define VSNPRINTF(STRING, SIZE, FORMAT, VALUE, RC)		\
{								\
	do							\
	{							\
		RC = vsnprintf(STRING, SIZE, FORMAT, VALUE);	\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		UNIX_ONLY(eintr_handling_check());		\
	} while (TRUE);						\
}

#define VSCANF(FORMAT, POINTER, RC)				\
{								\
	do							\
	{							\
		RC = vscanf(FORMAT, POINTER);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		UNIX_ONLY(eintr_handling_check());		\
	} while (TRUE);						\
}
#define VSSCANF(STRING, FORMAT, POINTER, RC)			\
{								\
	do							\
	{							\
		RC = vsscanf(STRING, FORMAT, POINTER);		\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		UNIX_ONLY(eintr_handling_check());		\
	} while (TRUE);						\
}
#define VFSCANF(STREAM, FORMAT, POINTER, RC)			\
{								\
	do							\
	{							\
		RC = vfscanf(STREAM, FORMAT, POINTER);		\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		UNIX_ONLY(eintr_handling_check());		\
	} while (TRUE);						\
}

#define SNPRINTF_ENV_NUM(BUFF, LEN, ENV_VAR, ENV_VAL, ENV_IND)						\
{													\
	assert(NULL == strchr(ENV_VAR, '='));	/* strchr() done in ojstartchild() relies on this */	\
	SNPRINTF(BUFF, LEN, "%s=%d", ENV_VAR, ENV_VAL); *ENV_IND++ = BUFF;				\
}

#define SNPRINTF_ENV_STR(BUFF, LEN, ENV_VAR, ENV_VAL, ENV_IND)						\
{													\
	assert(NULL == strchr(ENV_VAR, '='));	/* strchr() done in ojstartchild() relies on this */	\
	SNPRINTF(BUFF, LEN, "%s=%s", ENV_VAR, ENV_VAL); *ENV_IND++ = BUFF;				\
}

#endif
