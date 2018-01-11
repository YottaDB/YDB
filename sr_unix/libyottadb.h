/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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

/* Note: Some constructs in this header file were taken from gtmxc_types.h which has an
 * FIS copyright hence the copyright is copied over here even though this header file was
 * not created by FIS.
 */

/*	libyottadb.h - YottaDB type and routine definitions for call_ins, call-outs, plugins, etc.  */
#ifndef LIBYOTTADB_TYPES_H
#define LIBYOTTADB_TYPES_H

#include <sys/types.h>	/* For intptr_t */
#include "inttypes.h"	/* .. ditto (defined different places in different platforms) .. */

/* Enumerated parameter values */
typedef enum
{
	YDB_DEL_TREE = 1,
	YDB_DEL_NODE,
} ydb_delete_method;

/* Maximum values */
#define YDB_MAX_SUBS	31	/* Maximum subscripts currently supported */
#define YDB_MAX_IDENT	31	/* Maximum size of global/local name (not including '^') */
#define YDB_MAX_NAMES	31	/* Maximum number of variable names can be specified in a single ydb_*_s() call */
/* Note YDB_MAX_NAMES may be temporary and currently only relates to ydb_delete_excl() */

/* Non-error return codes (all positive) */
#define YDB_OK		0		/* Successful return code */
/* Keep the below YDB_* macros a very high positive error # to avoid intersection with system errors #s
 * (e.g. EINTR etc. all of which are mostly <= 1024). Can use INT_MAX but not sure if that will change
 * in the future to a higher value in <limits.h> so set YDB_INT_MAX to the hardcoded fixed value of ((2**31)-1).
 */
#define YDB_INT_MAX	((int)2147483647)
#define	YDB_TP_RESTART	(YDB_INT_MAX - 1)
#define	YDB_TP_ROLLBACK	(YDB_INT_MAX - 2)
#define YDB_NODE_END	(YDB_INT_MAX - 3)

/* Miscellaneous defines */
#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define FALSE 0
#endif
#ifndef NULL
#  define NULL ((void *)0)
#endif

/* Macro to create/fill-in a ydb_buffer_t structure from a literal - use - literal varnames, subscripts
 * or values.
 */
#define YDB_STRLIT_TO_BUFFER(BUFFERP, LITERAL)					\
{										\
	(BUFFERP)->buf_addr = LITERAL;						\
	(BUFFERP)->len_used = (BUFFERP)->len_alloc = sizeof(LITERAL) - 1;	\
}

/* Macro to create/fill-in a ydb_buffer_t structure from a string (i.e. "char *" pointer in C).
 * User of this macro needs to include <string.h> (needed for "strlen" prototype).
 */
#define YDB_STR_TO_BUFFER(BUFFERP, STRING)					\
{										\
	(BUFFERP)->buf_addr = STRING;						\
	(BUFFERP)->len_used = (BUFFERP)->len_alloc = strlen(STRING);		\
}

/* Macro to verify an assertion is true before proceeding - put expression to test as the parameter */
#define YDB_ASSERT(X)														\
{																\
	if (!(X))														\
	{															\
		fprintf(stderr, "YDB_ASSERT: ** Assert failed (%s) at line %d in routine %s - generating core\n",		\
			#X, __LINE__, __FILE__);										\
		fflush(stderr);													\
		ydb_fork_n_core();												\
		exit(1);													\
	}															\
}

/* If only want assertions in DEBUG mode (-DDEBUG option specified), use this macro instead */
#ifdef DEBUG
#  define YDB_ASSERT_DBG(X)	YDB_ASSERT(X)
#else
#  define YDB_ASSERT_DBG(X)
#endif

/* Basic/standard types */
typedef int		ydb_status_t;
typedef	int		ydb_int_t;
typedef unsigned int 	ydb_uint_t;
typedef	long		ydb_long_t;
typedef unsigned long 	ydb_ulong_t;
typedef	float		ydb_float_t;
typedef	double		ydb_double_t;
typedef	char		ydb_char_t;
typedef int		(*ydb_pointertofunc_t)();

/* Structure for passing (non-NULL-terminated) character arrays whose length corresponds to the value of the
 * 'length' field. Note that for output-only ydb_string_t * arguments the 'length' field is set to the
 * preallocation size of the buffer pointed to by 'address', while the first character of the buffer is '\0'.
 */
typedef struct
{
	ydb_ulong_t	length;
	ydb_char_t	*address;
} ydb_string_t;

/* Structure for interfacing with simple API routines. Meant to be used with the YDB_* macros defined
 * above in this header file and with simple API calls.
 */
typedef struct
{
	ydb_uint_t	len_alloc;
	ydb_uint_t	len_used;
	ydb_char_t	*buf_addr;
} ydb_buffer_t;

typedef intptr_t	ydb_tid_t;		/* Timer id */
typedef void		*ydb_fileid_ptr_t;

/* Structure used to reduce name lookup for callins. The handle field (should be zeroed on first call) after
 * the first call using ydb_cij() contains information that allows quick resolution of the routine for a
 * faster call.
 */
typedef struct
{
        ydb_string_t	rtn_name;
        void*		handle;
} ci_name_descriptor;

/* Java types with special names for clarity. */
typedef ydb_int_t	ydb_jboolean_t;
typedef ydb_int_t	ydb_jint_t;
typedef ydb_long_t	ydb_jlong_t;
typedef ydb_float_t	ydb_jfloat_t;
typedef ydb_double_t	ydb_jdouble_t;
typedef ydb_char_t	ydb_jstring_t;
typedef ydb_char_t	ydb_jbyte_array_t;
typedef ydb_char_t	ydb_jbig_decimal_t;

/* Call-in interface. */
ydb_status_t 	ydb_ci(const char *c_rtn_name, ...);
ydb_status_t 	ydb_cip(ci_name_descriptor *ci_info, ...);
ydb_status_t 	ydb_init(void);
#ifdef GTM_PTHREAD
ydb_status_t 	ydb_jinit(void);
#endif
ydb_status_t 	ydb_exit(void);
ydb_status_t	ydb_cij(const char *c_rtn_name, char **arg_blob, int count, int *arg_types, unsigned int *io_vars_mask,
		unsigned int *has_ret_value);
void 		ydb_zstatus(char* msg, int len);

/* Other entry points accessable in libyottadb.so */
ydb_status_t	gtm_filename_to_id(ydb_string_t *filename, ydb_fileid_ptr_t *fileid);
void		gtm_hiber_start(ydb_uint_t mssleep);
void		gtm_hiber_start_wait_any(ydb_uint_t mssleep);
void		gtm_start_timer(ydb_tid_t tid, ydb_int_t time_to_expir, void (*handler)(), ydb_int_t hdata_len, void *hdata);
void		gtm_cancel_timer(ydb_tid_t tid);
ydb_status_t	gtm_is_file_identical(ydb_fileid_ptr_t fileid1, ydb_fileid_ptr_t fileid2);
void		gtm_xcfileid_free(ydb_fileid_ptr_t fileid);
int		gtm_is_main_thread(void);
void 		*gtm_malloc(size_t);
void 		gtm_free(void *);
void		ydb_fork_n_core(void);

typedef int	(*ydb_tpfnptr_t)(void *tpfnparm);

/* Simple API routine declarations */
int ydb_data_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, unsigned int *ret_value);
int ydb_delete_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_delete_method delete_method);
int ydb_delete_excl_s(int namecount, ydb_buffer_t *varnames);
int ydb_set_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *value);
int ydb_get_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value);
int ydb_subscript_next_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value);
int ydb_subscript_previous_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *ret_value);
int ydb_node_next_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int *ret_subs_used,
		    ydb_buffer_t *ret_subsarray);
int ydb_node_previous_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, int *ret_subs_used,
			ydb_buffer_t *ret_subsarray);
int ydb_incr_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray, ydb_buffer_t *increment, ydb_buffer_t *ret_value);
int ydb_str2zwr_s(ydb_buffer_t *str, ydb_buffer_t *zwr);
int ydb_zwr2str_s(ydb_buffer_t *zwr, ydb_buffer_t *str);
int ydb_tp_s(ydb_tpfnptr_t tpfn, void *tpfnparm, const char *transid, const char *varnamelist);
void ydb_fork_n_core(void);

/* Comprehensive API routine declarations */
int ydb_child_init(void *param);

#endif /* LIBYOTTADB_TYPES_H */
