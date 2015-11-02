/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

#ifdef __osf__
/* Ensure 32-bit pointers for compatibility with GT.M internal representations.  */
#pragma pointer_size (save)
#pragma pointer_size (short)
#endif

typedef int		xc_status_t;
typedef	int		xc_int_t;
typedef unsigned int 	xc_uint_t;

#if defined(__osf__)
typedef	int		xc_long_t;
typedef unsigned int 	xc_ulong_t;
#else
typedef	long		xc_long_t;
typedef unsigned long 	xc_ulong_t;
#endif

typedef	float		xc_float_t;

typedef	double		xc_double_t;

typedef	char		xc_char_t;

typedef int		(*xc_pointertofunc_t)();

typedef struct
{
	xc_long_t	length;
	xc_char_t	*address;
}	xc_string_t;

#ifdef __osf__
#pragma pointer_size (restore)
#endif

/* new types for external/call-in user - xc_* types still valid for backward compatibility */
typedef xc_status_t	gtm_status_t;
typedef xc_int_t	gtm_int_t;
typedef xc_uint_t	gtm_uint_t;
typedef xc_long_t	gtm_long_t;
typedef xc_ulong_t	gtm_ulong_t;
typedef xc_float_t	gtm_float_t;
typedef xc_double_t	gtm_double_t;
typedef xc_char_t	gtm_char_t;
typedef xc_string_t	gtm_string_t;
typedef xc_pointertofunc_t	gtm_pointertofunc_t;

typedef struct
{
        gtm_string_t rtn_name;
        void* handle;
} ci_name_descriptor;

/* call-in interface */
xc_status_t 	gtm_ci(const char *c_rtn_name, ...);
xc_status_t 	gtm_cip(ci_name_descriptor *ci_info, ...);
xc_status_t 	gtm_init(void);
xc_status_t 	gtm_exit(void);
void 		gtm_zstatus(char* msg, int len);

typedef	int	gtmcrypt_key_t;

typedef void	*xc_fileid_ptr_t;
xc_status_t	gtm_filename_to_id(xc_string_t *filename, xc_fileid_ptr_t *fileid);
xc_status_t	gtm_is_file_identical(xc_fileid_ptr_t fileid1, xc_fileid_ptr_t fileid2);
void		gtm_xcfileid_free(xc_fileid_ptr_t fileid);

void 		*gtm_malloc(size_t);
void 		gtm_free(void *);

#endif /* GTMXC_TYPES_H */
