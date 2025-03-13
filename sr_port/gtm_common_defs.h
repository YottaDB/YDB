/****************************************************************
 *								*
 * Copyright (c) 2012-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 * Copyright (c) 2018 Stephen L Johnson. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_COMMON_DEFS_H
#define GTM_COMMON_DEFS_H

#if defined(__x86_64__) || defined(__s390__) || defined (_AIX) || defined (__aarch64__)
#  define GTM64
#endif

#ifdef GTM64
#  define GTM64_ONLY(X)		X
#  define NON_GTM64_ONLY(X)
#else
#  define GTM64_ONLY(X)
#  define NON_GTM64_ONLY(X)	X
#endif

#define readonly
#define GBLDEF
#define GBLREF		extern
#define LITDEF		const
#define LITREF		extern const
#define error_def(x)	/* No longer used as errors are now #define values */

/* Note that a STATICREF for variables does not make sense since statics are supposed to be used only within one module. */
#define	STATICDEF		static
#define	STATICFNDCL		static
#define	STATICFNDEF		static

#ifndef TRUE
#  define TRUE			1
#endif
#ifndef FALSE
#  define FALSE			0
#endif
#ifndef NULL
#  define NULL			((void *) 0)
#endif

#if defined(__MVS__)
#  define INTCAST(X)		((int)(X))
#  define UINTCAST(X)		((uint4)(X))
#  define STRLEN(X)		((int)(strlen(X)))
#  define USTRLEN(X)		((unsigned int)(strlen(X)))
#  define OFFSETOF(X,Y)		((int)(offsetof(X,Y)))
#else
#  define INTCAST(X)		X
#  define UINTCAST(X)		X
#  define STRLEN(X)		strlen(X)
#  define USTRLEN(X)		strlen(X)
#  define OFFSETOF(X,Y)		offsetof(X,Y)
#endif

#define DIR_SEPARATOR		'/'
#define UNALIAS			"unalias -a; "

/* Below macro allows us to mark parameters/return-values as unused and avoid warnings from the compiler/clang-tidy etc. */
#define UNUSED(x) (void)(x)

/* the LITERAL version of the macro should be used over STRING whenever possible for efficiency reasons */
#define	STR_LIT_LEN(LITERAL)			(SIZEOF(LITERAL) - 1)
#define	LITERAL_AND_LENGTH(LITERAL)		(LITERAL), (SIZEOF(LITERAL) - 1)
#define	LENGTH_AND_LITERAL(LITERAL)		(SIZEOF(LITERAL) - 1), (LITERAL)
#define	STRING_AND_LENGTH(STRING)		(STRING), (STRLEN((char *)(STRING)))
#define	LENGTH_AND_STRING(STRING)		(strlen((char *)(STRING))), (STRING)

#define	LEN_AND_LIT(LITERAL)			LENGTH_AND_LITERAL(LITERAL)
#define	LIT_AND_LEN(LITERAL)			LITERAL_AND_LENGTH(LITERAL)
#define	STR_AND_LEN(STRING)			STRING_AND_LENGTH(STRING)
#define	LEN_AND_STR(STRING)			LENGTH_AND_STRING(STRING)

#define	ARRAYSIZE(arr)				SIZEOF(arr)/SIZEOF(arr[0])	/* # of elements defined in the array */
#define	ARRAYTOP(arr)				(&arr[0] + ARRAYSIZE(arr))	/* address of the TOP of the array (first byte AFTER
										 * array limits).Use &arr[0] + size instead of
										 * &arr[size] to avoid compiler warning.
							 			 */

#define	MEMCMP_LIT(SOURCE, LITERAL)		memcmp(SOURCE, LITERAL, SIZEOF(LITERAL) - 1)
#define MEMCPY_LIT(TARGET, LITERAL)		memcpy((void *)TARGET, LITERAL, SIZEOF(LITERAL) - 1)

#define DIVIDE_ROUND_UP(VALUE, MODULUS)		(((VALUE) + ((MODULUS) - 1)) / (MODULUS))
#define DIVIDE_ROUND_DOWN(VALUE, MODULUS)	((VALUE) / (MODULUS))
#define ROUND_UP(VALUE, MODULUS)		(DIVIDE_ROUND_UP(VALUE, MODULUS) * (MODULUS))
#define ROUND_DOWN(VALUE, MODULUS)		(DIVIDE_ROUND_DOWN(VALUE, MODULUS) * (MODULUS))

/* Macros to enable block macros to be used in any context taking a single statement. See MALLOC_* macros below for examples of use.
 * Note that if the macro block does a "break" or "continue" and expects it to transfer control to the calling context
 * (i.e. OUTSIDE the macro block because a for/while/switch exists outside the macro block), these macros should not be used.
 * This means that a "break" or "continue" inside a for/while/switch statement in the macro block which transfers control to
 * within the macro block is not an issue.
 */
#define MBSTART		do
#define MBEND		while (FALSE)

#define	MALLOC_CPY_LIT(DST, SRC)		\
MBSTART {					\
	char	*mcs_ptr;			\
	int	mcs_len;			\
						\
	mcs_len = SIZEOF(SRC);			\
	mcs_ptr = malloc(mcs_len);		\
	memcpy(mcs_ptr, SRC, mcs_len);		\
	DST = mcs_ptr;				\
} MBEND

#define MALLOC_INIT(DST, SIZ)			\
MBSTART {					\
	void	*lcl_ptr;			\
						\
	lcl_ptr = malloc(SIZ);			\
	memset(lcl_ptr, 0, SIZ);		\
	DST = lcl_ptr;				\
} MBEND

/* Macro to define the exit handler and optionally set an atexit() for it */
#define DEFINE_EXIT_HANDLER(EXITHNDLR, ATEXIT)		\
MBSTART {						\
	GBLREF void	(*exit_handler_fptr)();		\
	GBLREF void	(*primary_exit_handler)(void);	\
							\
	exit_handler_fptr = &EXITHNDLR;			\
	if (ATEXIT)					\
	{						\
		atexit(EXITHNDLR);			\
		primary_exit_handler = EXITHNDLR;	\
	}						\
} MBEND

/* Macro to drive the exit handler if it exists  */
#define DRIVE_EXIT_HANDLER_IF_EXISTS					\
MBSTART {								\
	GBLREF void (*exit_handler_fptr)();				\
									\
	assert(NULL != chnd);	/* Verify err_init() was done first */	\
	if (NULL != exit_handler_fptr)					\
		(*exit_handler_fptr)();					\
} MBEND


/* Shared between GT.M and external plugins */
#define EXT_NEW 		"_%YGTM"

/* Define types that are needed by external plugins (e.g. encryption plugin) here (and not in mdef.h) */

/* mstr needs to be defined before including "mdefsp.h".  */
#define	MSTR_LEN_MAX	INT_MAX
typedef	int	mstr_len_t;		/* Change MSTR_LEN_MAX if this changes */
typedef struct
{
	unsigned int	char_len;	/* Character length */
	mstr_len_t	len;
	char		*addr;
} mstr;

#include <stdint.h>

typedef int32_t		int4;		/* 4-byte signed integer */
typedef uint32_t	uint4;		/* 4-byte unsigned integer */
typedef uint64_t	ydb_uint8;
typedef int		boolean_t;	/* boolean type */

#endif /* GTM_COMMON_DEFS_H */
