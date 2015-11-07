/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	gtmxc_types.h - GT.M, Unix Edition External Call type definitions.  */
#ifndef GTMXC_TYPES_H
#define GTMXC_TYPES_H

#include <sys/types.h>	/* For intptr_t */
#include "inttypes.h"	/* .. ditto (defined different places in different platforms) .. */

#ifdef __osf__
/* Ensure 32-bit pointers for compatibility with GT.M internal representations.  */
#pragma pointer_size (save)
#pragma pointer_size (short)
#endif
typedef int		gtm_status_t;
typedef	int		gtm_int_t;
typedef unsigned int 	gtm_uint_t;
#if defined(__osf__)
typedef	int		gtm_long_t;
typedef unsigned int 	gtm_ulong_t;
#else
typedef	long		gtm_long_t;
typedef unsigned long 	gtm_ulong_t;
#endif
typedef	float		gtm_float_t;
typedef	double		gtm_double_t;
typedef	char		gtm_char_t;
typedef int		(*gtm_pointertofunc_t)();
typedef struct
{
	gtm_long_t	length;
	gtm_char_t	*address;
}	gtm_string_t;
typedef	int	gtmcrypt_key_t;
#ifdef __osf__
#pragma pointer_size (restore)
#endif

#if !defined(__alpha)
typedef intptr_t	gtm_tid_t;
#else
typedef int		gtm_tid_t;
#endif

typedef void		*gtm_fileid_ptr_t;
typedef struct
{
        gtm_string_t	rtn_name;
        void*		handle;
} ci_name_descriptor;

/* Define deprecated types for backward compatibility. */
typedef gtm_status_t	xc_status_t;
typedef gtm_int_t	xc_int_t;
typedef gtm_uint_t	xc_uint_t;
typedef gtm_long_t	xc_long_t;
typedef gtm_ulong_t	xc_ulong_t;
typedef gtm_float_t	xc_float_t;
typedef gtm_double_t	xc_double_t;
typedef gtm_char_t	xc_char_t;
typedef gtm_string_t	xc_string_t;
typedef gtm_pointertofunc_t	xc_pointertofunc_t;
typedef gtm_fileid_ptr_t	xc_fileid_ptr_t;

/* Java types with special names for clarity. */
typedef gtm_int_t	gtm_jboolean_t;
typedef gtm_int_t	gtm_jint_t;
typedef gtm_long_t	gtm_jlong_t;
typedef gtm_float_t	gtm_jfloat_t;
typedef gtm_double_t	gtm_jdouble_t;
typedef gtm_char_t	gtm_jstring_t;
typedef gtm_char_t	gtm_jbyte_array_t;
typedef gtm_char_t	gtm_jbig_decimal_t;

/* Call-in interface. */
gtm_status_t 	gtm_ci(const char *c_rtn_name, ...);
gtm_status_t 	gtm_cip(ci_name_descriptor *ci_info, ...);
gtm_status_t 	gtm_init(void);
#ifdef GTM_PTHREAD
gtm_status_t 	gtm_jinit(void);
#endif
gtm_status_t 	gtm_exit(void);
gtm_status_t	gtm_cij(const char *c_rtn_name, char **arg_blob, int count, int *arg_types, unsigned int *io_vars_mask,
		unsigned int *has_ret_value);
void 		gtm_zstatus(char* msg, int len);

/* Other entry points accessable in libgtmshr. */
gtm_status_t	gtm_filename_to_id(gtm_string_t *filename, gtm_fileid_ptr_t *fileid);
void		gtm_hiber_start(gtm_uint_t mssleep);
void		gtm_hiber_start_wait_any(gtm_uint_t mssleep);
void		gtm_start_timer(gtm_tid_t tid, gtm_int_t time_to_expir, void (*handler)(), gtm_int_t hdata_len, void *hdata);
void		gtm_cancel_timer(gtm_tid_t tid);
gtm_status_t	gtm_is_file_identical(gtm_fileid_ptr_t fileid1, gtm_fileid_ptr_t fileid2);
void		gtm_xcfileid_free(gtm_fileid_ptr_t fileid);
int		gtm_is_main_thread(void);
void 		*gtm_malloc(size_t);
void 		gtm_free(void *);

#endif /* GTMXC_TYPES_H */
