/****************************************************************
 *								*
 *	Copyright 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MDEF_included
#define MDEF_included

/* mstr needs to be defined before including "mdefsp.h".  */
typedef struct
{
        unsigned short  len;
        char            *addr;
} mstr;

#define MSTR_CONST(name,string)    mstr name = { sizeof(string)-1, string }

#include "v07_mdefsp.h"

#define MAXPOSINT4      ((int4)0x7fffffff)

#ifndef _AIX
#	ifndef __sparc
	typedef int             boolean_t;
#	endif
#endif
typedef char            bool;
typedef unsigned char   mreg;
typedef int4            mint;

typedef struct
{
        char    c[8];
} mident;

typedef long            ulimit_t;       /* NOT int4; the Unix ulimit function returns a value of type long */

#define MV_NM            1
#define MV_INT           2
#define MV_NUM_MASK      3
#define MV_STR           4
#define MV_NUM_APPROX    8
#define MV_SBS          16
#define MV_SYM          32
#define MV_SUBLIT       64
#define MV_RETARG      128
#define MV_XBIAS        62
#define MV_XZERO         0
#define MV_BIAS       1000
#define MV_BIAS_PWR      3
#define NR_REG          16
#define TRUE             1
#define FALSE            0
#ifndef NULL
#define NULL             0
#endif
#define NUL              0x00
#define SP               0x20
#define DEL              0x7f
#define MAX_STRLEN   32767
#define MAX_NUM_SIZE    64
#define MAX_FORM_NUM_SUBLEN 128         /*this is enough to hold the largest numeric subscript*/
#define PERIODIC_FLUSH_CHECK_INTERVAL (30 * 1000)

#define MV_FORCE_STR(X)         if (((X)->mvtype & MV_STR) == 0) n2s(X);
#define MV_FORCE_NUM(X)         if (((X)->mvtype & MV_NM) == 0) s2n(X);
#define MV_FORCE_INT(M)         ( (M)->mvtype & MV_INT ? (M)->m[1]/MV_BIAS : mval2i(M) )
#define MV_FORCE_ULONG_MVAL(M,I)   (((I) >= 1000000) ? i2usmval((M),(I)) : \
                                ( (M)->mvtype = MV_NM | MV_INT , (M)->m[1] = (I)*MV_BIAS ))
#define MV_FORCE_MVAL(M,I)      (((I) >= 1000000 || (I) <= -1000000) ? i2mval((M),(I)) : \
                                ( (M)->mvtype = MV_NM | MV_INT , (M)->m[1] = (I)*MV_BIAS ))
#define MV_FORCE_FLT(F,I)       ( (F)->e = 0 , (F)->m[1] = (I)*MV_BIAS )
#define MV_ASGN_FLT2MVAL(M,F)   ( (F).e == 0 ? ( (M).mvtype = MV_NM | MV_INT , (M).m[1] = (F).m[1] )\
                                             : ( (M).mvtype = MV_NM , (M).m[0] = (F).m[0] , (M).m[1] = (F).m[1]\
                                                , (M).sgn = (F).sgn  , (M).e = (F).e ))
#define MV_ASGN_MVAL2FLT(F,M)   ( (M).mvtype & MV_INT ? ( (F).e = 0 , (F).m[1] = (M).m[1] )\
                                                      : ( (F).m[0] = (M).m[0] , (F).m[1] = (M).m[1]\
                                                        , (F).sgn = (M).sgn , (F).e = (M).e ))
#define MV_FORCE_DEFINED(X)     if (!MV_DEFINED(X)) underr(X);
#define MV_FORCE_CANONICAL(X)   ((((X)->mvtype & MV_NM) == 0   ? s2n(X) : 0 )\
                                 ,((X)->mvtype & MV_NUM_APPROX ? (X)->mvtype &= MV_NUM_MASK : 0 ))
#define MV_IS_NUMERIC(X)        (((X)->mvtype & MV_NM) != 0)
#define MV_IS_INT(X)            (bool)isint(X)
#define MV_IS_STRING(X)         (((X)->mvtype & MV_STR) != 0)
#define MV_DEFINED(X)           (((X)->mvtype & (MV_STR | MV_NM)) != 0)
#define MV_IS_CANONICAL(X)      (((X)->mvtype & MV_NM) ? (((X)->mvtype & MV_NUM_APPROX) == 0) : (bool)nm_iscan(X))

#define DISK_BLOCK_SIZE 512

#define DIVIDE_ROUND_UP(VALUE, MODULUS)		(((VALUE) + ((MODULUS) - 1)) / (MODULUS))
#define DIVIDE_ROUND_DOWN(VALUE, MODULUS)	((VALUE) / (MODULUS))
#define ROUND_UP(VALUE, MODULUS)		(DIVIDE_ROUND_UP(VALUE, MODULUS) * (MODULUS))
#define ROUND_DOWN(VALUE, MODULUS)		(DIVIDE_ROUND_DOWN(VALUE, MODULUS) * (MODULUS))
#ifdef DEBUG
#define CHECKPOT(MODULUS)			(!(MODULUS) & ((MODULUS) - 1)) ? GTMASSERT, 0 :
#else
#define CHECKPOT(MODULUS)
#endif
/* these are the analogs of the preceeding, but are more efficient when the MODULUS is a Power Of Two */
#define ROUND_UP2(VALUE, MODULUS)		(CHECKPOT(MODULUS) ((VALUE) + ((MODULUS) - 1)) & ~((MODULUS) - 1))
#define ROUND_DOWN2(VALUE, MODULUS)		(CHECKPOT(MODULUS) (VALUE) & ~((MODULUS) - 1))
#define ISDIGIT(x)      ( (x) >='0' && (x) <= '9' )
#define ISALPHA(x)      ( (x) >='a' && (x) <= 'z' || (x) >= 'A' && (x) <= 'Z' )

#define CALLFROM        sizeof(__FILE__) - 1, __FILE__, __LINE__
#define GTMASSERT       (gtm_assert(CALLFROM))

#define RTS_ERROR_TEXT(STRING)  strlen(STRING), STRING

#ifdef DEBUG_TRACEABLE_VARS
# define SET_TRACEABLE_VAR(var,value) \
{ \
          error_def(ERR_TRACEVAR); \
          var = value; \
          gtm_putmsg(VARLSTCNT(9) ERR_TRACEVAR, 7, sizeof("var") - 1, "var", sizeof("value") - 1, "value", CALLFROM); \
}
#else
# define SET_TRACEABLE_VAR(var,value) var = value;
#endif

/* If this is unix, we have a faster sleep for short sleeps ( < 1 second) than doing a hiber start. */
#ifdef UNIX
# define SHORT_SLEEP(x) {assert(1000 > (x)); m_usleep((x) * 1000);}
#else
# define SHORT_SLEEP(x) hiber_start(x);
#endif

/*
    HPPA latches (used by load_and_clear) must be 16 byte aligned.  By
    allocating 16 bytes, the routines and macros used to access the
    latch can do the alignment.  Since nothing else should follow to
    avoid cache threshing, this doesn't really waste space.
    While specific to HPPA, it is defined for all platforms to allow
    use in region header where a constant size is required.
 */

typedef union
{
        volatile int4 hp_latch_space[4];
        volatile int4    latch_word;
} global_latch_t;

#ifdef INT8_SUPPORTED
   typedef	gtm_uint8			qw_num;
   typedef	gtm_uint8			seq_num;	/* Define 8-byte sequence number */
   typedef	gtm_uint8			qw_off_t;	/* quad-word offset */
#  define	DWASSIGNQW(A,B)		(A)=(uint4)(B)
#  define	QWASSIGN(A,B)		(A)=(B)
#  define	QWASSIGNDW(A,B)		QWASSIGN((A),(gtm_uint8)(B))
#  define 	QWADD(A,B,C)		(A)=(B)+(C)
#  define 	QWSUB(A,B,C)		(A)=(B)-(C)
#  define 	QWADDDW(A,B,C)		(A)=(B)+(gtm_uint8)(C)
#  define 	QWSUBDW(A,B,C)		(A)=(B)-(gtm_uint8)(C)
#  define 	QWINCRBY(A,B)		(A)+=(B)
#  define 	QWDECRBY(A,B)		(A)-=(B)
#  define 	QWINCRBYDW(A,B)		(A)+=(gtm_uint8)(B)
#  define 	QWDECRBYDW(A,B)		(A)-=(gtm_uint8)(B)
#  define	QWMULBYDW(A,B,C)	(A)=(B)*(C)
#  define	QWDIVIDEBYDW(A,B,Q,R)	{(R)=(A)%(B); (Q)=(A)/(B);}
#  define	QWMODDW(A,B)		((A)%(B))
#  define	QWLE(A,B)		((A)<=(B))
#  define	QWLT(A,B)		((A)<(B))
#  define	QWGE(A,B)		((A)>=(B))
#  define	QWGT(A,B)		((A)>(B))
#  define	QWEQ(A,B)		((A)==(B))
#  define	QWNE(A,B)		((A)!=(B))
#else

 typedef struct
 {
   	uint4	value[2];
 } qw_num, seq_num, qw_off_t; 			/* Define 8-bytes as a structure containing 2-byte array of uint4s */

#ifdef BIGENDIAN
#  define	msb_index		0
#  define	lsb_index		1
#  else
#  define	msb_index		1
#  define	lsb_index		0
#endif

#  define	DWASSIGNQW(A,B)		(A)=(B).value[lsb_index]
#  define	QWASSIGN(A,B)		(A)=(B)
#  define	QWASSIGNDW(A,B)		{(A).value[msb_index]=0; (A).value[lsb_index]=B;}
#  define 	QWADD(A,B,C)		{									\
						uint4 temp; 							\
						temp = (B).value[lsb_index];					\
						(A).value[lsb_index]=(B).value[lsb_index]+(C).value[lsb_index];	\
						(A).value[msb_index]=(B).value[msb_index]+(C).value[msb_index];	\
						if ((A).value[lsb_index] < temp) (A).value[msb_index]++;		\
					}
#  define 	QWSUB(A,B,C)		{									\
						uint4 temp; 							\
						temp = (B).value[lsb_index]; 					\
						(A).value[lsb_index]=(B).value[lsb_index]-(C).value[lsb_index]; 	\
						(A).value[msb_index]=(B).value[msb_index]-(C).value[msb_index]; 	\
						if ((A).value[lsb_index] > temp) (A).value[msb_index]--;		\
					}
#  define 	QWADDDW(A,B,C)		{									\
						uint4 temp; 							\
						temp = (B).value[lsb_index]; 					\
						(A).value[lsb_index]=(B).value[lsb_index]+C; 			\
						(A).value[msb_index]=(B).value[msb_index]; 				\
						if ((A).value[lsb_index] < temp) (A).value[msb_index]++;		\
					}
#  define 	QWSUBDW(A,B,C)		{									\
						uint4 temp; 							\
						temp = (B).value[lsb_index]; 					\
						(A).value[lsb_index]=(B).value[lsb_index]-(C); 			\
						(A).value[msb_index]=(B).value[msb_index]; 				\
						if ((A).value[lsb_index] > temp) (A).value[msb_index]--;		\
					}
#  define 	QWINCRBY(A,B)		QWADD(A,A,B)
#  define 	QWDECRBY(A,B)		QWSUB(A,A,B)
#  define 	QWINCRBYDW(A,B)		QWADDDW(A,A,B)
#  define 	QWDECRBYDW(A,B)		QWSUBDW(A,A,B)

				/* B should be less than 64K for the QWDIDIVEBYDW, QWMODDW macros to work correctly */

#  define	QWMULBYDW(A,B,C)	{	               							\
						uint4	bh, bl, ch, cl, temp, temp1, temp2;			\
						(A).value[msb_index] = (B).value[msb_index] * (C);			\
						bl = (B).value[lsb_index] & 0x0000ffff;				\
						bh = ((B).value[lsb_index] & 0xffff0000) >> 16;			\
						cl = (C) & 0x0000ffff;						\
						ch = ((C) & 0xffff0000) >> 16;					\
						(A).value[msb_index] += bh * ch;					\
						(A).value[lsb_index] = bl * cl;					\
						temp = temp1 = bh * cl;						\
						temp += bl * ch;						\
						if (temp1 > temp)						\
							(A).value[msb_index] += 0x00010000;			\
						temp2 = (A).value[lsb_index];					\
						(A).value[lsb_index] += (temp & 0x0000ffff) << 16;		\
						if ((A).value[lsb_index] < temp2)					\
							(A).value[msb_index] ++;					\
						(A).value[msb_index] += (temp & 0xffff0000) >> 16;		\
					}

#  define	QWDIVIDEBYDW(A,B,Q,R)	{									\
						uint4 msbr, lsbq, twoq, twor;					\
						(R) = (A).value[lsb_index] % (B);					\
						lsbq = (A).value[lsb_index] / (B);					\
						msbr = A.value[msb_index] % B;					\
						(Q).value[msb_index] = (A).value[msb_index] / (B);			\
						twoq = ((uint4)-1) / (B);						\
						twor = (((uint4)-1) % (B) + 1) % (B);				\
						if (0 == twor)							\
							twoq++;							\
						(Q).value[lsb_index] = lsbq;					\
						(Q).value[lsb_index] += twoq * msbr;				\
						if ((Q).value[lsb_index] < lsbq)					\
							(Q).value[msb_index]++;					\
						(R) = (R) + (twor * msbr) % (B);					\
						lsbq = (Q).value[lsb_index];					\
						(Q).value[lsb_index] += (twor * msbr) / (B);			\
						if ((R) > (B))							\
						{								\
							(R) -= (B);							\
							(Q).value[lsb_index]++;					\
						}								\
						if ((Q).value[lsb_index] < lsbq)					\
							(Q).value[msb_index]++;					\
					}

#  define	QWMODDW(A,B)		((((A).value[msb_index] % (B)) * (((uint4)-1) % (B) + 1) + (A).value[lsb_index]) % (B))

#  define	QWLE(A,B)		((A).value[msb_index] < (B).value[msb_index] || 				\
						((A).value[msb_index] == (B).value[msb_index] && (A).value[lsb_index] <= (B).value[lsb_index]))

#  define	QWLT(A,B)		((A).value[msb_index] < (B).value[msb_index] || 				\
						((A).value[msb_index] == (B).value[msb_index] && (A).value[lsb_index] < (B).value[lsb_index]))

#  define	QWGE(A,B)		((A).value[msb_index] > (B).value[msb_index] || 				\
						((A).value[msb_index] == (B).value[msb_index] && (A).value[lsb_index] >= (B).value[lsb_index]))

#  define	QWGT(A,B)		((A).value[msb_index] > (B).value[msb_index] || 				\
						((A).value[msb_index] == (B).value[msb_index] && (A).value[lsb_index] > (B).value[lsb_index]))

#  define	QWEQ(A,B)		((A).value[msb_index] == (B).value[msb_index] && (A).value[lsb_index] == (B).value[lsb_index])
#  define	QWNE(A,B)		((A).value[msb_index] != (B).value[msb_index] || (A).value[lsb_index] != (B).value[lsb_index])
#endif

/* Define some basic types for shared memory (sm) access depending on whether the platform we are    */
/* using is capable of supporting 32 or 64 bit pointers or not.                                      */

#ifdef DB64
#  if defined(__osf__) && defined(__alpha)
#    pragma pointer_size(save)
#    pragma pointer_size(long)
#  else
#    error UNSUPPORTED PLATFORM
#  endif
  typedef unsigned char *sm_uc_ptr_t;   /* Define 64 bit pointer to unsigned char */
  typedef char *sm_c_ptr_t;             /* Define 64 bit pointer to char */
  typedef unsigned char *uchar_ptr_t;   /* Define 64 bit pointer to unsigned char (no sm connotation) */
  typedef char *char_ptr_t;             /* Define 64 bit pointer to char (no sm connotation) */
  typedef uint4 *uint_ptr_t;            /* Define 64 bit pointer to uint (no sm connotation */
  typedef int4 *sm_int_ptr_t;           /* Define 64 bit pointer to int */
  typedef uint4 *sm_uint_ptr_t;         /* Define 64 bit pointer to uint */
  typedef short *sm_short_ptr_t;        /* Define 64 bit pointer to short */
  typedef gtm_int8 sm_off_t;                /* Define 64 bit offset type */
  typedef gtm_int8 sm_long_t;               /* Define 64 bit integer type */
  typedef gtm_uint8 sm_ulong_t;             /* Define 64 bit unsigned integer type */
  typedef global_latch_t *sm_global_latch_ptr_t; /* Define 64 bit pointer to hp_latch */
#  ifdef __osf__
#    pragma pointer_size(restore)
#  endif
  /* The macro FILL8DCL (explained below) is simple on a 64 bit system since all 64 bits
     will be declared and used. */
#  define FILL8DCL(type,name,fillnum) type name
#else
  typedef unsigned char *sm_uc_ptr_t;   /* Define 32 bit pointer to unsigned char */
  typedef char *sm_c_ptr_t;             /* Define 32 bit pointer to char */
  typedef unsigned char *uchar_ptr_t;   /* Define 32 bit pointer to unsigned char (no sm connotation) */
  typedef char *char_ptr_t;             /* Define 32 bit pointer to char (no sm connotation) */
  typedef uint4 *uint_ptr_t;            /* Define 32 bit pointer to uint (no sm connotation */
  typedef int4 *sm_int_ptr_t;           /* Define 32 bit pointer to int */
  typedef uint4 *sm_uint_ptr_t;         /* Define 32 bit pointer to uint */
  typedef short *sm_short_ptr_t;        /* Define 32 bit pointer to short */
  typedef int4 sm_off_t;                /* Define 32 bit offset type */
  typedef int4 sm_long_t;               /* Define 32 bit integer type */
  typedef uint4 sm_ulong_t;             /* Define 32 bit unsigned integer type */
  typedef global_latch_t *sm_global_latch_ptr_t; /* Define 32 bit pointer to hp_latch */
  /* The macro FILL8DCL is used (on a 32 bit system) to provide a filler area of 32 bits and
     the actual 32 bit declared area. Whether the high order word or the low order word of
     the 64 bit area should be filler depends on the endian mode of the machine. This macro
     will be defined to take care of that for us. */
#  ifdef BIGENDIAN
#    define FILL8DCL(type,name,fillnum) type fill##fillnum,name
#  else
#    define FILL8DCL(type,name,fillnum) type name,fill##fillnum
#  endif
#endif

/* Need to define a consistently sized off_t type. Some platforms it is 4 bytes, others it is
   4 or 8 bytes depending on flags. The following OFF_T macro is setup to allow the size of the
   variable declared by it to always take up 8 bytes for alignment purposes. If the OFF_T_LONG
   value is set, we will expect the size of 'off_t' to be 8 bytes. An assert will be placed in
   gtm.c to verify this.  */

#ifdef OFF_T_LONG
#  define OFF_T(name,fillnum) off_t     name
#else
#  define OFF_T(name,fillnum) FILL8DCL(off_t,name,fillnum)
#endif

/* Type for offsets in journal files.  VMS uses uint4 to get a full 32 bit
   offset for large journal files (OK since doesn't use lseek/etc. for IO.) */

#ifdef OFF_T_LONG
#  define JNL_OFF_T(name,fillnum) off_t     name
#else
#  ifdef VMS
#    define JNL_OFF_T(name,fillnum) FILL8DCL(uint4,name,fillnum)
#  else
#    define JNL_OFF_T(name,fillnum) FILL8DCL(off_t,name,fillnum)
#  endif
#endif

/* Need to define a consistently sized counter that is controlled by interlocks. The counter
   will occupy 4 bytes in the file header but on some platforms (currently VAX and AXP VMS),
   these counters need to be shorts whereas other platforms would realize a performance
   improvement if they were 32 bits long. So we create another macro in the spirit of the
   FILL8DCL macro above which will always give us a 32 byte entity but will pad a 2 byte
   addressable entity if necessary. If not specified, the default is for 'short' counters. */
#  ifdef CNTR_WORD_32
#    define FILL4DCL(type,name,fillnum) type name
#    define CNTR4DCL(name,fillnum) int4 name
#  else
#    ifdef BIGENDIAN
#      define FILL4DCL(type,name,fillnum) type fill##fillnum,name
#    else
#      define FILL4DCL(type,name,fillnum) type name,fill##fillnum
#    endif
#    define CNTR4DCL(name,fillnum) FILL4DCL(short,name,fillnum)
#  endif

/* For machines with a cache line dependency for locks and such,
   define a macro that can be used to generate padding such that
   fields are in separate cache lines. Note that this macro should
   *NOT* be used in the fileheader as its length expansion is platform
   specific. It should only be used in internal shared memory
   structures that are NOT otherwise placement sensitive.
   A ; is included in the definition instead of when used since an extra ;
   in a structure is not accepted by some compilers.
*/

#ifdef CACHELINE_SIZE
# define CACHELINE_PAD(fieldSize, fillnum) char fill##fillnum[CACHELINE_SIZE - (fieldSize)];
#else
# define CACHELINE_PAD(fieldSize, fillnum)
#endif

#define MEMCP(dst,src,start,count,limit){ \
        if(start+count > limit) \
                rts_error(VARLSTCNT(1) ERR_CPBEYALLOC); \
        else    \
                memcpy(dst+start,src,count); \
}

#endif /* MDEF_included */
