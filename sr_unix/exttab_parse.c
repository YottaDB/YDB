/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_ctype.h"
#include "gtm_stat.h"

#include <stdarg.h>
#include <errno.h>

#include "copy.h"
#include "libyottadb.h"
#include "rtnhdr.h"
#include "lv_val.h"	/* needed for "fgncal.h" */
#include "fgncal.h"
#include "gtmci.h"
#include "eintr_wrappers.h"
#include "error.h"
#include "gtm_malloc.h"
#include "trans_log_name.h"
#include "iosp.h"
#include "ydb_getenv.h"
#include "gtm_limits.h"
#include "restrict.h"

GBLREF	char 			ydb_dist[YDB_PATH_MAX];

#define	CR			0x0A		/* Carriage return */
#define	NUM_TABS_FOR_GTMERRSTR	2
#define	MAX_SRC_LINE		1024
#define	MAX_NAM_LEN 		32
#define	MAXIMUM_STARS		2
#define	SPACE_BLOCK_SIZE	((NON_GTM64_ONLY(1024) GTM64_ONLY(2048)) - SIZEOF(storElem))
#define	TABLEN			8
#define	POINTER_SIZE		6

STATICDEF int	ext_source_line_num;
STATICDEF int	ext_source_line_len;
STATICDEF int	ext_source_column;
STATICDEF char	ext_source_line[MAX_SRC_LINE];
STATICDEF char	*ext_table_file_name;
STATICDEF boolean_t star_found;

STATICFNDCL void *get_memory(size_t n);
STATICFNDCL char *exttab_scan_space(char *c);
STATICFNDCL char *scan_ident(char *c);
STATICFNDCL char *scan_labelref(char *c);
STATICFNDCL enum ydb_types scan_keyword(char **c);
STATICFNDCL int scan_array_bound(char **b,int curr_type);
STATICFNDCL char *read_table(char *b, int l, FILE *f);
STATICFNDCL void put_mstr(mstr *src, mstr *dst);
STATICFNDCL uint4 array_to_mask(boolean_t ar[MAX_ACTUALS], int n);
STATICFNDCL void ext_stx_error(int in_error, ...);

const int parm_space_needed[] =
{
	0,
	0,
	SIZEOF(void *),
	SIZEOF(ydb_int_t),
	SIZEOF(ydb_uint_t),
	SIZEOF(ydb_long_t),
	SIZEOF(ydb_ulong_t),
	SIZEOF(ydb_float_t),
	SIZEOF(ydb_double_t),
	SIZEOF(ydb_int_t *) + SIZEOF(ydb_int_t),
	SIZEOF(ydb_uint_t *) + SIZEOF(ydb_uint_t),
	SIZEOF(ydb_long_t *) + SIZEOF(ydb_long_t),
	SIZEOF(ydb_ulong_t *) + SIZEOF(ydb_ulong_t),
	SIZEOF(ydb_string_t *) + SIZEOF(ydb_string_t),
	SIZEOF(ydb_float_t *) + SIZEOF(ydb_float_t),
	SIZEOF(ydb_char_t *),
	SIZEOF(ydb_char_t **) + SIZEOF(ydb_char_t *),
	SIZEOF(ydb_double_t *) + SIZEOF(ydb_double_t),
	SIZEOF(ydb_pointertofunc_t),
	SIZEOF(ydb_pointertofunc_t *) + SIZEOF(ydb_pointertofunc_t),
	SIZEOF(ydb_int_t *) + SIZEOF(ydb_int_t),
	SIZEOF(ydb_int_t *) + SIZEOF(ydb_int_t),
	SIZEOF(ydb_long_t *) + SIZEOF(ydb_long_t),
	SIZEOF(ydb_float_t *) + SIZEOF(ydb_float_t),
	SIZEOF(ydb_double_t *) + SIZEOF(ydb_double_t),
	SIZEOF(ydb_string_t),  /* ydb_string_t contains a (ydb_long_t) and (ydb_char_t *) */
	SIZEOF(ydb_string_t),
	SIZEOF(ydb_string_t)
};

/* This table is searched serially so the search priority is:
 *   1. ydb_ types
 *   2. gtm_ types
 *   3. native types
 *   4. the long deprecated xc_ types bringing up the rear.
 */
const static struct
{
	char		nam[MAX_NAM_LEN];
	enum ydb_types	typ[MAXIMUM_STARS + 1]; /* One entry for each level of indirection eg [1] is type* */
} xctab[] =
{
/*	typename		type			type *			type **			*/
	{"ydb_char_t",		{ydb_notfound,		ydb_char_star,		ydb_char_starstar}	},
	{"ydb_double_t",	{ydb_double,		ydb_double_star,	ydb_notfound}		},
	{"ydb_float_t",		{ydb_float,		ydb_float_star,		ydb_notfound}		},
	{"ydb_int_t",		{ydb_int,		ydb_int_star,		ydb_notfound}		},
	{"ydb_jbig_decimal_t",	{ydb_jbig_decimal,	ydb_notfound,		ydb_notfound}		},
	{"ydb_jboolean_t",	{ydb_jboolean,		ydb_notfound,		ydb_notfound}		},
	{"ydb_jbyte_array_t",	{ydb_jbyte_array, 	ydb_notfound,		ydb_notfound}		},
	{"ydb_jdouble_t",	{ydb_jdouble,		ydb_notfound,		ydb_notfound}		},
	{"ydb_jfloat_t",	{ydb_jfloat,		ydb_notfound,		ydb_notfound}		},
	{"ydb_jint_t",		{ydb_jint,		ydb_notfound,		ydb_notfound}		},
	{"ydb_jlong_t",		{ydb_jlong,		ydb_notfound,		ydb_notfound}		},
	{"ydb_jstring_t",	{ydb_jstring,		ydb_notfound,		ydb_notfound}		},
	{"ydb_long_t",		{ydb_long,		ydb_long_star,		ydb_notfound}		},
	{"ydb_pointertofunc_t", {ydb_pointertofunc, 	ydb_pointertofunc_star,	ydb_notfound}		},
	{"ydb_status_t",	{ydb_status,		ydb_notfound,		ydb_notfound}		},
	{"ydb_string_t",	{ydb_notfound,		ydb_string_star,	ydb_notfound}		},
	{"ydb_uint_t",		{ydb_uint,		ydb_uint_star,		ydb_notfound}		},
	{"ydb_ulong_t",		{ydb_ulong,		ydb_ulong_star,		ydb_notfound}		},
	{"gtm_char_t",		{ydb_notfound,		ydb_char_star,		ydb_char_starstar}	},
	{"gtm_double_t",	{ydb_double,		ydb_double_star,	ydb_notfound}		},
	{"gtm_float_t",		{ydb_float,		ydb_float_star,		ydb_notfound}		},
	{"gtm_int_t",		{ydb_int,		ydb_int_star,		ydb_notfound}		},
	{"gtm_jbig_decimal_t",	{ydb_jbig_decimal,	ydb_notfound,		ydb_notfound}		},
	{"gtm_jboolean_t",	{ydb_jboolean,		ydb_notfound,		ydb_notfound}		},
	{"gtm_jbyte_array_t",	{ydb_jbyte_array, 	ydb_notfound,		ydb_notfound}		},
	{"gtm_jdouble_t",	{ydb_jdouble,		ydb_notfound,		ydb_notfound}		},
	{"gtm_jfloat_t",	{ydb_jfloat,		ydb_notfound,		ydb_notfound}		},
	{"gtm_jint_t",		{ydb_jint,		ydb_notfound,		ydb_notfound}		},
	{"gtm_jlong_t",		{ydb_jlong,		ydb_notfound,		ydb_notfound}		},
	{"gtm_jstring_t",	{ydb_jstring,		ydb_notfound,		ydb_notfound}		},
	{"gtm_long_t",		{ydb_long,		ydb_long_star,		ydb_notfound}		},
	{"gtm_pointertofunc_t", {ydb_pointertofunc, 	ydb_pointertofunc_star,	ydb_notfound}		},
	{"gtm_status_t",	{ydb_status,		ydb_notfound,		ydb_notfound}		},
	{"gtm_string_t",	{ydb_notfound,		ydb_string_star,	ydb_notfound}		},
	{"gtm_uint_t",		{ydb_uint,		ydb_uint_star,		ydb_notfound}		},
	{"gtm_ulong_t",		{ydb_ulong,		ydb_ulong_star,		ydb_notfound}		},
	{"void",		{ydb_void,		ydb_notfound,		ydb_notfound}		},
	{"char",		{ydb_notfound,		ydb_char_star,		ydb_char_starstar}	},
	{"double",		{ydb_double,		ydb_double_star,	ydb_notfound}		},
	{"float",		{ydb_float,		ydb_float_star,		ydb_notfound}		},
	{"int",			{ydb_int,		ydb_notfound,		ydb_notfound}		},
	{"long",		{ydb_long,		ydb_long_star,		ydb_notfound}		},
	{"string",		{ydb_notfound,		ydb_string_star,	ydb_notfound}		},
	{"uint",		{ydb_uint,		ydb_uint_star,		ydb_notfound}		},
	{"ulong",		{ydb_ulong,		ydb_ulong_star,		ydb_notfound}		},
	{"xc_char_t",		{ydb_notfound,		ydb_char_star,		ydb_char_starstar}	},
	{"xc_double_t",		{ydb_double,		ydb_double_star,	ydb_notfound}		},
	{"xc_float_t",		{ydb_float,		ydb_float_star,		ydb_notfound}		},
	{"xc_int_t",		{ydb_int,		ydb_int_star,		ydb_notfound}		},
	{"xc_long_t",		{ydb_long,		ydb_long_star,		ydb_notfound}		},
	{"xc_pointertofunc_t", 	{ydb_pointertofunc, 	ydb_pointertofunc_star,	ydb_notfound}		},
	{"xc_status_t",		{ydb_status,		ydb_notfound,		ydb_notfound}		},
	{"xc_string_t",		{ydb_notfound,		ydb_string_star,	ydb_notfound}		},
	{"xc_uint_t",		{ydb_uint,		ydb_uint_star,		ydb_notfound}		},
	{"xc_ulong_t",		{ydb_ulong,		ydb_ulong_star,		ydb_notfound}		}
};

const static int default_pre_alloc_value[] =
{
	0, /* unknown Type */
	0, /* void */
	0, /* status */
	0, /* int */
	0, /* uint */
	0, /* long */
	0, /* unsigned long */
	0, /* float */
	0, /* double */
	1, /* pointer to int */
	1, /* pointer to unsigned int */
	1, /* pointer to long */
	1, /* pointer to unsigned long */
	1, /* pointer to string */
	1, /* pointer to float */
	100, /* pointer to char */
	1, /* pointer to pointer of char */
	1, /* pointer to double */
	1, /* pointer to function */
	1, /* pointer to pointer to function */
	1, /* java int */
	1, /* java int */
	1, /* java long */
	1, /* java float */
	1, /* java double */
	1, /* java string */
	1, /* java byte array */
	1  /* java big decimal */
};

error_def(ERR_CIDIRECTIVE);
error_def(ERR_CIENTNAME);
error_def(ERR_CIMAXPARAM);
error_def(ERR_CIPARTYPE);
error_def(ERR_CIRCALLNAME);
error_def(ERR_CIRPARMNAME);
error_def(ERR_CIRTNTYP);
error_def(ERR_CITABENV);
error_def(ERR_CITABOPN);
error_def(ERR_CIUNTYPE);
error_def(ERR_COLON);
error_def(ERR_EXTSRCLIN);
error_def(ERR_EXTSRCLOC);
error_def(ERR_LOGTOOLONG);
error_def(ERR_TEXT);
error_def(ERR_SYSCALL);
error_def(ERR_ZCALLTABLE);
error_def(ERR_ZCCOLON);
error_def(ERR_ZCCLNUPRTNMISNG);
error_def(ERR_ZCCSQRBR);
error_def(ERR_ZCCTENV);
error_def(ERR_ZCCTNULLF);
error_def(ERR_ZCCTOPN);
error_def(ERR_ZCENTNAME);
error_def(ERR_ZCINVALIDKEYWORD);
error_def(ERR_ZCMLTSTATUS);
error_def(ERR_ZCPREALLNUMEX);
error_def(ERR_ZCPREALLVALINV);
error_def(ERR_ZCPREALLVALPAR);
error_def(ERR_ZCRCALLNAME);
error_def(ERR_ZCRPARMNAME);
error_def(ERR_ZCRTENOTF);
error_def(ERR_ZCRTNTYP);
error_def(ERR_ZCUNAVAIL);
error_def(ERR_ZCUNTYPE);
error_def(ERR_ZCUSRRTN);

/* Manage local get_memory'ed space (the space is never returned) */
STATICFNDEF void *get_memory(size_t n)
{
	void		*x;
	static void	*heap_base = 0;
	static int	space_remaining = 0;

	if (0 == n)
		return NULL;
	/* round up to 64 bits bytes */
	n = ROUND_UP(n, 8);
	if (space_remaining < n)
	{
		if (SPACE_BLOCK_SIZE < n)
			return (void *)malloc(n);
		else
		{
			heap_base = (void *)malloc(SPACE_BLOCK_SIZE);
			assert(NULL != heap_base); /* 4SCA: NULL return */
			space_remaining = SPACE_BLOCK_SIZE;
		}
	}
	assert(space_remaining >= n);
	x = heap_base;
	heap_base = (char *)heap_base + n;
	space_remaining -= (int)n;
	return x;
}

/* Skip white space */
STATICFNDEF char *exttab_scan_space(char *c)
{
	for ( ; ISSPACE_ASCII(*c); c++, ext_source_column++)
		;
	return c;
}

/* If this is an identifier (alphameric and underscore), then
 * return the address after the end of the identifier.
 * Otherwise, return zero
 */
STATICFNDEF char *scan_ident(char *c)
{
	char	*b;

	b = c;
	for ( ; ISALNUM_ASCII(*b) || ('_' == *b); b++, ext_source_column++)
		;
	return (b == c) ? 0 : b;
}

/* If this is a label (alphameric, underscore, caret, and percent (C9E12-002681)), then
 * return the address after the end of the label.
 * Otherwise, return zero.
 */
STATICFNDEF char *scan_labelref(char *c)
{
	char	*b = c;
	uint4	state = 0;

	for ( ; ; b++, ext_source_column++)
	{
		if (ISALNUM_ASCII(*b))
			continue;
		if ('%' == *b)
		{
			if (1 & state)
				break;
			state++;
			continue;
		}
		if ('^' == *b)
		{
			if (2 & state)
				break;
			state = 2;
			continue;
		}
		break;
	}
	return (('(' != *b) && (' ' != *b)) ? 0 : b;
}

STATICFNDEF enum ydb_types scan_keyword(char **c)
{
	char	*b = *c;
	char	*d;
	int	len, i, star_count;

	b = exttab_scan_space(b);
	d = scan_ident(b);
	if (!d)
		return ydb_notfound;
	len = (int)(d - b);
	for (i = 0 ; i < SIZEOF(xctab) / SIZEOF(xctab[0]) ; i++)
	{
		if ((0 == memcmp(xctab[i].nam, b, len)) && ('\0' ==  xctab[i].nam[len]))
		{
			/* got name */
			/* scan stars */
			for (star_count = 0; (MAXIMUM_STARS >= star_count); star_count++, d++)
			{
				d = exttab_scan_space(d);
				if ('*' != *d)
					break;
				star_found = TRUE;
			}
			assert(star_count <= MAXIMUM_STARS);
			*c = exttab_scan_space(d);
			return xctab[i].typ[star_count];
		}
	}
	return ydb_notfound;
}

STATICFNDEF int scan_array_bound(char **b,int curr_type)
{
	char 		number[MAX_DIGITS_IN_INT];
	char		*c;
	int 		index;

	c = *b;
	/* Already found '[' */
	for (index = 0, c++; ']' != *c; c++)
	{
		if ('\0' != *c)
		{
			if (ISDIGIT_ASCII((int)*c))
				number[index++] = *c;
			else
				ext_stx_error(ERR_ZCPREALLNUMEX, ext_table_file_name);
		} else
			ext_stx_error(ERR_ZCCSQRBR, ext_table_file_name);
	}
	c++; /* Skip ']' */
	*b = c;
	if (0 == index)
		return default_pre_alloc_value[curr_type];
	number[index] = 0;
	return ATOI(number);
}

STATICFNDEF char *read_table(char *b, int l, FILE *f)
{
	char	*t;
	int	fclose_res;

	ext_source_column = 0;
	FGETS(b, l, f, t);
	if (NULL == t)
	{
		if (0 != ferror(f))
		{
			FCLOSE(f, fclose_res);
			/* Expand error message */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
		} else
			assert(feof(f) != 0);
	} else
	{
		/* Unfortunately, fgets does not strip the NL, we will do it for it */
		for (ext_source_line_len = 0; *t ; t++)
		{
			ext_source_line[ext_source_line_len++] = *t;
			if (CR == *t)
			{
				*t = 0;
				ext_source_line_num += 1;
				break;
			}
		}
	}
	return t;
}

/* Utility routine to store static mstr's which will remain for the duration of the process
 * These mstr's will have a trailing null attached so that UNIX system routines can operate
 * on them directly as const char *'s
 */
STATICFNDEF void put_mstr(mstr *src, mstr *dst)
{
	char	*cp;
	ssize_t	n;

	dst->len = src->len;
	n = (ssize_t)src->len;

	assert(n >= 0);
	dst->addr = cp = (char *)get_memory((size_t)(n + 1));
	assertpro(NULL != dst->addr); /* 4SCA: NULL return */
	if (0 < n)
		memcpy(dst->addr, src->addr, dst->len);
	dst->addr[n] = 0;
	return;
}

/* Utility to convert an array of bool's to a bit mask */
STATICFNDEF uint4 array_to_mask(boolean_t ar[MAX_ACTUALS], int n)
{
	uint4	mask = 0;
	int	i;

	for (i = n - 1; 0 <= i; i--)
	{
		assert((ar[i] & ~1) == 0);
		mask = (mask << 1) | ar[i];
	}
	return mask;
}

/* Note: Need condition handler to clean-up allocated structures and close intput file in the event of an error */
struct extcall_package_list *exttab_parse(mval *package)
{
	int		parameter_alloc_values[MAX_ACTUALS], parameter_count, ret_pre_alloc_val, i, fclose_res;
	int		len, keywordlen;
	boolean_t	is_input[MAX_ACTUALS], is_output[MAX_ACTUALS], got_status;
	mstr		callnam, rtnnam, clnuprtn;
	mstr 		val, trans;
	void_ptr_t	pakhandle;
	int		nbytes;
	enum ydb_types	ret_tok, parameter_types[MAX_ACTUALS], pr;
	char		str_buffer[MAX_TABLINE_LEN], *tbp, *end;
	char		str_temp_buffer[MAX_TABLINE_LEN];
	FILE		*ext_table_file_handle;
	boolean_t	is_ydb_env_match;
	struct extcall_package_list	*pak;
	struct extcall_entry_list	*entry_ptr;

	/* First, construct package name environment variable */
	if (package->str.len)
		ext_table_file_name = ydb_getenv(YDBENVINDX_XC_PREFIX, &package->str, &is_ydb_env_match);
	else
		ext_table_file_name = ydb_getenv(YDBENVINDX_XC, NULL_SUFFIX, &is_ydb_env_match);
	if (NULL == ext_table_file_name)
	{	/* Environment variable for the package not found */
		if (package->str.len)
			nbytes = SNPRINTF(str_buffer, SIZEOF(str_buffer), "%s%.*s/%s%.*s",
					ydbenvname[YDBENVINDX_XC_PREFIX] + 1, package->str.len, package->str.addr,
					gtmenvname[YDBENVINDX_XC_PREFIX] + 1, package->str.len, package->str.addr);
		else
			nbytes = SNPRINTF(str_buffer, SIZEOF(str_buffer), "%s/%s",
					ydbenvname[YDBENVINDX_XC] + 1,
					gtmenvname[YDBENVINDX_XC] + 1);
		if ((0 < nbytes) && (SIZEOF(str_buffer) > nbytes))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZCCTENV, 2, LEN_AND_STR(str_buffer));
		else
		{
			assert(FALSE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
					LEN_AND_LIT("SNPRINTF(exttab_parse)"), CALLFROM, errno);
		}
	}
	/* Now we have the environment name, lookup file name */
	Fopen(ext_table_file_handle, ext_table_file_name, "r");
	if (NULL == ext_table_file_handle)
	{	/* Package's external call table could not be found */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZCCTOPN, 2, LEN_AND_STR(ext_table_file_name));
	}
	ext_source_line_num = 0;
	/* Pick-up name of shareable library */
	tbp = read_table(LIT_AND_LEN(str_buffer), ext_table_file_handle);
	if (NULL == tbp)
	{
		/* External call table is a null file */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZCCTNULLF, 2, package->str.len, package->str.addr);
	}
	STRNCPY_STR(str_temp_buffer, str_buffer, MAX_TABLINE_LEN);
	val.addr = str_temp_buffer;
	val.len = STRLEN(str_temp_buffer);
	/* Need to copy the str_buffer into another temp variable since
	 * trans_log_name requires input and output buffers to be different.
	 * If there is an env variable present in the pathname, trans_log_name
	 * expands it and return SS_NORMAL. Else it returns SS_NOLOGNAM.
	 * Instead of checking 2 return values, better to check against SS_LOG2LONG
	 * which occurs if the pathname is too long after any kind of expansion.
 	 */
	if (SS_LOG2LONG == trans_log_name(&val, &trans, str_buffer, SIZEOF(str_buffer), dont_sendmsg_on_log2long))
	{
		/* Env variable expansion in the pathname caused buffer overflow */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3, val.len, val.addr, SIZEOF(str_buffer) - 1);
	}
	pakhandle = fgn_getpak(str_buffer, INFO);
	if (NULL == pakhandle)
	{
		/* Unable to obtain handle to the shared library */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZCUNAVAIL, 2, package->str.len, package->str.addr);
	}
	pak = get_memory(SIZEOF(*pak));
	pak->first_entry = 0;
	put_mstr(&package->str, &pak->package_name);
	pak->package_handle = pakhandle;
	pak->package_clnup_rtn = NULL;
	len = STRLEN("GTMSHLIBEXIT");
	/* At this point, we have a valid package, pointed to by pak */
#	ifdef DEBUG_EXTCALL
	FPRINTF(stderr, "GT.M external call opened package name: %s\n", pak->package_name.addr);
#	endif
	for (;;)
	{
		star_found = FALSE;
		tbp = read_table(LIT_AND_LEN(str_buffer), ext_table_file_handle);
		if (NULL == tbp)
			break;
		tbp = exttab_scan_space(str_buffer);
		/* Empty line? */
		if (!*tbp)
			continue;
		/* No, must be entryref or keyword */
		end = scan_ident(tbp);
		if (!end)
			ext_stx_error(ERR_ZCENTNAME, ext_table_file_name);
		keywordlen = end - tbp;
		end = exttab_scan_space(end);
		if ('=' == *end)
		{	/* Keyword before '=' has a string of size == STRLEN("GTMSHLIBEXIT") */
			if (keywordlen == len)
			{
				if (0 == MEMCMP_LIT(tbp, "GTMSHLIBEXIT"))
				{
					/* Skip past the '=' char */
					tbp = exttab_scan_space(end + 1);
					if (*tbp)
					{	/* We have a cleanup routine name */
						clnuprtn.addr = tbp;
						clnuprtn.len = scan_ident(tbp) - tbp;
						clnuprtn.addr[clnuprtn.len] = 0;
						pak->package_clnup_rtn =
						  (clnupfptr)fgn_getrtn(pak->package_handle, &clnuprtn, ERROR);
					} else
						ext_stx_error(ERR_ZCCLNUPRTNMISNG, ext_table_file_name);
					continue;
				}
			}
			ext_stx_error(ERR_ZCINVALIDKEYWORD, ext_table_file_name);
			continue;
		}
		if ('^' == *end)
		{
			end++;
			end = scan_ident(end);
			if (!end)
				ext_stx_error(ERR_ZCENTNAME, ext_table_file_name);
		}
		rtnnam.addr = tbp;
		rtnnam.len = INTCAST(end - tbp);
		tbp = exttab_scan_space(end);
		if (':' != *tbp++)
			ext_stx_error(ERR_ZCCOLON, ext_table_file_name);
		/* Get return type */
		ret_tok = scan_keyword(&tbp);
		/* Check for legal return type */
		switch (ret_tok)
		{
			case ydb_status:
			case ydb_void:
			case ydb_int:
			case ydb_uint:
			case ydb_long:
			case ydb_ulong:
			case ydb_char_star:
			case ydb_float_star:
			case ydb_string_star:
			case ydb_int_star:
			case ydb_uint_star:
			case ydb_long_star:
			case ydb_ulong_star:
			case ydb_double_star:
			case ydb_char_starstar:
			case ydb_pointertofunc:
			case ydb_pointertofunc_star:
			case ydb_jboolean:
			case ydb_jint:
			case ydb_jlong:
			case ydb_jfloat:
			case ydb_jdouble:
			case ydb_jstring:
			case ydb_jbyte_array:
			case ydb_jbig_decimal:
				break;
			default:
				ext_stx_error(ERR_ZCRTNTYP, ext_table_file_name);
		}
		got_status = (ret_tok == ydb_status);
		/* Get call name */
		if ('[' == *tbp)
		{
			if (star_found)
				ret_pre_alloc_val = scan_array_bound(&tbp,ret_tok);
			else
				ext_stx_error(ERR_ZCPREALLVALPAR, ext_table_file_name);
			/* We should allow the pre-allocated value upto to the maximum string size (MAX_STRLEN) plus 1 for the
			 * extra terminating NULL. Negative values would have been caught by scan_array_bound() above.
			 */
			if (ret_pre_alloc_val > MAX_STRLEN + 1)
				ext_stx_error(ERR_ZCPREALLVALINV, ext_table_file_name);
		} else
			ret_pre_alloc_val = -1;
		/* Fix C9E12-002681 */
		if ('%' == *tbp)
			*tbp = '_';
		end = scan_ident(tbp);
		if (!end)
			ext_stx_error(ERR_ZCRCALLNAME, ext_table_file_name);
		callnam.addr = tbp;
		callnam.len = INTCAST(end - tbp);
		tbp = exttab_scan_space(end);
		tbp = exttab_scan_space(tbp);
		for (parameter_count = 0;(MAX_ACTUALS > parameter_count) && (')' != *tbp); parameter_count++)
		{
			star_found = FALSE;
			/* Must have comma if this is not the first parameter, otherwise '(' */
			if (((0 == parameter_count)?'(':',') != *tbp++)
				ext_stx_error(ERR_ZCRPARMNAME, ext_table_file_name);
			tbp = exttab_scan_space(tbp);
			/* Special case: () is ok */
			if ((0 == parameter_count) && (*tbp == ')'))
				break;
			/* Looking for an I, an O or an IO */
			is_input[parameter_count] = is_output[parameter_count] = FALSE;
			if ('I' == *tbp)
			{
				is_input[parameter_count] = TRUE;
				tbp++;
			}
			if ('O' == *tbp)
			{
				is_output[parameter_count] = TRUE;
				tbp++;
			}
			if (((FALSE == is_input[parameter_count]) && (FALSE == is_output[parameter_count]))
			    ||(':' != *tbp++))
				ext_stx_error(ERR_ZCRCALLNAME, ext_table_file_name);
			/* Scanned colon--now get type */
			pr = scan_keyword(&tbp);
			if (ydb_notfound == pr)
				ext_stx_error(ERR_ZCUNTYPE, ext_table_file_name);
			if (ydb_status == pr)
			{
				/* Only one type "status" allowed per call */
				if (got_status)
					ext_stx_error(ERR_ZCMLTSTATUS, ext_table_file_name);
				else
					got_status = TRUE;
			}
			parameter_types[parameter_count] = pr;
			if ('[' == *tbp)
			{
				if (star_found && !is_input[parameter_count])
					parameter_alloc_values[parameter_count] = scan_array_bound(&tbp, pr);
				else
					ext_stx_error(ERR_ZCPREALLVALPAR, ext_table_file_name);
				/* We should allow the pre-allocated value upto to the maximum string size (MAX_STRLEN) plus 1 for
				 * the extra terminating NULL. Negative values would have been caught by scan_array_bound() above.
				 */
				if (parameter_alloc_values[parameter_count] > MAX_STRLEN + 1)
					ext_stx_error(ERR_ZCPREALLVALINV, ext_table_file_name);
			} else
				parameter_alloc_values[parameter_count] = -1;
			tbp = exttab_scan_space(tbp);
		}
		entry_ptr = get_memory(SIZEOF(*entry_ptr));
		entry_ptr->next_entry = pak->first_entry;
		pak->first_entry = entry_ptr;
		entry_ptr->return_type = ret_tok;
		entry_ptr->ret_pre_alloc_val = ret_pre_alloc_val;
		entry_ptr->argcnt = parameter_count;
		entry_ptr->input_mask = array_to_mask(is_input, parameter_count);
		entry_ptr->output_mask = array_to_mask(is_output, parameter_count);
		entry_ptr->parms = get_memory(parameter_count * SIZEOF(entry_ptr->parms[0]));
		entry_ptr->param_pre_alloc_size = get_memory(parameter_count * SIZEOF(intszofptr_t));
		entry_ptr->parmblk_size = (SIZEOF(void *) * parameter_count) + SIZEOF(intszofptr_t);
		for (i = 0 ; i < parameter_count; i++)
		{
			entry_ptr->parms[i] = parameter_types[i];
			assert(ydb_void != parameter_types[i]);
			entry_ptr->parmblk_size += parm_space_needed[parameter_types[i]];
			entry_ptr->param_pre_alloc_size[i] = parameter_alloc_values[i];
		}
		put_mstr(&rtnnam, &entry_ptr->entry_name);
		put_mstr(&callnam, &entry_ptr->call_name);

		/* The reason for passing INFO severity is that PROFILE has several routines listed in
		 * the external call table that are not in the shared library. PROFILE folks would
		 * rather see info/warning messages for such routines at shared library open time,
		 * than error out. These unimplemented routines, they say were not being called from
		 * the application and wouldn't cause any application failures. If we fail to open
		 * the shared libary, or we fail to locate a routine that is called from the
		 * application, we issue rts_error message (in extab_parse.c).
		 */
		entry_ptr->fcn = fgn_getrtn(pak->package_handle, &entry_ptr->call_name, INFO);
#		ifdef DEBUG_EXTCALL
		FPRINTF(stderr, "   package entry point: %s, address: %x\n", entry_ptr->entry_name.addr, entry_ptr->fcn);
#		endif
	}
	FCLOSE(ext_table_file_handle, fclose_res);
	return pak;
}

callin_entry_list* citab_parse (boolean_t internal_use)
{
	int			parameter_count, i, fclose_res;
	uint4			inp_mask, out_mask, mask;
	mstr			labref, callnam;
	enum ydb_types		ret_tok, parameter_types[MAX_ACTUALS], pr;
	char			str_buffer[MAX_TABLINE_LEN], *tbp, *end, rcfpath[YDB_PATH_MAX];
	FILE			*ext_table_file_handle;
	callin_entry_list	*entry_ptr = NULL, *save_entry_ptr = NULL;
	boolean_t		is_ydb_env_match;
	int			nbytes;
	char			tmpbuff[256];

	if (!internal_use)
	{
		ext_table_file_name = ydb_getenv(YDBENVINDX_CI, NULL_SUFFIX, &is_ydb_env_match);
		if (!ext_table_file_name) /* environment variable not set */
		{
			nbytes = SNPRINTF(tmpbuff, SIZEOF(tmpbuff), "%s/%s",
						ydbenvname[YDBENVINDX_CI] + 1, gtmenvname[YDBENVINDX_CI] + 1);
			if ((0 < nbytes) && (SIZEOF(str_buffer) > nbytes))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CITABENV, 2, LEN_AND_STR(tmpbuff));
			else
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
						LEN_AND_LIT("SNPRINTF(citab_parse)"), CALLFROM, errno);
		}
	} else
	{
		SNPRINTF(rcfpath, YDB_PATH_MAX, "%s/%s", ydb_dist, COMM_FILTER_FILENAME);
		ext_table_file_name = rcfpath;
	}
	Fopen(ext_table_file_handle, ext_table_file_name, "r");
	if (!ext_table_file_handle) /* call-in table not found */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(11) ERR_CITABOPN, 2, LEN_AND_STR(ext_table_file_name),
			  ERR_SYSCALL, 5, LEN_AND_LIT("fopen"), CALLFROM, errno);
	ext_source_line_num = 0;
	while (read_table(LIT_AND_LEN(str_buffer), ext_table_file_handle))
	{
		if (!*(tbp = exttab_scan_space(str_buffer)))
			continue;
		if (!(end = scan_ident(tbp)))
			ext_stx_error(ERR_CIRCALLNAME, ext_table_file_name);
		callnam.addr = tbp;
		callnam.len = INTCAST(end - tbp);
		tbp = exttab_scan_space(end);
		if (':' != *tbp++)
			ext_stx_error(ERR_COLON, ext_table_file_name);
		ret_tok = scan_keyword(&tbp); /* return type */
		switch (ret_tok) /* return type valid ? */
		{
			case ydb_void:
			case ydb_char_star:
			case ydb_int_star:
			case ydb_uint_star:
			case ydb_long_star:
			case ydb_ulong_star:
			case ydb_float_star:
			case ydb_double_star:
			case ydb_string_star:
			case ydb_jboolean:
			case ydb_jint:
			case ydb_jlong:
			case ydb_jfloat:
			case ydb_jdouble:
			case ydb_jstring:
			case ydb_jbyte_array:
			case ydb_jbig_decimal:
				break;
			default:
				ext_stx_error(ERR_CIRTNTYP, ext_table_file_name);
		}
		labref.addr = tbp;
		if ((end = scan_labelref(tbp)))
			labref.len = INTCAST(end - tbp);
		else
			ext_stx_error(ERR_CIENTNAME, ext_table_file_name);
		tbp = exttab_scan_space(end);
		inp_mask = out_mask = 0;
		for (parameter_count = 0; (*tbp && ')' != *tbp); parameter_count++)
		{
			if (MAX_ACTUALS <= parameter_count)
				ext_stx_error(ERR_CIMAXPARAM, ext_table_file_name);
			/* must have comma if this is not the first parameter, otherwise '(' */
			if (((0 == parameter_count)?'(':',') != *tbp++)
				ext_stx_error(ERR_CIRPARMNAME, ext_table_file_name);
			tbp = exttab_scan_space(tbp);
			if ((0 == parameter_count) && (*tbp == ')')) /* special case () */
				break;
			/* looking for an I, a O or an IO */
			mask = (1 << parameter_count);
			inp_mask |= ('I' == *tbp) ? (tbp++, mask) : 0;
			out_mask |= ('O' == *tbp) ? (tbp++, mask) : 0;
			if ((!(inp_mask & mask) && !(out_mask & mask)) || (':' != *tbp++))
				ext_stx_error(ERR_CIDIRECTIVE, ext_table_file_name);
			switch ((pr = scan_keyword(&tbp))) /* valid param type? */
			{
				case ydb_int:
				case ydb_uint:
				case ydb_long:
				case ydb_ulong:
				case ydb_float:
				case ydb_double:
					if (out_mask & mask)
						ext_stx_error(ERR_CIPARTYPE, ext_table_file_name);
					/* fall-thru */
				case ydb_char_star:
				case ydb_int_star:
				case ydb_uint_star:
				case ydb_long_star:
				case ydb_ulong_star:
				case ydb_float_star:
				case ydb_double_star:
				case ydb_string_star:
				case ydb_jboolean:
				case ydb_jint:
				case ydb_jlong:
				case ydb_jfloat:
				case ydb_jdouble:
				case ydb_jstring:
				case ydb_jbyte_array:
				case ydb_jbig_decimal:
					break;
				default:
					ext_stx_error(ERR_CIUNTYPE, ext_table_file_name);
			}
			parameter_types[parameter_count] = pr;
			tbp = exttab_scan_space(tbp);
		}
		if (!*tbp)
			ext_stx_error(ERR_CIRPARMNAME, ext_table_file_name);
		entry_ptr = get_memory(SIZEOF(callin_entry_list));
		entry_ptr->next_entry = save_entry_ptr;
		save_entry_ptr = entry_ptr;
		entry_ptr->return_type = ret_tok;
		entry_ptr->argcnt = parameter_count;
		entry_ptr->input_mask = inp_mask;
		entry_ptr->output_mask = out_mask;
		entry_ptr->parms = get_memory(parameter_count * SIZEOF(entry_ptr->parms[0]));
		for (i = 0 ; i < parameter_count; i++)
			entry_ptr->parms[i] = parameter_types[i];
		put_mstr(&labref, &entry_ptr->label_ref);
		put_mstr(&callnam, &entry_ptr->call_name);
	}
	FCLOSE(ext_table_file_handle, fclose_res);
	return entry_ptr;
}

STATICFNDEF void ext_stx_error(int in_error, ...)
{
	va_list	args;
	char	*ext_table_name;
	char	buf[MAX_SRC_LINE], *b;
	int	num_tabs, num_spaces;

	va_start(args, in_error);
	ext_table_name = va_arg(args, char *);
	va_end(args);

	num_tabs = ext_source_column/TABLEN;
	num_spaces = ext_source_column%TABLEN;

	b = &buf[0];
	memset(buf, '\t', num_tabs+2);
	b += num_tabs+NUM_TABS_FOR_GTMERRSTR;
	memset(b, ' ', num_spaces);
	b += num_spaces;
	memcpy(b, "^-----", POINTER_SIZE);
	b += POINTER_SIZE;
	*b = 0;

	dec_err(VARLSTCNT(6) ERR_EXTSRCLIN, 4, ext_source_line_len, ext_source_line, b - &buf[0], &buf[0]);
	dec_err(VARLSTCNT(6) ERR_EXTSRCLOC, 4, ext_source_column, ext_source_line_num, LEN_AND_STR(ext_table_name));
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) in_error);
}
