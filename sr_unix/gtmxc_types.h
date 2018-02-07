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

/* The current version of this header file is now called libyottadb.h. This header file contains those
 * definitions (based on the base types defined in libyottadb.h) that enable compatibility with exiting
 * GT.M shops supporting the gtm_* types and routines and even the deprecated xc_* types. This allows
 * shops used to GT.M to "just work" using previously supported type/routine names.
 */
#ifndef GTMXC_TYPES_H
#define GTMXC_TYPES_H

#include "libyottadb.h"

/* Define types for GT.M backward compatibility. */
typedef ydb_status_t		gtm_status_t;
typedef ydb_int_t		gtm_int_t;
typedef ydb_uint_t		gtm_uint_t;
typedef ydb_long_t		gtm_long_t;
typedef ydb_ulong_t		gtm_ulong_t;
typedef ydb_float_t		gtm_float_t;
typedef ydb_double_t		gtm_double_t;
typedef ydb_char_t		gtm_char_t;
typedef ydb_string_t		gtm_string_t;
typedef ydb_pointertofunc_t	gtm_pointertofunc_t;
typedef ydb_fileid_ptr_t	gtm_fileid_ptr_t;
typedef ydb_tid_t		gtm_tid_t;

/* Define deprecated types for backward compatibility. */
typedef ydb_status_t		xc_status_t;
typedef ydb_int_t		xc_int_t;
typedef ydb_uint_t		xc_uint_t;
typedef ydb_long_t		xc_long_t;
typedef ydb_ulong_t		xc_ulong_t;
typedef ydb_float_t		xc_float_t;
typedef ydb_double_t		xc_double_t;
typedef ydb_char_t		xc_char_t;
typedef ydb_string_t		xc_string_t;
typedef ydb_pointertofunc_t	xc_pointertofunc_t;
typedef ydb_fileid_ptr_t	xc_fileid_ptr_t;

/* Java types for GTM backward compatibility */
typedef ydb_int_t		gtm_jboolean_t;
typedef ydb_int_t		gtm_jint_t;
typedef ydb_long_t		gtm_jlong_t;
typedef ydb_float_t		gtm_jfloat_t;
typedef ydb_double_t		gtm_jdouble_t;
typedef ydb_char_t		gtm_jstring_t;
typedef ydb_char_t		gtm_jbyte_array_t;
typedef ydb_char_t		gtm_jbig_decimal_t;

/* GT.M flavor utility routines */
ydb_status_t	gtm_is_file_identical(ydb_fileid_ptr_t fileid1, ydb_fileid_ptr_t fileid2);
ydb_status_t	gtm_filename_to_id(ydb_string_t *filename, ydb_fileid_ptr_t *fileid);
int		gtm_is_main_thread(void);
void		gtm_xcfileid_free(ydb_fileid_ptr_t fileid);
void		gtm_cancel_timer(ydb_tid_t tid);
void 		*gtm_malloc(size_t);
void 		gtm_free(void *);
void		gtm_hiber_start(ydb_uint_t mssleep);
void		gtm_hiber_start_wait_any(ydb_uint_t mssleep);
void		gtm_start_timer(ydb_tid_t tid, ydb_int_t time_to_expir, void (*handler)(), ydb_int_t hdata_len, void *hdata);

/* The java plug-in has some very direct references to some of these routines that
 * cannot be changed by the pre-processor so for now, we have some stub routines
 * that take care of the translation. These routines are exported along with their
 * ydb_* variants.
 */
#ifdef GTM_PTHREAD
ydb_status_t 	gtm_jinit(void);
#endif
ydb_status_t 	gtm_exit(void);
ydb_status_t	gtm_cij(const char *c_rtn_name, char **arg_blob, int count, int *arg_types, unsigned int *io_vars_mask,
			unsigned int *has_ret_value);
void 		gtm_zstatus(char* msg, int len);

/* Define backward compatibility routine name defines for GT.M entry points. Note some of these entry points are
 * the same as entry points defined above. This allows those references that can be transformed to effect a direct
 * call without the gtm_xxx wrapper that it would otherwise use also eliminating an extra level of indirection.
 */
#define gtm_ci				ydb_ci
#define gtm_cip				ydb_cip
#define gtm_init			ydb_init
#ifdef GTM_PTHREAD
# define gtm_jinit			ydb_jinit
#endif
#define gtm_exit			ydb_exit
#define gtm_cij				ydb_cij
#define gtm_zstatus			ydb_zstatus

#endif /* GTMXC_TYPES_H */
