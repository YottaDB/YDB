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

#ifndef MDEF_included
#define MDEF_included

/* mstr needs to be defined before including "mdefsp.h".  */
#ifndef __vax
typedef struct
{
	unsigned int	len;
	char		*addr;
} mstr;
typedef unsigned int mstr_len_t;
#define GET_MSTR_LEN(X,Y)	GET_ULONG(X,Y)
#define PUT_MSTR_LEN(X,Y)	PUT_ULONG(X,Y)
#else
/* Vax version of mstr has short length still */
typedef struct
{
	unsigned short	len;
	char		*addr;
} mstr;
typedef unsigned short mstr_len_t;
#define GET_MSTR_LEN(X,Y)	GET_USHORT(X,Y)
#define PUT_MSTR_LEN(X,Y)	PUT_USHORT(X,Y)
#endif
#define MSTR_CONST(name,string)		mstr name = { LEN_AND_LIT(string) }
#define MSTR_DEF(name,length,string)	mstr name = { length, string }

#define MSTR_CMP(x, y, result) 	(((result) = memcmp((x)->addr, (y)->addr, MIN((x)->len, (y)->len))) ?\
								(result) : ((result) = (x)->len - (y)->len))
#define MSTR_EQ(x,y)		(((x)->len == (y)->len) && !memcmp((x)->addr, (y)->addr, (x)->len))

#include <sys/types.h>


#define sssize_t      size_t
#define SHMDT(X)	shmdt((void *)(X))

/* constant needed for FIFO - OS390 redefines in mdefsp.h */
#define FIFO_PERMISSION		010666 /* fifo with RW permissions for owner, group, other */

#include <inttypes.h>
#include "mdefsa.h"
#include "mdefsp.h"

#if !defined(__alpha) && !defined(__sparc) && !defined(__hpux) && !defined(mips)
#define UNALIGNED_ACCESS_SUPPORTED
#endif

#define BITS_PER_UCHAR  8 /* note, C does not require this to be 8, see <limits.h> for definitions of CHAR_BIT and UCHAR_MAX */

#define MAXPOSINT4		((int4)0x7fffffff)
#define	MAX_DIGITS_IN_INT	10	/* maximum number of decimal digits in an integer */
#define MAX_HOST_NAME_LEN	256

#ifndef _AIX
#	ifndef __sparc
	typedef int		boolean_t;
#	endif
#endif
typedef char		bool;
typedef unsigned char	mreg;
typedef int4		mint;

#define PRE_V5_MAX_MIDENT_LEN	8	/* Maximum length of an mident/mname before GT.M V5.0 */
typedef struct
{ /* The old mident structure used before V50FT01 */
	char	c[PRE_V5_MAX_MIDENT_LEN];
} pre_v5_mident;

#define MAX_MIDENT_LEN		31	/* Maximum length of an mident/mname */
typedef mstr		mident;
typedef struct
{ /* Although we use 31 chars, the extra byte is to keep things aligned */
	char	c[MAX_MIDENT_LEN + 1];
} mident_fixed;
#define mid_len(name)		strlen(&(name)->c[0])	/* callers of mid_len should include gtm_string.h as well */

#define MIDENT_CMP(x,y,result)	MSTR_CMP(x,y,result)
#define MIDENT_EQ(x,y)		MSTR_EQ(x,y)

#ifdef INT8_NATIVE
#	define NATIVE_WSIZE	8
#else
#	define NATIVE_WSIZE	4
#endif

/* Maximum length of entry reference of the form "label+offset^routine" */
#define MAX_ENTRYREF_LEN	(2 * MAX_MIDENT_LEN + MAX_DIGITS_IN_INT + STR_LIT_LEN("+^"))

/* M name entry used in various structures - variable table (rtnhdr.h), hash table (hashtab_def.h) and
 * global variable (gv_namehead in gdsfhead.h) */
typedef struct
{
	mident 		var_name;	/* var_name.addr points to the actual variable name */
	uint4		hash_code;	/* hash (scrambled) value of the variable name text */
} mname_entry;

/* The M stack frame on all platforms that follow pv-based linkage model (alpha model)
 * contains a pointer to the base of routine's literal section. All such platforms
 * must define HAS_LITERAL_SECT so that the routines that create a new stack frame
 * initialize literal_ptr field apppropriately. */
#if defined(__alpha) || defined(_AIX) || defined(__hpux) || defined(__MVS__) || defined(__s390__)
#define HAS_LITERAL_SECT
#endif

typedef long		ulimit_t;	/* NOT int4; the Unix ulimit function returns a value of type long */

#define MV_NM		 1
#define MV_INT		 2
#define MV_NUM_MASK	 3
#define MV_STR		 4
#define MV_NUM_APPROX	 8
#define MV_SBS		16
#define MV_SYM		32
#define MV_SUBLIT	64
#define MV_RETARG      128
#define MV_XBIAS	62
#define MV_XZERO	 0
#define MV_BIAS	      1000
#define MV_BIAS_PWR	 3
#define NR_REG		16
#ifndef TRUE
#define TRUE		 1
#endif
#ifndef FALSE
#define FALSE		 0
#endif
#ifndef NULL
#define NULL		((void *) 0)
#endif
#define NUL		 0x00
#define SP		 0x20
#define DEL		 0x7f

#define MAX_STRLEN_32K			32767
/* MAX_STRLEN for local variable is changed from 32767 to 1048576 (1 MB) */
#define MAX_STRLEN	 		(1 * 1024 * 1024)  /*maximum GT.M string size (1 MB)*/
#define MAX_DBSTRLEN			(32 * 1024 - 1) /* Maximum database string size */
/* Initial buffer size allocated for a GT.M string which can geometrically be increased upto the size enough to fit in MAX_STRLEN */
#define MAX_STRBUFF_INIT	 	(32 * 1024)

#define MAX_NUM_SIZE			64
#define MAX_FORM_NUM_SUBLEN		128	/* this is enough to hold the largest numeric subscript */
#define PERIODIC_FLUSH_CHECK_INTERVAL (30 * 1000)
#define MAX_ARGS	256 /* in formallist */
/* the macro ZWR_EXP_RATIO returns the maximum size of the extracted record given the internal subscript representation length
 * worst case is every other character is non-graphic. e.g. $C(128)_"A"_$C(128)
 * to cover cases of odd numbers of characters, add some buffer
 */
#define ZWR_EXP_RATIO(X)	((X) * 6 + 8)

unsigned char *n2s(mval *mv_ptr);
char *s2n(mval *u);

#define MV_FORCE_STR(X)		((0 == ((X)->mvtype & MV_STR)) ? n2s(X) : NULL)
#define MV_FORCE_NUM(X)		((0 == ((X)->mvtype & MV_NM )) ? s2n(X) : NULL)
#define MV_FORCE_BOOL(X)	(MV_FORCE_NUM(X), (X)->m[1] ? TRUE : FALSE)
#define MV_FORCE_INT(M)		( (M)->mvtype & MV_INT ? (M)->m[1]/MV_BIAS : mval2i(M) )
#define MV_FORCE_ULONG_MVAL(M,I)   (((I) >= 1000000) ? i2usmval((M),(I)) : \
				(void)( (M)->mvtype = MV_NM | MV_INT , (M)->m[1] = (I)*MV_BIAS ))
#define MV_FORCE_MVAL(M,I)	(((I) >= 1000000 || (I) <= -1000000) ? i2mval((M),(I)) : \
				(void)( (M)->mvtype = MV_NM | MV_INT , (M)->m[1] = (I)*MV_BIAS ))
#define MV_FORCE_FLT(F,I)	( (F)->e = 0 , (F)->m[1] = (I)*MV_BIAS )
#define MV_ASGN_FLT2MVAL(M,F)	( (F).e == 0 ? ( (M).mvtype = MV_NM | MV_INT , (M).m[1] = (F).m[1] )\
					     : ( (M).mvtype = MV_NM , (M).m[0] = (F).m[0] , (M).m[1] = (F).m[1]\
						, (M).sgn = (F).sgn  , (M).e = (F).e ))
#define MV_ASGN_MVAL2FLT(F,M)	( (M).mvtype & MV_INT ? ( (F).e = 0 , (F).m[1] = (M).m[1] )\
						      : ( (F).m[0] = (M).m[0] , (F).m[1] = (M).m[1]\
							, (F).sgn = (M).sgn , (F).e = (M).e ))
#define MV_FORCE_DEFINED(X)	if (!MV_DEFINED(X)) underr(X);
#define MV_FORCE_CANONICAL(X)	((((X)->mvtype & MV_NM) == 0   ? s2n(X) : 0 )\
				 ,((X)->mvtype & MV_NUM_APPROX ? (X)->mvtype &= MV_NUM_MASK : 0 ))
#define MV_IS_NUMERIC(X)	(((X)->mvtype & MV_NM) != 0)
#define MV_IS_INT(X)		(bool)isint(X)
#define MV_IS_STRING(X)		(((X)->mvtype & MV_STR) != 0)
#define MV_DEFINED(X)		(((X)->mvtype & (MV_STR | MV_NM)) != 0)
#define MV_IS_CANONICAL(X)	(((X)->mvtype & MV_NM) ? (((X)->mvtype & MV_NUM_APPROX) == 0) : (bool)nm_iscan(X))

#define DISK_BLOCK_SIZE		512
#define LOG2_DISK_BLOCK_SIZE	9

#define DIVIDE_ROUND_UP(VALUE, MODULUS)		(((VALUE) + ((MODULUS) - 1)) / (MODULUS))
#define DIVIDE_ROUND_DOWN(VALUE, MODULUS)	((VALUE) / (MODULUS))
#define ROUND_UP(VALUE, MODULUS)		(DIVIDE_ROUND_UP(VALUE, MODULUS) * (MODULUS))
#define ROUND_DOWN(VALUE, MODULUS)		(DIVIDE_ROUND_DOWN(VALUE, MODULUS) * (MODULUS))

#ifdef DEBUG
#  define CHECKPOT(MODULUS)			((MODULUS) & ((MODULUS) - 1)) ? GTMASSERT, 0 :
#  define BREAK_IN_PRO__CONTINUE_IN_DBG		continue
#  define DEBUG_ONLY(statement)			statement
#  define PRO_ONLY(statement)
#else
#  define CHECKPOT(MODULUS)
#  define BREAK_IN_PRO__CONTINUE_IN_DBG		break
#  define DEBUG_ONLY(statement)
#  define PRO_ONLY(statement)			statement
#endif

/* these are the analogs of the preceeding, but are more efficient when the MODULUS is a Power Of Two */
#define ROUND_UP2(VALUE, MODULUS)		(CHECKPOT(MODULUS) ((VALUE) + ((MODULUS) - 1)) & ~((MODULUS) - 1))
#define ROUND_DOWN2(VALUE, MODULUS)		(CHECKPOT(MODULUS) (VALUE) & ~((MODULUS) - 1))

/* Length needed to pad out to a given power of 2 boundary */
#define PADLEN(value, bndry) (ROUND_UP2((sm_long_t)(value), bndry) - (sm_long_t)(value))

/* LOG2_OF_INTEGER returns the ceiling of log (base 2) of number */
#define LOG2_OF_INTEGER(number, log2_of_number)			\
{								\
	int    temp = (number) - 1;				\
	for (log2_of_number = 0; 0 < temp; log2_of_number++)	\
	   temp = (temp) >> 1; 					\
}

#define ISDIGIT(x)	( (x) >='0' && (x) <= '9' )
#define ISALPHA(x)	( (x) >='a' && (x) <= 'z' || (x) >= 'A' && (x) <= 'Z' )

#define CALLFROM	LEN_AND_LIT(__FILE__), __LINE__
void gtm_assert ( int file_name_len, char file_name[], int line_no);
#define GTMASSERT	(gtm_assert(CALLFROM))

#ifdef UNIX
int rts_error();
#endif
void stx_error();
void dec_err();
void ins_errtriple(int4 in_error);

int4 timeout2msec(int4 timeout);

/* the RTS_ERROR_TEXT macro will stay till all existing references to it have been renamed to RTS_ERROR_{LITERAL,STRING} */
#define	RTS_ERROR_TEXT(STRING)		LENGTH_AND_STRING(STRING)

/* for those who prefer not remembering the order of the length and the literal/string in the rts_error command line */
#define	RTS_ERROR_LITERAL(LITERAL)	LENGTH_AND_LITERAL(LITERAL)
#define	RTS_ERROR_STRING(STRING)	LENGTH_AND_STRING(STRING)

/* the LITERAL version of the macro should be used over STRING whenever possible for efficiency reasons */
#define	STR_LIT_LEN(LITERAL)		(sizeof(LITERAL) - 1)
#define	LITERAL_AND_LENGTH(LITERAL)	(LITERAL), (sizeof(LITERAL) - 1)
#define	LENGTH_AND_LITERAL(LITERAL)	(sizeof(LITERAL) - 1), (LITERAL)
#define	STRING_AND_LENGTH(STRING)	(STRING), (strlen(STRING))
#define	LENGTH_AND_STRING(STRING)	(strlen(STRING)), (STRING)

#define	LEN_AND_LIT(LITERAL)		LENGTH_AND_LITERAL(LITERAL)
#define	LIT_AND_LEN(LITERAL)		LITERAL_AND_LENGTH(LITERAL)
#define	STR_AND_LEN(STRING)		STRING_AND_LENGTH(STRING)
#define	LEN_AND_STR(STRING)		LENGTH_AND_STRING(STRING)

#define	MEMCMP_LIT(SOURCE, LITERAL)	memcmp(SOURCE, LITERAL, sizeof(LITERAL) - 1)
#define MEMCPY_LIT(TARGET, LITERAL)	memcpy(TARGET, LITERAL, sizeof(LITERAL) - 1)
#define	STRNCMP_LIT(SOURCE, LITERAL)	strncmp(SOURCE, LITERAL, sizeof(LITERAL) - 1)
#define	STRNCMP_STR(SOURCE, STRING)	strncmp(SOURCE, STRING, strlen(STRING))

/* *********************************************************************************************************** */
/*		   Frequently used len + str combinations in macro form.				       */
/* *********************************************************************************************************** */
#define	DB_STR_LEN(reg)			(reg)->dyn.addr->fname, (reg)->dyn.addr->fname_len
#define	DB_LEN_STR(reg)			(reg)->dyn.addr->fname_len, (reg)->dyn.addr->fname
#define	REG_STR_LEN(reg)		(reg)->rname, (reg)->rname_len
#define	REG_LEN_STR(reg)		(reg)->rname_len, (reg)->rname
#define	JNL_STR_LEN(csd)		(csd)->jnl_file_name, (csd)->jnl_file_len
#define	JNL_LEN_STR(csd)		(csd)->jnl_file_len, (csd)->jnl_file_name

#define	FAB_LEN_STR(fab)		(fab)->fab$b_fns, (fab)->fab$l_fna
/* *********************************************************************************************************** */


#ifdef DEBUG
/* Original debug code has been removed since it was superfluous and did not work on all platforms. SE 03/01 */
# define SET_TRACEABLE_VAR(var,value) var = value;
#else
# define SET_TRACEABLE_VAR(var,value) var = value;
#endif

/* If this is unix, we have a faster sleep for short sleeps ( < 1 second) than doing a hiber start.
 * Take this chance to define UNIX_ONLY and VMS_ONLY macros.
 */
int m_usleep(int useconds);
#ifdef UNIX
#	define SHORT_SLEEP(x) {assert(1000 > (x)); m_usleep((x) * 1000);}
#else
#	define SHORT_SLEEP(x) hiber_start(x);
#endif

#ifdef UNIX
#	define UNIX_ONLY(X)		X
#else
#	define UNIX_ONLY(X)
#endif

#ifdef VMS
#	define VMS_ONLY(X)		X
#else
#	define VMS_ONLY(X)
#endif

#if (defined(UNIX) || defined(VMS))
#	define UNSUPPORTED_PLATFORM_CHECK
#else
#	define UNSUPPORTED_PLATFORM_CHECK	#error UNSUPPORTED PLATFORM
#endif

/* Note the macros below refer to the UNIX Shared Binary Support. Because the
   support is *specifically* for the Unix platform, "NON_USHBIN_ONLY()" will
   also be true for VMS even though that platform does have shared binary support
   (but it does not have Unix Shared Binary support). Use "NON_USHBIN_UNIX_ONLY()"
   for UNIX platforms that do not support Shared Binaries. */
#ifdef USHBIN_SUPPORTED
#	define USHBIN_ONLY(X)		X
#	define NON_USHBIN_ONLY(X)
#	define NON_USHBIN_UNIX_ONLY(X)
#else
#	define USHBIN_ONLY(X)
#	define NON_USHBIN_ONLY(X)	X
#	ifdef UNIX
#		define NON_USHBIN_UNIX_ONLY(X)	X
#	else
#		define NON_USHBIN_UNIX_ONLY(X)
#	endif
#endif

/* Note: LONG_SLEEP *MUST*NOT* be the sleep() function because use of the sleep() function in
   GT.M causes problems with GT.M's timers on some platforms. Specifically, the sleep() function
   causes the SIGARLM handler to be silently deleted on Solaris systems (through Solaris 9 at least).
   This leads to lost timer pops and has the potential for system hangs.
 */
#define LONG_SLEEP(x)		hiber_start((x) * 1000)

#define OS_PAGE_SIZE		gtm_os_page_size
#define OS_PAGE_SIZE_DECLARE	GBLREF int4 gtm_os_page_size;
#define IO_BLOCK_SIZE		OS_PAGE_SIZE

#ifndef GTM_INT64T_DEFINED
#define GTM_INT64T_DEFINED
   typedef	uint64_t		gtm_uint64_t;
   typedef	int64_t			gtm_int64_t;
#endif

/* HPPA latches (used by load_and_clear) must be 16 byte aligned.
 * By allocating 16 bytes, the routines and macros used to access the latch can do the alignment.
 * Since nothing else should follow to avoid cache threshing, this doesn't really waste space.
 * Note that the additional space for this latch is only allocated on HPPA. All other platforms
 * have a "sensible" compare-and-swap type lock using the first two words in the latch.
 */
typedef struct
{
	union
	{
		gtm_uint64_t	pid_imgcnt;		/* Combined atomic (unique) process id used on VMS */
		struct
		{
			volatile int4	latch_pid;	/* (Usually) Process id of latch holder or LOCK_AVAILABLE. On VMS
							   this word may have other values.  */
			volatile int4	latch_word;	/* Extra word associated with lock (sometimes bci lock or image cnt
							   for VMS) */
		} parts;
	} u;
#ifdef __hpux
	volatile int4	hp_latch_space[4];		/* Used for HP load_and_clear locking instructions per
							   HP whitepaper on spinlocks */
#endif
} global_latch_t;
#define latch_image_count latch_word

typedef	union gtm_time8_struct
{
	time_t	ctime;		/* For current GTM code sem_ctime field corresponds to creation time */
	int4	filler[2];	/* Filler to ensure size is 2 words on all platforms */
} gtm_time8;

typedef struct
{
	int4	fl;		/* forward link - relative offset from beginning of this element to next element in queue */
	int4	bl;		/* backward link - relative offset from beginning of this element to previous element in queue */
} que_ent;			/* this structure is intended to be identical to the first two items in a cache_que_head */

typedef struct
{
	int4	fl;		/* forward link - relative offset from beginning of this element to next element in queue */
	int4	bl;		/* backward link - relative offset from beginning of this element to previous element in queue */
	global_latch_t	latch;	/* required for platforms without atomic operations to modify both fl and bl concurrently;
				 * unused on platforms with such instructions. */
} que_head, cache_que_head, mmblk_que_head;

#define	IS_PTR_ALIGNED(ptr, ptr_base, elemSize)					\
	(0 == ((((sm_uc_ptr_t)(ptr)) - ((sm_uc_ptr_t)(ptr_base))) % elemSize))
#define	IS_PTR_IN_RANGE(ptr, ptr_lo, ptr_hi)								\
	(((sm_uc_ptr_t)(ptr) >= (sm_uc_ptr_t)(ptr_lo)) && ((sm_uc_ptr_t)(ptr) < (sm_uc_ptr_t)(ptr_hi)))

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef que_ent *	que_ent_ptr_t;
typedef que_head *	que_head_ptr_t;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

 /* Define 8-bytes as a structure containing 2-byte array of uint4s.  Overlay this structure upon an 8 byte quantity for easy
  * access to the lower or upper 4 bytes using lsb_index and msb_index respectively.
  */
 typedef struct
 {
	uint4	value[2];
 } non_native_uint8;

#ifdef BIGENDIAN
#  define	msb_index		0
#  define	lsb_index		1
#  else
#  define	msb_index		1
#  define	lsb_index		0
#endif

#ifdef INT8_SUPPORTED
   typedef	gtm_uint64_t		qw_num;
   typedef	gtm_uint64_t		seq_num;	/* Define 8-byte sequence number */
   typedef	gtm_uint64_t		token_num;	/* Define 8-byte token number */
   typedef	gtm_uint64_t		qw_off_t;	/* quad-word offset */
#  define	DWASSIGNQW(A,B)		(A)=(uint4)(B)
#  define	QWASSIGN(A,B)		(A)=(B)
#  define	QWASSIGNDW(A,B)		QWASSIGN((A),(gtm_uint64_t)(B))
#  define	QWASSIGN2DW(A,B,C)	QWASSIGN((A),(gtm_uint64_t)(B) << 32 | (C))
#  define	QWADD(A,B,C)		(A)=(B)+(C)
#  define	QWSUB(A,B,C)		(A)=(B)-(C)
#  define	QWADDDW(A,B,C)		(A)=(B)+(gtm_uint64_t)(C)
#  define	QWSUBDW(A,B,C)		(A)=(B)-(gtm_uint64_t)(C)
#  define	QWINCRBY(A,B)		(A)+=(B)
#  define	QWDECRBY(A,B)		(A)-=(B)
#  define	QWINCRBYDW(A,B)		(A)+=(gtm_uint64_t)(B)
#  define	QWDECRBYDW(A,B)		(A)-=(gtm_uint64_t)(B)
#  define	QWMULBYDW(A,B,C)	(A)=(B)*(C)
#  define	QWDIVIDEBYDW(A,B,Q,R)	{(R)=(A)%(B); (Q)=(A)/(B);}
#  define	QWMODDW(A,B)		((A)%(B))
#  define	QWLE(A,B)		((A)<=(B))
#  define	QWLT(A,B)		((A)<(B))
#  define	QWGE(A,B)		((A)>=(B))
#  define	QWGT(A,B)		((A)>(B))
#  define	QWEQ(A,B)		((A)==(B))
#  define	QWNE(A,B)		((A)!=(B))
#  define	INT8_PRINT(x)		x
#  define	INT8_PRINTX(x)		x
#  define	INT8_ONLY(x)		x
#else
 typedef struct non_native_uint8	qw_num;
 typedef struct non_native_uint8	seq_num;
 typedef struct non_native_uint8	token_num;
 typedef struct non_native_uint8	qw_off_t;

#  define	DWASSIGNQW(A,B)		(A)=(B).value[lsb_index]
#  define	QWASSIGN(A,B)		(A)=(B)
#  define	QWASSIGNDW(A,B)		{(A).value[msb_index]=0; (A).value[lsb_index]=B;}
#  define	QWASSIGN2DW(A,B,C)	{(A).value[msb_index]=B; (A).value[lsb_index]=C;}
#  define	QWADD(A,B,C)		{									\
						uint4 temp;							\
						temp = (B).value[lsb_index];					\
						(A).value[lsb_index]=(B).value[lsb_index]+(C).value[lsb_index]; \
						(A).value[msb_index]=(B).value[msb_index]+(C).value[msb_index]; \
						if ((A).value[lsb_index] < temp) (A).value[msb_index]++;	\
					}
#  define	QWSUB(A,B,C)		{									\
						uint4 temp;							\
						temp = (B).value[lsb_index];					\
						(A).value[lsb_index]=(B).value[lsb_index]-(C).value[lsb_index]; \
						(A).value[msb_index]=(B).value[msb_index]-(C).value[msb_index]; \
						if ((A).value[lsb_index] > temp) (A).value[msb_index]--;	\
					}
#  define	QWADDDW(A,B,C)		{									\
						uint4 temp;							\
						temp = (B).value[lsb_index];					\
						(A).value[lsb_index]=(B).value[lsb_index]+C;			\
						(A).value[msb_index]=(B).value[msb_index];			\
						if ((A).value[lsb_index] < temp) (A).value[msb_index]++;	\
					}
#  define	QWSUBDW(A,B,C)		{									\
						uint4 temp;							\
						temp = (B).value[lsb_index];					\
						(A).value[lsb_index]=(B).value[lsb_index]-(C);			\
						(A).value[msb_index]=(B).value[msb_index];			\
						if ((A).value[lsb_index] > temp) (A).value[msb_index]--;	\
					}
#  define	QWINCRBY(A,B)		QWADD(A,A,B)
#  define	QWDECRBY(A,B)		QWSUB(A,A,B)
#  define	QWINCRBYDW(A,B)		QWADDDW(A,A,B)
#  define	QWDECRBYDW(A,B)		QWSUBDW(A,A,B)

				/* B should be less than 64K for the QWDIDIVEBYDW, QWMODDW macros to work correctly */

#  define	QWMULBYDW(A,B,C)	{									\
						uint4	bh, bl, ch, cl, temp, temp1, temp2;			\
						(A).value[msb_index] = (B).value[msb_index] * (C);		\
						bl = (B).value[lsb_index] & 0x0000ffff;				\
						bh = ((B).value[lsb_index] & 0xffff0000) >> 16;			\
						cl = (C) & 0x0000ffff;						\
						ch = ((C) & 0xffff0000) >> 16;					\
						(A).value[msb_index] += bh * ch;				\
						(A).value[lsb_index] = bl * cl;					\
						temp = temp1 = bh * cl;						\
						temp += bl * ch;						\
						if (temp1 > temp)						\
							(A).value[msb_index] += 0x00010000;			\
						temp2 = (A).value[lsb_index];					\
						(A).value[lsb_index] += (temp & 0x0000ffff) << 16;		\
						if ((A).value[lsb_index] < temp2)				\
							(A).value[msb_index] ++;				\
						(A).value[msb_index] += (temp & 0xffff0000) >> 16;		\
					}
#  define	QWDIVIDEBYDW(A,B,Q,R)	{									\
						uint4 msbr, lsbq, twoq, twor;					\
						(R) = (A).value[lsb_index] % (B);				\
						lsbq = (A).value[lsb_index] / (B);				\
						msbr = A.value[msb_index] % B;					\
						(Q).value[msb_index] = (A).value[msb_index] / (B);		\
						twoq = ((uint4)-1) / (B);					\
						twor = (((uint4)-1) % (B) + 1) % (B);				\
						if (0 == twor)							\
							twoq++;							\
						(Q).value[lsb_index] = lsbq;					\
						(Q).value[lsb_index] += twoq * msbr;				\
						if ((Q).value[lsb_index] < lsbq)				\
							(Q).value[msb_index]++;					\
						(R) = (R) + (twor * msbr) % (B);				\
						lsbq = (Q).value[lsb_index];					\
						(Q).value[lsb_index] += (twor * msbr) / (B);			\
						if ((R) > (B))							\
						{								\
							(R) -= (B);						\
							(Q).value[lsb_index]++;					\
						}								\
						if ((Q).value[lsb_index] < lsbq)				\
							(Q).value[msb_index]++;					\
					}
#  define	QWMODDW(A,B)		((((A).value[msb_index] % (B)) * (((uint4)-1) % (B) + 1)		\
								+ (A).value[lsb_index]) % (B))
#  define	QWLE(A,B)		((A).value[msb_index] < (B).value[msb_index] ||				\
						((A).value[msb_index] == (B).value[msb_index]			\
							&& (A).value[lsb_index] <= (B).value[lsb_index]))
#  define	QWLT(A,B)		((A).value[msb_index] < (B).value[msb_index] ||				\
						((A).value[msb_index] == (B).value[msb_index]			\
							&& (A).value[lsb_index] < (B).value[lsb_index]))
#  define	QWGE(A,B)		((A).value[msb_index] > (B).value[msb_index] ||				\
						((A).value[msb_index] == (B).value[msb_index]			\
							&& (A).value[lsb_index] >= (B).value[lsb_index]))
#  define	QWGT(A,B)		((A).value[msb_index] > (B).value[msb_index] ||				\
						((A).value[msb_index] == (B).value[msb_index]			\
							&& (A).value[lsb_index] > (B).value[lsb_index]))
#  define	QWEQ(A,B)		((A).value[msb_index] == (B).value[msb_index]				\
							&& (A).value[lsb_index] == (B).value[lsb_index])
#  define	QWNE(A,B)		((A).value[msb_index] != (B).value[msb_index]				\
							|| (A).value[lsb_index] != (B).value[lsb_index])
#  define	INT8_FMT		"%s"
#  define	INT8_FMTX		"[0x%s]"
#  define	INT8_PRINT(x)		(seq_num_ptr = i2ascl(seq_num_str, x),					\
						seq_num_str[seq_num_ptr - &seq_num_str[0]] = '\0', seq_num_str)
#  define	INT8_PRINTX(x)		(seq_num_ptrx = i2asclx(seq_num_strx, x),				\
						seq_num_strx[seq_num_ptrx - &seq_num_strx[0]] = '\0', seq_num_strx)
#  define	INT8_ONLY(x)
#endif

/* Define some basic types for shared memory (sm) access depending on whether the platform we are    */
/* using is capable of supporting 32 or 64 bit pointers or not.					     */

#ifdef DB64
#  if defined(__osf__) && defined(__alpha)
#    pragma pointer_size(save)
#    pragma pointer_size(long)
#  else
#    error UNSUPPORTED PLATFORM
#  endif
  typedef char *char_ptr_t;		/* Define 64 bit pointer to char */
  typedef unsigned char *uchar_ptr_t;	/* Define 64 bit pointer to unsigned char */
  typedef short *short_ptr_t;		/* Define 64 bit pointer to short */
  typedef unsigned short *ushort_ptr_t; /* Define 64 bit pointer to unsigned short */
  typedef int4 *int_ptr_t;		/* Define 64 bit pointer to int */
  typedef volatile int4 *vint_ptr_t;	/* Define 64 bit pointer to volatile int */
  typedef uint4 *uint_ptr_t;		/* Define 64 bit pointer to uint */
  typedef volatile uint4 *vuint_ptr_t;	/* Define 64 bit pointer to volatile uint */
  typedef void *void_ptr_t;		/* Define 64 bit pointer to void */
  typedef qw_num *qw_num_ptr_t;		/* Define 64 bit pointer to qw_num */
#ifndef __vax
  typedef latch_t *latch_ptr_t;		/* Define 64 bit pointer to latch_t */
  typedef ulatch_t *ulatch_ptr_t;	/* Define 64 bit pointer to ulatch_t */
#endif

  /* Shared memory connotation */
  typedef char_ptr_t sm_c_ptr_t;	/* Define 64 bit pointer to char */
  typedef uchar_ptr_t sm_uc_ptr_t;	/* Define 64 bit pointer to unsigned char */
  typedef short_ptr_t sm_short_ptr_t;	/* Define 64 bit pointer to short */
  typedef ushort_ptr_t sm_ushort_ptr_t; /* Define 64 bit pointer to unsigned short */
  typedef int_ptr_t sm_int_ptr_t;	/* Define 64 bit pointer to int */
  typedef vint_ptr_t sm_vint_ptr_t;	/* Define 64 bit pointer to volatile int */
  typedef uint_ptr_t sm_uint_ptr_t;	/* Define 64 bit pointer to uint */
  typedef vuint_ptr_t sm_vuint_ptr_t;	/* Define 64 bit pointer to volatile uint */
  typedef gtm_int64_t sm_long_t;	/* Define 64 bit integer type */
  typedef gtm_uint64_t sm_ulong_t;	/* Define 64 bit unsigned integer type */
  typedef global_latch_t *sm_global_latch_ptr_t; /* Define 64 bit pointer to hp_latch */
#  ifdef __osf__
#    pragma pointer_size(restore)
#  endif
  /* The macro FILL8DCL (explained below) is simple on a 64 bit system since all 64 bits
     will be declared and used. */
#  define FILL8DCL(type,name,fillnum) type name
#else
  typedef char *char_ptr_t;		/* Define 32 bit pointer to char */
  typedef unsigned char *uchar_ptr_t;	/* Define 32 bit pointer to unsigned char */
  typedef short *short_ptr_t;		/* Define 32 bit pointer to short */
  typedef unsigned short *ushort_ptr_t; /* Define 32 bit pointer to unsigned short */
  typedef int4 *int_ptr_t;		/* Define 32 bit pointer to int */
  typedef volatile int4 *vint_ptr_t;	/* Define 32 bit pointer to volatile int */
  typedef uint4 *uint_ptr_t;		/* Define 32 bit pointer to uint */
  typedef volatile uint4 *vuint_ptr_t;	/* Define 32 bit pointer to volatile uint */
  typedef void *void_ptr_t;		/* Define 32 bit pointer to void */
  typedef qw_num *qw_num_ptr_t;		/* Define 32 bit pointer to qw_num */
#ifndef __vax
  typedef latch_t *latch_ptr_t;		/* Define 32 bit pointer to latch_t */
  typedef ulatch_t *ulatch_ptr_t;	/* Define 32 bit pointer to ulatch_t */
#endif

  /* Shared memory connotation */
  typedef char_ptr_t sm_c_ptr_t;	/* Define 32 bit pointer to char */
  typedef uchar_ptr_t sm_uc_ptr_t;	/* Define 32 bit pointer to unsigned char */
  typedef short_ptr_t sm_short_ptr_t;	/* Define 32 bit pointer to short */
  typedef ushort_ptr_t sm_ushort_ptr_t; /* Define 32 bit pointer to unsigned short */
  typedef int_ptr_t sm_int_ptr_t;	/* Define 32 bit pointer to int */
  typedef vint_ptr_t sm_vint_ptr_t;	/* Define 32 bit pointer to volatile int */
  typedef uint_ptr_t sm_uint_ptr_t;	/* Define 32 bit pointer to uint */
  typedef vuint_ptr_t sm_vuint_ptr_t;	/* Define 32 bit pointer to volatile uint */
  typedef int4 sm_long_t;		/* Define 32 bit integer type */
  typedef uint4 sm_ulong_t;		/* Define 32 bit unsigned integer type */
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
#  define OFF_T(name,fillnum) off_t	name
#else
#  define OFF_T(name,fillnum) FILL8DCL(off_t,name,fillnum)
#endif

/* Type for offsets in journal files.  VMS uses uint4 to get a full 32 bit
   offset for large journal files (OK since doesn't use lseek/etc. for IO.) */

#ifdef OFF_T_LONG
#  define JNL_OFF_T(name,fillnum) off_t	    name
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
# define CACHELINE_PAD(fieldSize, fillnum) char fill_cacheline##fillnum[CACHELINE_SIZE - (fieldSize)];
#else
# define CACHELINE_PAD(fieldSize, fillnum)
#endif

/* In certain cases we need to conditionally do a CACHELINE pad. For those platforms that
   have load-locked/store-conditional logic, counters that are incremented under interlock
   need to have spacing so they do not interfere with each other. But platforms that do
   NOT have this capability need the spacing on the actual latch used instead. Hence this
   form of padding is conditional.
*/
#if defined(__alpha) || defined(_AIX)
#  define CACHELINE_PAD_COND(fieldSize, fillnum) CACHELINE_PAD(fieldSize, fillnum)
#else
#  define CACHELINE_PAD_COND(fieldSize, fillnum)
#endif

#define MEMCP(dst,src,start,count,limit){ \
	if(start+count > limit) \
		rts_error(VARLSTCNT(1) ERR_CPBEYALLOC); \
	else	\
		memcpy(dst+start,src,count); \
}

#ifndef USING_ICONV
typedef enum
{
	NO_XLAT = 0,
	EBCDIC_TO_ASCII,
	ASCII_TO_EBCDIC
} gtm_iconv_t;
#define iconv_t gtm_iconv_t
#endif

#ifdef _AIX
# define VSIG_ATOMIC_T	sig_atomic_t
#else
# define VSIG_ATOMIC_T	volatile sig_atomic_t
#endif

/* For copying va_list items - Linux/390 needs __va_copy */
#ifndef VAR_COPY
#define VAR_COPY(dst, src)   dst = src
#endif

#define NOLICENSE	/* cheap way to obsolete it */

/* i2 conversation functions */
void i2hex(unsigned int val, uchar_ptr_t dest, int len);
void i2hexl(qw_num val, uchar_ptr_t dest, int len);
void i2hex_blkfill(int num, uchar_ptr_t addr, int len);
void i2hexl_blkfill(qw_num num, uchar_ptr_t addr, int len);
int i2hex_nofill(int num, uchar_ptr_t addr, int len);
int i2hexl_nofill(qw_num num, uchar_ptr_t addr, int len);

uchar_ptr_t i2ascl(uchar_ptr_t p, qw_num n);
uchar_ptr_t i2asclx(uchar_ptr_t p, qw_num n);
uchar_ptr_t i2asc(uchar_ptr_t p, unsigned int n);
int i2a(unsigned char *des, int *des_len, int num);
/* asc conversation functions */
int4 asc2i(uchar_ptr_t p, int4 len);
qw_num asc2l(uchar_ptr_t p, int4 len);
unsigned int asc_hex2i(char *p, int len);
/* double conversation */
void double2s(double *dp, mval *v);

int skpc(char c, int length, char *string);

void *gtm_malloc(size_t size);
void gtm_free(void *addr);
int gtm_memcmp (const void *, const void *, size_t);
#ifdef DEBUG
void printMallocInfo(void);
#endif
int is_equ(mval *u, mval *v);
char is_ident(mstr *v);

int nm_iscan(mval *v);

void mcfree(void);

int4 getprime(int4 n);

void push_parm();
void suspend(void);
mval *push_mval(mval *arg1);
void mval_lex(mval *v, mstr *output);

#ifdef VAX
char *i2s(int4 *i);
#endif

#define ZTRAP_CODE	0x00000001
#define ZTRAP_ENTRYREF	0x00000002
#define ZTRAP_POP	0x00000004
#define ZTRAP_ADAPTIVE	(ZTRAP_CODE | ZTRAP_ENTRYREF)

#define GTM_BYTESWAP_16(S)		\
	(  (((S) & 0xff00) >> 8)	\
	 | (((S) & 0x00ff) << 8)	\
	)

#define GTM_BYTESWAP_32(L)		\
	(  (((L) & 0xff000000) >> 24)	\
	 | (((L) & 0x00ff0000) >>  8)	\
	 | (((L) & 0x0000ff00) <<  8)	\
	 | (((L) & 0x000000ff) << 24)	\
	)

qw_num	gtm_byteswap_64(qw_num num64);
#ifdef INT8_SUPPORTED
#define GTM_BYTESWAP_64(LL)				\
	(  (((LL) & 0xff00000000000000ull) >> 56)	\
	 | (((LL) & 0x00ff000000000000ull) >> 40)	\
	 | (((LL) & 0x0000ff0000000000ull) >> 24)	\
	 | (((LL) & 0x000000ff00000000ull) >>  8)	\
	 | (((LL) & 0x00000000ff000000ull) <<  8)	\
	 | (((LL) & 0x0000000000ff0000ull) << 24)	\
	 | (((LL) & 0x000000000000ff00ull) << 40)	\
	 | (((LL) & 0x00000000000000ffull) << 56)	\
	)
#else
#define	GTM_BYTESWAP_64(LL) gtm_byteswap_64(LL)
#endif

#define ZDIR_FORM_FULLPATH		0x00000000
#define ZDIR_FORM_DIRECTORY		0x00000001
#define IS_VALID_ZDIR_FORM(zdirform)	(ZDIR_FORM_FULLPATH == (zdirform) || ZDIR_FORM_DIRECTORY == (zdirform))

#define MAXNUMLEN 		128	/* from PV_N2S */
#define CENTISECONDS		100	/* VMS lib$day returns 1/100s, we want seconds, use this factor to convert b/n the two */
#define MINUTE			60	/* seconds in a minute */
#define HOUR			3600	/* one hour in seconds 60 * 60 */
#define ONEDAY			86400	/* seconds in a day */
#define MILLISECS_IN_SEC	1000	/* millseconds in a second */
#define MICROSEC_IN_SEC		1000000 /* microseconds in a second */

#define ASSERT_IN_RANGE(low, x, high)	assert((low <= x) && (x <= high))

#if defined(VMS)
#define DAYS		6530  /* adjust VMS returned days by this amount; GTM zero time Dec 31, 1840, VMS zero time 7-NOV-1858 */
#define VARLSTCNT1(CNT)			VARLSTCNT(CNT)
#define PUT_SYS_ERRNO(SYS_ERRNO) 	SYS_ERRNO
#elif defined(UNIX)
#define DAYS		47117 /* adjust Unix returned days (seconds converted to days); Unix zero time 1970 */
#define VARLSTCNT1(CNT)			VARLSTCNT(CNT + 1)
#define PUT_SYS_ERRNO(SYS_ERRNO) 	0, SYS_ERRNO
#else
#error Unsupported platform
#endif

#define FORMATZWR_CONVERSION_FACTOR (sizeof("_$c()_") - 1)
#define EXIT_NRM	0
#define EXIT_INF	1
#define EXIT_WRN	2
#define EXIT_ERR	4
#define EXIT_RDONLY	8
#define EXIT_MASK 	7
#define MIN_FN_LEN	1
#define MAX_FN_LEN	255
#define MAX_TRANS_NAME_LEN	257

#define MAX_ZWR_INFLATION	6 	/* Inflation of key length when converting to ZWR representation.
					 * The worst case is that every other character is a non-graphic with a 3 digit code
					 * e.g. "a"_$c(127)_"a"_$C(127)...
					 * which leads to (n / 2 * 4) + n / 2 * 8) = n / 2 * 12 = n * 6 */
typedef uint4 		jnl_tm_t;
typedef uint4 		off_jnl_t;
typedef int4 		sm_off_t;

#define MAXUINT8	((gtm_uint64_t)-1)
#define MAXUINT4	((uint4)-1)
#define MAXUINT2	((unsigned short)-1)

/* On platforms that support native 8 byte operations (such as Alpha), an assignment to an 8 byte field is atomic. On other
 * platforms, an 8 byte assignment is a sequence of 4 byte operations. On such platforms, use this macro to determine if the
 * change from the current value to the new value provides a consistent view (entirely the pre read, or entirely the post read,
 * and not in between). Any change that causes the most significant 4 bytes to differ can cause inconsistency. In such cases, it
 * may be necessary to grab crit if modifying a shared field.
 */
#ifdef  INT8_NATIVE
#define QWCHANGE_IS_READER_CONSISTENT(FROM8, TO8)	(TRUE)
#else
/* Note: cannot use this macro when FROM8 or TO8 do not have an lvalue (eg.  literal) */
#define QWCHANGE_IS_READER_CONSISTENT(FROM8, TO8)	(((non_native_uint8 *)&(FROM8))->value[msb_index]	\
							 == ((non_native_uint8 *)&(TO8))->value[msb_index])
#endif

#endif /* MDEF_included */
