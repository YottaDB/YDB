/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MDEFSP_included
#define MDEFSP_included

#define INT8_SUPPORTED
#define	INT8_FMT		"%llu"
#define	INT8_FMTX		"[0x%llx]"
#define INT8_NATIVE /* on Alpha processors, 8 byte operations are native to the chip and hence atomic */

#include <builtins.h>
#include <lib$routines>

#define insqhi vax_insqhi
#define insqti vax_insqti
#define remqhi vax_remqhi
#define remqti vax_remqti

/* Use our own malloc and free to guarantee return value is checked for error condition.  */
#ifdef __cplusplus
extern "C" void *gtm_malloc(int);
extern "C" void gtm_free(void *);
/* extern "C" int gtm_memcmp (const void *, const void *, int); */
#endif

/* NOTE: this redefines stringpool.free as stringpool.gtm_free, but that appears benign.  */
#define malloc gtm_malloc
#define free gtm_free

#define rts_error lib$signal
#ifdef __cplusplus
#define error_def(x) extern x
#else
#define error_def(x) globalvalue x
#endif

#define INTERLOCK_ADD(X,Y,Z) (__ATOMIC_ADD_LONG((sm_vint_ptr_t)(X), (Z)) + (Z))

#undef BIGENDIAN

#ifdef __cplusplus
#define GBLDEF
#define GBLREF extern
#define LITDEF const
#define LITREF extern const
#else
#define GBLDEF globaldef
#define GBLREF globalref
#define LITDEF const globaldef
#define LITREF const globalref
#endif

/* Reserve enough space in routine header for call to GTM$MAIN.  */
#define RHEAD_JSB_SIZE	12
typedef struct
{
	unsigned short	mvtype;
	unsigned	e   	:  7;
	unsigned	sgn	:  1;
	unsigned char	fnpc_indx;	/* Index to fnpc_work area this mval is using */
	mstr		str    ;
	int4		m[2]   ;
} mval;
/* Another version of mval with byte fields instead of bit fields */
typedef struct
{
	unsigned short	mvtype;
	unsigned char	sgne;
	unsigned char	fnpc_indx;	/* Index to fnpc_work area this mval is using */
	mstr		str    ;
	int4		m[2]   ;
} mval_b;

#define VAR_START(a, b)	va_start(a, b)
#define VARLSTCNT(a)			/* stub for argument count.  VAX CALLS instruction pushes count automatically */

#define CHF_MCH_DEPTH	chf$q_mch_depth
#define CHF_MCH_SAVR0	chf$q_mch_savr0

typedef struct
{
	short		flags;
	short		rsa_offset;
	unsigned char	reserved_1;
	unsigned char	fret;
	short		signature_offset;
	void		*entry;		/* actually a pointer to a code address, but not representable directly in C */
	int4		reserved_2;	/* other half of quadword pointer (entry) not allocated by C compiler */
	int4		size;
	short		reserved_3;
	short		entry_length;
} alpha_procedure_descriptor;

#define CODE_ADDRESS(func)	gtm$code_address(func)
#define GTM_CONTEXT(func)	func

/* External symbols (global symbols defined in another module) are represented as belonging to this PSECT: */
#define GTM_ANOTHER_MODULE	-1

/* Under Alpha AXP OpenVMS, the procedure value of a module is defined in the PSECT used for linkage: */
#define GTM_MODULE_DEF_PSECT	GTM_LINKAGE

#define OS_PAGELET_SIZE		512
#define OS_VIRTUAL_BLOCK_SIZE	OS_PAGELET_SIZE

typedef volatile	int4    latch_t;
typedef volatile	uint4   ulatch_t;

#define INSIDE_CH_SET		"ISO8859-1"
#define OUTSIDE_CH_SET		"ISO8859-1"
#define EBCDIC_SP		0x40
#define NATIVE_SP		0x20
#define DEFAULT_CODE_SET	ascii	/* enum ascii defined in io.h */

#define util_out_print2 util_out_print

int adawi(short x, short *y);

#define CACHELINE_SIZE		256	/* alpha cache line size */

#endif /* MDEFSP_included */
