/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MDESP_included
#define MDESP_included

typedef          long	int4;		/* 4-byte signed integer */
typedef unsigned long	uint4;		/* 4-byte unsigned integer */

/* Starting off life as debugging parms and now we need them for the
   short term so define them here */
#define DEBUG_LEAVE_SM
#define DEBUG_NOMSYNC

#define readonly
#define GBLDEF
#define GBLREF extern
#define LITDEF
#define LITREF extern
#define error_def(x) LITREF int x
#ifdef DEBUG
error_def(ERR_ASSERT);
#define assert(x) ((x) ? 1 : rts_error(VARLSTCNT(5) ERR_ASSERT, 3, sizeof(__FILE__) - 1, __FILE__, __LINE__))
#else
#define assert(x)
#endif

#ifdef __sparc /* Not yet for hppa */
#define CACHELINE_SIZE        256
#define MSYNC_ADDR_INCS        OS_PAGE_SIZE

#define OFF_T_LONG
#endif

#define INTERLOCK_ADD(X,Y,Z)	(add_inter(Z, (sm_int_ptr_t)(X), (sm_int_ptr_t)(Y)))

/* #define memcmp(DST,SRC,LEN) memucmp(DST,SRC,LEN) */

#define UNIX 1
#undef VMS
#define PADDING 1
#define BIGENDIAN 1
#define CNTR_WORD_32

/* Reserve enough space in routine header for the dummy string "GTM_CODE".  */
#define RHEAD_JSB_SIZE	8

typedef struct
{ 	unsigned int	sgn : 1 ;
	unsigned int	  e : 7 ;
		int4	m[2]	;
} mflt ;

typedef struct
{ 		char	mvtype	;
	unsigned int	sgn : 1 ;
	unsigned int	  e : 7 ;
		mstr	str	;
		int4	m[2]	;
} mval ;

#define DEFINE_MVAL_LITERAL(TYPE, EXPONENT, SIGN, LENGTH, ADDRESS, MANT_LOW, MANT_HIGH) \
	{TYPE, SIGN, EXPONENT, LENGTH, ADDRESS, MANT_LOW, MANT_HIGH}

#define VAR_START(a)	va_start(a)
#define VARLSTCNT(a)	a,		/* push count of arguments*/

#ifdef __sparc
#define malloc gtm_malloc
#define free gtm_free
#endif

#define CODE_ADDRESS(func)	(unsigned char *)func
#define	CONTEXT(func)		0	/* not used on this target */

/* PSECT in which the address of the module is defined: */
#define GTM_MODULE_DEF_PSECT	GTM_CODE


#define OS_PAGELET_SIZE		512
#define OS_PAGE_SIZE		OS_PAGELET_SIZE
#define OS_PAGE_SIZE_DECLARE
#define OS_VIRTUAL_BLOCK_SIZE	OS_PAGELET_SIZE
#define GTM_MM_FLAGS		MAP_SHARED
#define SSM_SIZE                OS_PAGE_SIZE

typedef volatile int4    latch_t;
typedef volatile uint4   ulatch_t;

#endif /* MDESP_included */
