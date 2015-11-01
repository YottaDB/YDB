/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MDESP_included
#define MDESP_included

#include <sys/types.h>

typedef          long	int4;		/* 4-byte signed integer */
typedef unsigned long	uint4;		/* 4-byte unsigned integer */

#ifdef __s390__
typedef		 short int2;		/* 2-byte signed integer */
typedef	unsigned short uint2;		/* 2-byte unsigned integer */
#define LINKAGE_PSECT_BOUNDARY	8
typedef uint4 mach_inst;
#endif

#define INT8_SUPPORTED
#define	INT8_FMT		"%llu"
#define	INT8_FMTX		"[0x%llx]"
#define UNICODE_SUPPORTED

/* Starting off life as debugging parms and now we need them for the
   short term so define them here */
#define DEBUG_LEAVE_SM
#define DEBUG_NOMSYNC

#define readonly
#define GBLDEF
#define GBLREF extern
#define LITDEF const
#define LITREF extern const
#define error_def(x) LITREF int x
#ifdef DEBUG
error_def(ERR_ASSERT);
#define assert(x) ((x) ? 1 : rts_error(VARLSTCNT(5) ERR_ASSERT, 3, LEN_AND_LIT(__FILE__), __LINE__))
#else
#define assert(x)
#endif

#define UNIX 1
#undef VMS
#define BIGENDIAN 1
#define CNTR_WORD_32

/* #define memcmp(DST,SRC,LEN) memucmp(DST,SRC,LEN) */

#ifdef __sparc
#define CACHELINE_SIZE        256
#define MSYNC_ADDR_INCS        OS_PAGE_SIZE

#define OFF_T_LONG
#define INO_T_LONG
#define MUTEX_MSEM_WAKE
#define POSIX_MSEM
#undef sssize_t
#define sssize_t ssize_t
#undef SHMDT
#define SHMDT(X) shmdt((char *)(X))

/* Use rc_mval2subsc only for sun until every DTM client (that needs 16-bit precision as opposed to 18-bit for GT.M) is gone */
#define	mval2subsc	rc_mval2subsc
#ifndef VAR_COPY
#define VAR_COPY	va_copy
#endif
#endif /* __sparc */

#ifdef __hpux
#define CACHELINE_SIZE        64
#define MSYNC_ADDR_INCS        OS_PAGE_SIZE
#define MUTEX_MSEM_WAKE
#define POSIX_MSEM
#define USHBIN_SUPPORTED
#define OFF_T_LONG
/* Make sure linkage Psect is aligned on appropriate boundary. */
#define LINKAGE_PSECT_BOUNDARY	4
typedef uint4 mach_inst;	/* machine instruction */
#endif /* __hpux */

#ifdef __linux__
#define OFF_T_LONG
#ifdef NeedInAddrPort
typedef unsigned short	in_port_t;
#endif
#ifndef VAR_COPY
#define VAR_COPY(dst,src) __va_copy(dst, src)
#endif
#ifdef __s390__
#ifndef Linux390
#define Linux390
#endif
#define INO_T_LONG			    /* see gdsfhead.h, actually for dev_t == 8 on Linux390 2.2.15 */
#endif
#endif /* __linux__ */

#ifdef __s390__
#define CACHELINE_SIZE        256
/* typedef struct {
	unsigned char	*code_address;
	unsigned char	*toc;
	int		unknown;
} func_desc; */

#define	GTM_CONTEXT(func)	(unsigned char *)func

#define SSM_SIZE		256*1024*1024	/* Segments on 256M boundary */
#define SHMAT_ADDR_INCS 	SSM_SIZE
#define MSYNC_ADDR_INCS 	OS_PAGE_SIZE

#endif /* __s390__ */

#ifdef __i386
/* Through Pentium Pro/II/III, should use CPUID to get real value perhaps */
#define CACHELINE_SIZE        32
#define MSYNC_ADDR_INCS        OS_PAGE_SIZE
#undef BIGENDIAN
#endif /* __i386 */

#define INTERLOCK_ADD(X,Y,Z)    (add_inter(Z, (sm_int_ptr_t)(X), (sm_global_latch_ptr_t)(Y)))


/* On NON_USHBIN_ONLY platforms, reserve enough space in routine header for the dummy
 * string "GTM_CODE". On USHBIN_ONLY platforms, reserve space of 16 bytes that holds
 * instructions for simple 'return -1' plus as many characters of "GTM_CODE" as can be fit
 * in the rest of the available bytes */
#define RHEAD_JSB_SIZE	NON_USHBIN_ONLY(8) USHBIN_ONLY(16)

typedef struct
{
#ifdef	BIGENDIAN
	unsigned int	sgn : 1 ;
	unsigned int	e   : 7 ;
#else
	unsigned int	e   : 7 ;
	unsigned int	sgn : 1 ;
#endif
	int4		m[2]	;
} mflt ;

typedef struct
{
	unsigned int	mvtype   : 16;
#ifdef	BIGENDIAN
	unsigned int	sgn      : 1;
	unsigned int	e        : 7;
#else
	unsigned int	e        : 7;
	unsigned int	sgn      : 1;
#endif
	unsigned int	fnpc_indx: 8;	/* Index to fnpc_work area this mval is using */
	int4	m[2];
	mstr	str;
} mval ;


#ifdef BIGENDIAN
#define DEFINE_MVAL_LITERAL(TYPE, EXPONENT, SIGN, LENGTH, ADDRESS, MANT_LOW, MANT_HIGH) \
	{TYPE, SIGN, EXPONENT, 0xff, MANT_LOW, MANT_HIGH, 0, LENGTH, ADDRESS}
#else
#define DEFINE_MVAL_LITERAL(TYPE, EXPONENT, SIGN, LENGTH, ADDRESS, MANT_LOW, MANT_HIGH) \
	{TYPE, EXPONENT, SIGN, 0xff, MANT_LOW, MANT_HIGH, 0, LENGTH, ADDRESS}
#endif

#define VAR_START(a, b)	va_start(a, b)
#define VARLSTCNT(a)	a,		/* push count of arguments*/

#ifndef GTSQL    /* these cannot be used within SQL */
#define malloc gtm_malloc
#define free gtm_free
#endif

#define CODE_ADDRESS(func)	(unsigned char *)func
#ifndef GTM_CONTEXT
#define	GTM_CONTEXT(func)	0	/* not used on this target */
#endif

/* PSECT in which the address of the module is defined: */
#define GTM_MODULE_DEF_PSECT	GTM_CODE


#define OS_PAGELET_SIZE		512
#define OS_VIRTUAL_BLOCK_SIZE	OS_PAGELET_SIZE
#define GTM_MM_FLAGS		MAP_SHARED
#ifndef SSM_SIZE
#define SSM_SIZE                OS_PAGE_SIZE
#endif

typedef volatile	int4    latch_t;
typedef volatile	uint4   ulatch_t;

#define INSIDE_CH_SET		"ISO8859-1"
#define OUTSIDE_CH_SET		"ISO8859-1"
#define EBCDIC_SP		0x40
#define NATIVE_SP		0x20
#define DEFAULT_CODE_SET	ascii	/* enum ascii defined in io.h */

#endif /* MDESP_included */
