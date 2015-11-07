/****************************************************************
 *                                                              *
 *    Copyright 2001, 2013 Fidelity Information Services, Inc   *
 *                                                              *
 *    This source code contains the intellectual property       *
 *    of its copyright holder(s), and is made available         *
 *    under a license.  If you do not know the terms of         *
 *    the license, please stop and do not read further.         *
 *                                                              *
 ****************************************************************/

#ifndef MDESP_included
#define MDESP_included

#include <sys/types.h>

#ifdef GTM64
typedef	long		int8;		/* 8-byte signed integer */
typedef unsigned long	uint8;	  	/* 8-byte unsigned integer */
typedef unsigned long 	gtm_uint8;	/*these two datatypes are defined because */
typedef		 long	gtm_int8;	/*int8 and uint8 are system defined in AIX_64*/
#define INT8_NATIVE
#endif

#ifdef __s390__
typedef		 short int2;		/* 2-byte signed integer */
typedef	unsigned short uint2;		/* 2-byte unsigned integer */
#define LINKAGE_PSECT_BOUNDARY	8
typedef uint2 mach_inst;
#endif

#define INT8_SUPPORTED
#define	INT8_FMT		"%llu"
#define	INT8_FMTX		"[0x%llx]"
#define UNICODE_SUPPORTED

#define UNIX 1
#undef VMS
#define BIGENDIAN 1
#define CNTR_WORD_32

#ifdef __sparc
#define CACHELINE_SIZE	256
#define USHBIN_SUPPORTED
#define LINKAGE_PSECT_BOUNDARY	8
#define OFF_T_LONG
#define INO_T_LONG
#define MUTEX_MSEM_WAKE
#define POSIX_MSEM
#undef sssize_t
#define sssize_t ssize_t
#undef SHMDT
#define SHMDT(X) shmdt((char *)(X))
typedef uint4 mach_inst;

/* Use rc_mval2subsc only for sun until every DTM client (that needs 16-bit precision as opposed to 18-bit for GT.M) is gone */
#define	mval2subsc	rc_mval2subsc
#ifndef VAR_COPY
#define VAR_COPY	va_copy
#endif
#endif /* __sparc */

#if defined(__ia64)
#define CACHELINE_SIZE	128
#define INO_T_LONG		/* define this for both Linux ia64 and HPUX ia64 as these are 64-bit builds */
#elif defined(__hppa)
#define CACHELINE_SIZE	64
#endif /* __ia64 */

#ifdef __hpux
#define MUTEX_MSEM_WAKE
#define POSIX_MSEM
#define USHBIN_SUPPORTED
#define OFF_T_LONG
/* Make sure linkage Psect is aligned on appropriate boundary. */
#ifdef __ia64
#define LINKAGE_PSECT_BOUNDARY	8
#else /* parisc */
#define LINKAGE_PSECT_BOUNDARY	4
#ifdef __GNUC__
typedef unsigned short	in_port_t; /* GCC needs this on PARISC */
#endif
#endif
typedef uint4 mach_inst;	/* machine instruction */
#endif /* __hpux */

#if defined(__linux__) || defined(__CYGWIN__)
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
#define INO_T_LONG				/* see gdsfhead.h, actually for dev_t == 8 on Linux390 2.2.15 */
#endif /* __s390__ */
#endif /* __linux__ */

#ifdef __linux__
#define SYS_ERRLIST_INCLUDE	"gtm_stdio.h"
#define MUTEX_MSEM_WAKE
#define POSIX_MSEM
#endif

#ifdef __CYGWIN__
#ifdef UNICODE_SUPPORTED_OBEYED
#undef UNICODE_SUPPORTED
#endif
#define KEY_T_LONG			/* 8 bytes */
#define SYS_ERRLIST_INCLUDE	<errno.h>
#endif

#ifdef __s390__
#define CACHELINE_SIZE	256
#define	GTM_CONTEXT(func)	(unsigned char *)func
#define SSM_SIZE		256*1024*1024	/* Segments on 256M boundary */
#define SHMAT_ADDR_INCS 	SSM_SIZE
#define USHBIN_SUPPORTED
#endif /* __s390__ */

#ifdef __ia64
#  ifdef __linux__
#    undef BIGENDIAN
#    define USHBIN_SUPPORTED
     /* Make sure linkage Psect is aligned on appropriate boundary. */
#    define LINKAGE_PSECT_BOUNDARY  8
typedef uint4 mach_inst;	/* machine instruction */
#  elif defined(__hpux)
void call_runtime();
void opp_dmode();
void dyncall();
#  endif
#endif /* __ia64 */

#ifdef __i386
/* Through Pentium Pro/II/III, should use CPUID to get real value perhaps */
#define CACHELINE_SIZE	32
#undef BIGENDIAN
typedef char  mach_inst;	/* machine instruction */
#endif /* __i386 */

#ifdef __x86_64__
#define CACHELINE_SIZE	64
#define USHBIN_SUPPORTED
#define INO_T_LONG
/*
#define MUTEX_MSEM_WAKE
#define POSIX_MSEM
*/
#define LINKAGE_PSECT_BOUNDARY  8
#undef BIGENDIAN
typedef char  mach_inst;	/* machine instruction */
#endif

#ifdef Linux390
#  define INTERLOCK_ADD(X,Y,Z)	(interlock_add(Z, (sm_int_ptr_t)(X)))
#else
#  define INTERLOCK_ADD(X,Y,Z)	(add_inter(Z, (sm_int_ptr_t)(X), (sm_global_latch_ptr_t)(Y)))
#endif

/* On NON_USHBIN_ONLY platforms, reserve enough space in routine header for the dummy
 * string "GTM_CODE". On USHBIN_ONLY platforms, reserve space of 16 bytes that holds
 * instructions for simple 'return -1' plus as many characters of "GTM_CODE" as can be fit
 * in the rest of the available bytes */
#if __ia64
#define RHEAD_JSB_SIZE 24 /* We need 16 bytes for putting the 'return -1' instruction + the GTM_CODE string */
#elif __x86_64__
#define RHEAD_JSB_SIZE 16 /* We need 8 bytes for putting the 'return -1' instruction + the GTM_CODE string */
#else
#define RHEAD_JSB_SIZE	NON_USHBIN_ONLY(8) USHBIN_ONLY(16)
#endif /* __ia64 */

typedef struct
{
	unsigned short	mvtype;
#ifdef	BIGENDIAN
	unsigned	sgn	: 1;
	unsigned	e	: 7;
#else
	unsigned	e	: 7;
	unsigned	sgn	: 1;
#endif
	unsigned char	fnpc_indx;	/* Index to fnpc_work area this mval is using */
	int4	m[2];
	mstr	str;
} mval;
/* Another version of mval struct with byte fields instead of bit fields */
typedef struct
{
	unsigned short	mvtype;
	unsigned char	sgne;
	unsigned char	fnpc_indx;	/* Index to fnpc_work area this mval is using */
	int4	m[2];
	mstr	str;
} mval_b;

#define VAR_START(a, b)	va_start(a, b)
#define VARLSTCNT(a)	a,		/* push count of arguments*/

#define malloc gtm_malloc
#define free gtm_free
/* gtm_shmget calls either the native shmget or libhugetlbfs's shmget which uses Huge Pages
 * to back the shared segment if possible. This is a Linux only library.
 */
#if defined(__linux__) && (defined(__x86_64__) || defined(__i386__))
#	define shmget	gtm_shmget
	extern int gtm_shmget(key_t , size_t , int);
#endif

#ifndef __ia64
#define CODE_ADDRESS(func)	(unsigned char *)func
#else
/* On IA64, there is a need to differentiate between generated code and regular
 * functions. When regular function addresses are obtained, this macro is
 * always used, and this will set the bottom two bits of the given address
 * (which is actually the function descriptor/PLABEL). Later in call_runtime/dyncall,
 * the bottom two bits will be checked and removed if required and a
 * dereference will happen to extract the actual target address.
 * This is done to mimic the behaviour of PLABEL/$$DYNCALL of HPPA on IA64.
 */
#define CODE_ADDRESS_C(func)	((unsigned char*) ((unsigned long)func | (unsigned long)0x3))
#define CODE_ADDRESS_ASM(func)	((unsigned char *) *(unsigned long *)func)
#define CODE_ADDRESS(func)	CODE_ADDRESS_ASM(func)

#endif /* __ia64 */

#ifndef GTM_CONTEXT
#define	GTM_CONTEXT(func)	0	/* not used on this target */
#endif

/* PSECT in which the address of the module is defined: */
#define GTM_MODULE_DEF_PSECT	GTM_CODE


#define OS_PAGELET_SIZE		512
#define OS_VIRTUAL_BLOCK_SIZE	OS_PAGELET_SIZE
#define GTM_MM_FLAGS		MAP_SHARED
#ifndef SSM_SIZE
#define SSM_SIZE		OS_PAGE_SIZE
#endif

typedef volatile	int4	latch_t;
typedef volatile	uint4	ulatch_t;

#define INSIDE_CH_SET		"ISO8859-1"
#define OUTSIDE_CH_SET		"ISO8859-1"
#define EBCDIC_SP		0x40
#define NATIVE_SP		0x20
#define DEFAULT_CODE_SET	ascii	/* enum ascii defined in io.h */

#endif /* MDESP_included */
