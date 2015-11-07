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

/* gtm_stdio.h - gtm interface to stdio.h */

#ifndef GTM_STDIOH
#define GTM_STDIOH

/* This header is split between sr_unix and sr_vvms because there are several test system and standalone modules
 * that do not #define UNIX or VMS for us to know which defines to proceed with. So now this split makes
 * that determination unnecessary.
 */

#include <stdio.h>

#define FDOPEN				fdopen
#define FGETS(strg, n, strm, fgets_res)	(fgets_res = fgets(strg,n,strm))
#define Fopen				fopen
#define GETS(buffer, gets_res)		syntax error
#define PERROR				perror
#define	POPEN				popen
#define TEMPNAM				tempnam

#define	DEFAULT_GTM_TMP			P_tmpdir
#define RENAME				rename
#define SETVBUF				setvbuf

#define FPRINTF				fprintf
#define PRINTF				printf
#define SCANF				scanf
#define SSCANF				sscanf
#define SNPRINTF			gtm_snprintf /* hack for VMS, ignore size argument and call sprintf */
#define VPRINTF(STRING, FORMAT, VALUE, RC)		vsprintf(STRING, FORMAT, VALUE)
#define VFPRINTF(STREAM, FORMAT, VALUE, RC)		vfprintf(STREAM, FORMAT, VALUE)
#define VSPRINTF(STRING, FORMAT, VALUE, RC)		vsprintf(STRING, FORMAT, VALUE)
#define VSNPRINTF(STRING, SIZE, FORMAT, VALUE, RC)	vsnprintf(STRING, SIZE, FORMAT, VALUE)
#define VSCANF(FORMAT, POINTER, RC)			vscanf(FORMAT, POINTER)
#define VSSCANF(STRING, FORMAT, POINTER, RC)		vsscanf(STRING, FORMAT, POINTER)
#define VFSCANF(STREAM, FORMAT, POINTER, RC)		vfscanf(STREAM, FORMAT, POINTER)
int	gtm_snprintf(char *str, size_t size, const char *format, ...);

#endif
