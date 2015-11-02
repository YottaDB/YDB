/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

#include <stdio.h>

#define FDOPEN				fdopen
#define FGETS(strg,n,strm,fgets_res)	(fgets_res = fgets(strg,n,strm))
#define Fopen				fopen
#define GETS(buffer,gets_res)		syntax error
#define PERROR				perror
#define	POPEN				popen
#define TEMPNAM				tempnam
#ifndef P_tmpdir
#define P_tmpdir			"/tmp"
#endif
#define	DEFAULT_GTM_TMP			P_tmpdir
#define RENAME				rename
#define SETVBUF				setvbuf

#define FPRINTF         		fprintf

#ifdef UNIX
/* #define to be gtm_fprintf to serve as a wrapper for blocking signals since fprintf is not signal-safe. */
#define	fprintf		gtm_fprintf
int	gtm_fprintf(FILE *stream, const char *format, ...);	/* Define prototype of "gtm_fprintf" here */
#endif

#define FSCANF         			fscanf
#define PRINTF         			printf
#define SCANF          			scanf
#define SSCANF         			sscanf
#define SPRINTF       			sprintf
#ifdef VMS
int					gtm_snprintf(char *str, size_t size, const char *format, ...);
#define SNPRINTF       			gtm_snprintf /* hack for VMS, ignore size argument and call sprintf */
#else
#define SNPRINTF       			snprintf
#endif
#define VFPRINTF       			vfprintf
#define VPRINTF        			vprintf
#define VSPRINTF       			vsprintf

#define SPRINTF_ENV_NUM(BUFF, ENV_VAR, ENV_VAL, ENV_IND)							\
{														\
	assert(NULL == strchr(ENV_VAR, '='));	/* strchr() done later in ojstartchild() relies on this */	\
	sprintf(BUFF, "%s=%d", ENV_VAR, ENV_VAL); *ENV_IND++ = BUFF;						\
}

#define SPRINTF_ENV_STR(BUFF, ENV_VAR, ENV_VAL, ENV_IND)							\
{														\
	assert(NULL == strchr(ENV_VAR, '='));	/* strchr() done later in ojstartchild() relies on this */	\
	sprintf(BUFF, "%s=%s", ENV_VAR, ENV_VAL); *ENV_IND++ = BUFF;						\
}

#endif
