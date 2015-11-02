/****************************************************************
 *								*
 *	Copyright 2008, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_ZLIB_H_INCLUDED
#define	GTM_ZLIB_H_INCLUDED

#if (defined(__osf__) && defined(__alpha)) || (defined(__MVS__))
	/* For some reason, zconf.h (included by zlib.h) on Tru64 seems to undefine const if STDC is not defined.
	 * The GT.M build time options currently dont define __STDC__ on Tru64 (which is what leads zconf.h to define STDC)
	 * so define STDC temporarily. In any case check if it is defined and only if not defined, do the overriding define.
	 */
#	if (!defined(STDC))
#		define	GTM_ZLIB_STDC_DEFINE
#		define	STDC
#	endif
#endif

#include <zlib.h>

#if (defined(__osf__) && defined(__alpha)) || (defined(__MVS__))
	/* Undefine STDC in case it was defined just above */
#	if (defined(GTM_ZLIB_STDC_DEFINE))
#		undef STDC
#	endif
#endif

typedef int	(*zlib_cmp_func_t)(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level);
typedef	int	(*zlib_uncmp_func_t)(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen);
typedef	uLong	(*zlib_cmpbound_func_t)(uLong sourceLen);

GBLREF	zlib_cmp_func_t		zlib_compress_fnptr;
GBLREF	zlib_uncmp_func_t	zlib_uncompress_fnptr;

/* The standard shared library suffix for HPUX on HPPA is .sl.
 * On HPUX/IA64, the standard suffix was changed to .so (to match other Unixes) but for
 * the sake of compatibility, they still accept (and look for) .sl if .so is not present.
 * Nevertheless, we use the standard suffix on all platforms.
 */
#if (defined(__hpux) && defined(__hppa))
#	define	ZLIB_LIBNAME	"libz.sl"
#else
#	define	ZLIB_LIBNAME	"libz.so"
#endif

#define	ZLIB_LIBFLAGS	(RTLD_NOW)	/* RTLD_NOW - resolve immediately so we know errors sooner than later */

#define	ZLIB_CMP_FNAME		"compress2"
#define	ZLIB_UNCMP_FNAME	"uncompress"

#define	ZLIB_NUM_DLSYMS		2	/* number of function names that we need to dlsym (compress2 and uncompress) */

GBLREF	int4			gtm_zlib_cmp_level;	/* zlib compression level specified at process startup */
GBLREF	int4			repl_zlib_cmp_level;	/* zlib compression level currently in use in replication pipe */

#define	ZLIB_CMPLVL_MIN		0
#define	ZLIB_CMPLVL_MAX		9	/* although currently known max zlib compression level is 9, it could be higher in
					 * future versions of zlib so we dont do any edit checks on this value inside of GT.M */
#define	ZLIB_CMPLVL_NONE	ZLIB_CMPLVL_MIN

#define	GTM_CMPLVL_OUT_OF_RANGE(x)	(ZLIB_CMPLVL_MIN > x)

void gtm_zlib_init(void);

/* Macros for zlib compress2 and uncompress function calls. Since 'malloc' or 'free' inside zlib library does NOT go
 * through gtm_malloc or gtm_free respectively, defer signals (MUPIP STOP for instance) until the corresponding zlib
 * call is completed so as to avoid deadlocks involving nested 'malloc' or 'free' each waiting for the other's
 * completion
 */
#define ZLIB_COMPRESS(CMPBUFF_PTR, CMPLEN, UNCMPBUFF_PTR, UNCMPLEN, ZLIB_CMP_LEVEL, RC)					\
{															\
	GBLREF zlib_cmp_func_t		zlib_compress_fnptr;								\
															\
	DEFER_INTERRUPTS(INTRPT_IN_ZLIB_CMP_UNCMP);									\
	assert(0 < (signed)(CMPLEN));											\
	assert(NULL != zlib_compress_fnptr);										\
	RC = (*zlib_compress_fnptr)(((Bytef *)(CMPBUFF_PTR)), (uLongf *)&(CMPLEN), (const Bytef *)(UNCMPBUFF_PTR), 	\
					(uLong)(UNCMPLEN), ZLIB_CMP_LEVEL);						\
	ENABLE_INTERRUPTS(INTRPT_IN_ZLIB_CMP_UNCMP);									\
}

#define ZLIB_UNCOMPRESS(UNCMPBUFF_PTR, UNCMPLEN, CMPBUFF_PTR, CMPLEN, RC)						\
{															\
	GBLREF zlib_uncmp_func_t	zlib_uncompress_fnptr;								\
															\
	DEFER_INTERRUPTS(INTRPT_IN_ZLIB_CMP_UNCMP);									\
	assert(0 < (signed)(UNCMPLEN));											\
	assert(NULL != zlib_uncompress_fnptr);										\
	RC = (*zlib_uncompress_fnptr)(((Bytef *)(UNCMPBUFF_PTR)), (uLongf *)&(UNCMPLEN), (const Bytef *)(CMPBUFF_PTR),	\
					(uLong)(CMPLEN));								\
	ENABLE_INTERRUPTS(INTRPT_IN_ZLIB_CMP_UNCMP);									\
}

#endif
