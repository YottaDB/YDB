/****************************************************************
 *								*
 *	Copyright 2006, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* NOTE: THIS MODULE MUST NEVER INCLUDE HEADER fILE gtm_icu_api.h */

#include "mdef.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include <wctype.h>
#include <dlfcn.h>
#include <locale.h>		/* needed for setlocale() */
#include <langinfo.h>		/* needed for nl_langinfo() */
#include "error.h"
#include "io.h"
#include "iosp.h"
#include "gtm_logicals.h"	/* needed for GTM_ICU_MINOR_ENV */
#include "trans_log_name.h"
#include "fgncal.h"		/* needed for COPY_DLLERR_MSG() */

typedef void (*icu_func_t)();	/* a generic pointer type to the ICU function */

/* Declare enumerators for all functions */
#define ICU_DEF(x) x##_,
enum {
#include "gtm_icu.h"
icu_func_n		/* total number of ICU functions used */
};
#undef ICU_DEF

/* Initialize the table of versioned symbol names to be used in dlsym() */
#define ICU_STRINGIFY(x) #x
#define ICU_ADDVER(x,y) ICU_STRINGIFY(x##y)
#define ICU_DEF(x) ICU_ADDVER(x,_3_6),		/* ICU version 3.6 */
LITDEF char* icu_fname[] =
{
#include "gtm_icu.h"
NULL
};
#undef ICU_DEF

#define ICU_DEF(x) GBLDEF icu_func_t x##_ptr;
#include "gtm_icu.h"
#undef ICU_DEF

/* initialize the table of locations of function pointers that are set by dlsym() */
#define ICU_DEF(x) &x##_ptr,
GBLDEF icu_func_t *icu_fptr[] =
{
#include "gtm_icu.h"
NULL
};
#undef ICU_DEF

/* duplicated prototypes needed to avoid including header file gtm_icu_api.h */
void gtm_icu_init(void);
void gtm_conv_init(void);

error_def(ERR_DLLNOOPEN);
error_def(ERR_INVICUVER);
error_def(ERR_TEXT);
error_def(ERR_NONUTF8CHSET);

void gtm_icu_init(void)
{
	char		*locale, *chset, *libname, err_msg[MAX_ERRSTR_LEN];
	void_ptr_t	handle;
	char_ptr_t	err_str;
	icu_func_t	fptr;
	int		findx;
#ifdef _AIX
	int		buflen;
	mstr		icu_minor_ver, trans;
	char            buf[MAX_TRANS_NAME_LEN + sizeof(ICU_LIBNAME)];
#endif

	assert(!gtm_utf8_mode);
	locale = setlocale(LC_CTYPE, "");
	chset = nl_langinfo(CODESET);
	if (NULL == locale || NULL == chset || (0 != strcasecmp(chset, "utf-8") &&
			0 != strcasecmp(chset, "utf8")))
	{
		rts_error(VARLSTCNT(4) ERR_NONUTF8CHSET, 2, LEN_AND_STR(chset));
	}
#ifdef _AIX
	/* AIX has unique packaging convention in that shared objects are conventionally
	 * archived into a static (.a) library. Moreover, ICU adds the major/minor version
	 * suffix to the member shared object. We hardcode major version in the ICU_LIBNAME.
	 * In order to allow all minor versions within a major ICU release, GT.M tags the
	 * minor version number (at runtime) from the environment variable */
	icu_minor_ver.addr = GTM_ICU_MINOR_ENV;
	icu_minor_ver.len = STR_LIT_LEN(GTM_ICU_MINOR_ENV);
	MEMCPY_LIT(buf, ICU_LIBNAME);
	buflen = STR_LIT_LEN(ICU_LIBNAME);
	if (SS_NORMAL == TRANS_LOG_NAME(&icu_minor_ver, &trans, &buf[buflen], sizeof(buf) - buflen, do_sendmsg_on_log2long))
		buflen += trans.len;
	else { /* default minor version is "0" */
		MEMCPY_LIT(&buf[buflen], "0");
		buflen += STR_LIT_LEN("0");
	}
	MEMCPY_LIT(&buf[buflen], ".so)");
	buflen += STR_LIT_LEN(".so)");
	buf[buflen] = '\0'; /* null-termination */
	handle = dlopen(buf, ICU_LIBFLAGS | RTLD_MEMBER);
	libname = buf;
#else
	libname = ICU_LIBNAME;
	handle = dlopen(libname, ICU_LIBFLAGS);
#endif

	if (NULL == handle)
	{
		COPY_DLLERR_MSG(err_str, err_msg);
		rts_error(VARLSTCNT(8) ERR_DLLNOOPEN, 2, LEN_AND_STR(libname),
				ERR_TEXT, 2, LEN_AND_STR(err_msg));
	}
#ifdef __hpux
	/* HP-UX dlsym() doesn't allow lookup for symbols that are present in the nested dependent
	 * shared libraries of ICU_LIBNAME. Workaround is to lookup within the global space (i.e.
	 * from invoking module libgtmshr) where all symbols would have been brought in by previous
	 * dlopen() with the RTLD_GLOBAL flag */
	handle = dlopen(NULL, ICU_LIBFLAGS);
	if (NULL == handle)
		GTMASSERT;
#endif
	for (findx = 0; findx < icu_func_n; ++findx)
	{
		fptr = (icu_func_t)dlsym(handle, icu_fname[findx]);
		if (NULL == fptr)
		{
			COPY_DLLERR_MSG(err_str, err_msg);
			rts_error(VARLSTCNT(10) ERR_INVICUVER, 4, LEN_AND_STR(icu_fname[findx]),
					LEN_AND_LIT("3.6.x"), ERR_TEXT, 2, LEN_AND_STR(err_msg));
		}
		*icu_fptr[findx] = fptr;
	}
	gtm_utf8_mode = TRUE;
	/* gtm_wcswidth()/U_ISPRINT() in util_format() can henceforth be safely called now that ICU initialization is complete */
	gtm_conv_init();
}
