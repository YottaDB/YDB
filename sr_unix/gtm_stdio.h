/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
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
 * compilation of a GTM source file or a standalone file not needing (or able to get to) libgtmshr wrappers.
 */

#include <stdio.h>

/* If interrupted, this function has previously caused hangs to do a subsequent gtm_putmsg() invocation from
 * generic_signal_handler(), so just defer interrupts to be safe.
 */
#define FDOPEN(VAR, FILE_DES, MODE)		\
{						\
	DEFER_INTERRUPTS(INTRPT_IN_FDOPEN);	\
	VAR = fdopen(FILE_DES, MODE);		\
	ENABLE_INTERRUPTS(INTRPT_IN_FDOPEN);	\
}

#define FGETS(strg, n, strm, fgets_res)	(fgets_res = fgets(strg,n,strm))
#define Fopen				fopen
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
#  define SPRINTF			gtm_sprintf
#  define SNPRINTF			gtm_snprintf
int	gtm_printf(const char *format, ...);
int	gtm_fprintf(FILE *stream, const char *format, ...);
int	gtm_sprintf(char *str, const char *format, ...);
int	gtm_snprintf(char *str, size_t size, const char *format, ...);
#else
/* We are compiling a standalone or test system module so no override (This is NOT VMS)  */
#  define FPRINTF			fprintf
#  define PRINTF			printf
#  define SPRINTF			sprintf
#  define SNPRINTF			snprintf
#endif

/* Similar to above for *scanf invocations. Note however that TRU64 does NOT have
 * the v*scanf functions used by the wrappers so always use the non-wrapper versions.
 */
#if defined(UNIX) && !defined(__osf__)
#  define SCANF				gtm_scanf
#  define SSCANF			gtm_sscanf
#  define FSCANF			gtm_fscanf
int	gtm_scanf(const char *format, ...);
int	gtm_fscanf(FILE *stream, const char *format, ...);
int	gtm_sscanf(char *str, const char *format, ...);
#else
#  define SCANF				scanf
#  define SSCANF			sscanf
#  define FSCANF			fscanf
#endif

#define VPRINTF(FORMAT, VALUE, RC)				\
{								\
	do							\
	{							\
		RC = vprintf(FORMAT, VALUE);			\
	} while(-1 == RC && EINTR == errno);			\
}
#define VFPRINTF(STREAM, FORMAT, VALUE, RC)			\
{								\
	do							\
	{							\
		RC = vfprintf(STREAM, FORMAT, VALUE);		\
	} while(-1 == RC && EINTR == errno);			\
}
#define VSPRINTF(STRING, FORMAT, VALUE, RC)			\
{								\
	do							\
	{							\
		RC = vsprintf(STRING, FORMAT, VALUE);		\
	} while(-1 == RC && EINTR == errno);			\
}
#define VSNPRINTF(STRING, SIZE, FORMAT, VALUE, RC)		\
{								\
	do							\
	{							\
		RC = vsnprintf(STRING, SIZE, FORMAT, VALUE);	\
	} while(-1 == RC && EINTR == errno);			\
}

/* Note TRU64 does not have these v*scanf() functions so they will generate errors if used */
#define VSCANF(FORMAT, POINTER, RC)				\
{								\
	do							\
	{							\
		RC = vscanf(FORMAT, POINTER);			\
	} while(-1 == RC && EINTR == errno);			\
}
#define VSSCANF(STRING, FORMAT, POINTER, RC)			\
{								\
	do							\
	{							\
		RC = vsscanf(STRING, FORMAT, POINTER);		\
	} while(-1 == RC && EINTR == errno);			\
}
#define VFSCANF(STREAM, FORMAT, POINTER, RC)			\
{								\
	do							\
	{							\
		RC = vfscanf(STREAM, FORMAT, POINTER);		\
	} while(-1 == RC && EINTR == errno);			\
}

#define SPRINTF_ENV_NUM(BUFF, ENV_VAR, ENV_VAL, ENV_IND)						\
{													\
	assert(NULL == strchr(ENV_VAR, '='));	/* strchr() done in ojstartchild() relies on this */	\
	SPRINTF(BUFF, "%s=%d", ENV_VAR, ENV_VAL); *ENV_IND++ = BUFF;					\
}

#define SPRINTF_ENV_STR(BUFF, ENV_VAR, ENV_VAL, ENV_IND)						\
{													\
	assert(NULL == strchr(ENV_VAR, '='));	/* strchr() done in ojstartchild() relies on this */	\
	SPRINTF(BUFF, "%s=%s", ENV_VAR, ENV_VAL); *ENV_IND++ = BUFF;					\
}

#endif
