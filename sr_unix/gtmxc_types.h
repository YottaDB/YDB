/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	gtmxc_types.h - GT.M, Unix Edition External Call type definitions.  */

#ifdef __osf__
/* Ensure 32-bit pointers for compatibility with GT.M internal representations.  */
#pragma pointer_size (save)
#pragma pointer_size (short)
#endif


typedef int		xc_status_t;

#ifdef __osf__
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
	int		length;
	xc_char_t	*address;
}	xc_string_t;

#ifdef __osf__
#pragma pointer_size (restore)
#endif

/* new types for external/call-in user - xc_* types still valid for backward compatibility */
typedef xc_status_t	gtm_status_t;
typedef xc_long_t	gtm_long_t;
typedef xc_ulong_t	gtm_ulong_t;
typedef xc_float_t	gtm_float_t;
typedef xc_double_t	gtm_double_t;
typedef xc_char_t	gtm_char_t;
typedef xc_string_t	gtm_string_t;
typedef xc_pointertofunc_t	gtm_pointertofunc_t;

/* call-in interface */
xc_status_t 	gtm_ci(const char *c_rtn_name, ...);
xc_status_t 	gtm_init(void);
xc_status_t 	gtm_exit(void);
void 		gtm_zstatus(char* msg, int len);
