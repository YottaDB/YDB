/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	ydbxc_types.h - GT.M, Unix Edition External Call type definitions.  */
#ifndef YDBXC_TYPES_H
#define YDBXC_TYPES_H

#include <sys/types.h>	/* For intptr_t */
#include "inttypes.h"	/* .. ditto (defined different places in different platforms) .. */

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
	ydb_long_t	length;
	ydb_char_t	*address;
}	ydb_string_t;

typedef intptr_t	ydb_tid_t;

typedef void		*ydb_fileid_ptr_t;
typedef struct
{
        ydb_string_t	rtn_name;
        void*		handle;
} ci_name_descriptor;

/* Java types with special names for clarity. */
typedef ydb_int_t		ydb_jboolean_t;
typedef ydb_int_t		ydb_jint_t;
typedef ydb_long_t		ydb_jlong_t;
typedef ydb_float_t		ydb_jfloat_t;
typedef ydb_double_t		ydb_jdouble_t;
typedef ydb_char_t		ydb_jstring_t;
typedef ydb_char_t		ydb_jbyte_array_t;
typedef ydb_char_t		ydb_jbig_decimal_t;

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

/* Other entry points accessable in libgtmshr. */
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

#endif /* YDBXC_TYPES_H */
