/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef COMPILER_H_INCLUDED
#define COMPILER_H_INCLUDED

/* Values for oprclass - Changes made here need to be reflected in cdbg_dump opr_type_names table */
typedef enum
{
	NO_REF,		/* 0 */
	TVAR_REF,	/* 1 */
	TVAL_REF,	/* 2 */
	TINT_REF,	/* 3 */
	TVAD_REF,	/* 4 */
	TCAD_REF,	/* 5  - This and all _REFs before are VALUED_REF_TYPES (see alloc_reg.c) */
	MLIT_REF,	/* 6 */
	MVAR_REF,	/* 7 */
	TRIP_REF,	/* 8 */
	TNXT_REF,	/* 9  */
	TJMP_REF,	/* 10 */
	INDR_REF,	/* 11 */
	MLAB_REF,	/* 12 */
	ILIT_REF,	/* 13 */
	CDLT_REF,	/* 14 - apparently no longer used */
	TEMP_REF,	/* 15 - apparently no longer maintained */
	MFUN_REF,	/* 16 */
	MNXL_REF,	/* 17 refer to internalentry of child line */
	TSIZ_REF,	/* 18 ilit refering to size of given triple codegen */
	OCNT_REF,	/* 19 Offset from Call to Next Triple */
	CDIDX_REF	/* 20 Denotes index into a table containing a code address */
} operclass;
#define VALUED_REF_TYPES 6	/* Types 0-5 are specific types used by alloc_reg() in array references
				 * **** WARNING **** Do NOT reorder
				 */

typedef unsigned int	opctype;

typedef struct	mvarstruct
{
	struct	mvarstruct	*lson,
				*rson;
	int4			mvidx;
	mident			mvname;
	struct	tripletype	*last_fetch;
} mvar;

typedef struct	mvaxstruct
{
	struct	mvaxstruct	*last,
				*next;
	mvar			*var;
	int4			mvidx;
} mvax;

typedef struct	mlinestruct
{
	struct	mlinestruct	*parent,
				*sibling,
				*child;
	struct	tripletype	*externalentry;
	uint4			line_number;	/* ...operation on this line */
	boolean_t		table;		/* put in table or not */
	boolean_t		block_ok;	/* saw argumentless DO or not */
} mline;

typedef struct	mlabstruct
{
	struct	mlabstruct	*lson,
				*rson;
	mline			*ml;
	mident			mvname;
	int			formalcnt;
	boolean_t		gbl;
} mlabel;

typedef struct	mliteralstruct
{
	struct
	{
		struct	mliteralstruct	*fl,
					*bl;
	}			que;
	INTPTR_T       		rt_addr;
	int			reference_count;	/* Used in the hash table to track references */
	mval			v;
} mliteral;

typedef struct	triplesize
{
	struct tripletype	*ct;
	int4			size;
} tripsize;

typedef struct	oprtypestruct
{
	operclass		oprclass;
	union
	{
		struct	oprtypestruct	*indr;
		struct	tripletype	*tref;
		struct	triplesize	*tsize;
		mlabel			*lab;
		mline			*mlin;
		mliteral		*mlit;
		mstr			*cdlt;
		mstr			*cdidx;
		mvar			*vref;
		int4			temp;
		int4			ilit;
		int4			offset;
		unsigned char		vreg;
	} oprval;
} oprtype;

typedef struct	tbptype
{
	struct
	{
		struct	tbptype		*fl,
					*bl;
	}			que;
	struct	tripletype	*bpt;
} tbp;

typedef struct
{
	uint4			line;
	uint4			column;
} source_address;

typedef struct	tripletype
{
	opctype			opcode;
	struct
	{
		struct	tripletype	*fl,
					*bl;
	}			exorder;
	tbp			backptr,	/* triples which reference this triple's value */
				jmplist;	/* triples which jump to this one */
	source_address		src;
	int			rtaddr;		/* relative run time address of triple */
	oprtype			operand[2],
				destination;
} triple;

typedef struct
{
	uint4		octype;
} octabstruct;

/* Values for octype */
#define OCT_NULL	0
#define OCT_MVAL	1
#define OCT_MINT	2
#define OCT_MVADDR	4
#define OCT_CDADDR	8
#define OCT_VALUE	(OCT_MVAL | OCT_MINT | OCT_CDADDR)
#define OCT_BOOL	16
#define OCT_JUMP	32
#define OCT_EXPRLEAF	64
#define OCT_CGSKIP	128
#define OCT_COERCE	256
#define OCT_ARITH	512
#define OCT_UNARY	1024
#define OCT_NEGATED	2048
#define OCT_REL	4096

typedef struct
{
	char			name[20];
	opctype			bo_type;
	char			uo_type;
	unsigned short		opr_type;
} toktabtype;

/* These two structures really belong in glvn_pool.h, but gtmpcat doesn't know to include that file. So put them here for now. */
#include "callg.h"
typedef struct
{
	opctype			sav_opcode;
	uint4			mval_top;			/* mval just beyond ones used by this entry */
	uint4			precursor;			/* index of previous FOR slot at same level */
	mval			*lvname;
	gparam_list		glvn_info;
} glvn_pool_entry;

typedef struct
{
	uint4			capacity;			/* total # allocated entries */
	uint4			top;				/* current available glvn_pool_entry slot */
	uint4			for_slot[MAX_FOR_STACK + 1];	/* indices of most recent FOR slots */
	uint4			share_slot;			/* currently active slot */
	opctype			share_opcode;			/* currently active opcode */
	uint4			mval_capacity;			/* total # allocated mvals */
	uint4			mval_top;			/* current available mval in mval_stack */
	mval			*mval_stack;			/* stack of mvals */
	glvn_pool_entry		slot[1];			/* stack of entries */
} glvn_pool;

#define VMS_OS  01
#define UNIX_OS 02
#define ALL_SYS (VMS_OS | UNIX_OS)
#ifdef UNIX                     /* function and svn validation are a function of the OS */
#	 define VALID_FUN(i) (fun_data[i].os_syst & UNIX_OS)
#	 define VALID_SVN(i) (svn_data[i].os_syst & UNIX_OS)
#	 ifdef __hppa
#	 	 define TRIGGER_OS 0
#	 else
#	 	 define TRIGGER_OS UNIX_OS
#	 endif
#elif defined VMS
#	 define VALID_FUN(i) (fun_data[i].os_syst & VMS_OS)
#	 define VALID_SVN(i) (svn_data[i].os_syst & VMS_OS)
#	 define TRIGGER_OS 0
#else
#	 error UNSUPPORTED PLATFORM
#endif

#define MUMPS_INT	0	/* integer - they only type the compiler handles differently */
#define MUMPS_EXPR	1	/* expression */
#define MUMPS_STR	2	/* string */
#define MUMPS_NUM	3	/* numeric - potentially non-integer */

#define EXPR_FAIL	0	/* expression had syntax error - frequently represented by FALSE*/
#define EXPR_GOOD	1	/* expression ok, no indirection at root - frequently represented by TRUE */
#define EXPR_INDR	2	/* expression ok, indirection at root */
#define EXPR_SHFT	4	/* expression ok, involved shifted GV references */

#define CHARMAXARGS	256
#define MAX_FORARGS	127
#define MAX_SRCLINE	8192	/* maximum length of a program source or indirection line */
#define NO_FORMALLIST	(-1)

/* Some errors should not cause stx_error to issue an rts_error. These are the errors related to
 *	a) Invalid Intrinsic Commands
 *	b) Invalid Intrinsic Function Names
 *	c) Invalid Intrinsic Special Variables
 *	d) Invalid Deviceparameters for IO commands
 * These should cause an error at runtime if and only if that codepath is reached.
 * PostConditionals can cause this path to be avoided in which case we do not want to issue an error at compile time.
 * Therefore issue only a warning at compile-time and proceed with compilation as if this codepath will not be reached at runtime.
 */
error_def(ERR_BOOLSIDEFFECT);
error_def(ERR_DEVPARINAP);
error_def(ERR_DEVPARUNK);
error_def(ERR_DEVPARVALREQ);
error_def(ERR_FNOTONSYS);
error_def(ERR_INVCMD);
error_def(ERR_INVFCN);
error_def(ERR_INVSVN);
error_def(ERR_SVNONEW);
error_def(ERR_SVNOSET);

#define	IS_STX_WARN(errcode)										\
	((ERR_DEVPARINAP == errcode) || (ERR_DEVPARUNK == errcode) || (ERR_DEVPARVALREQ == errcode)	\
		|| (ERR_FNOTONSYS == errcode) || (ERR_INVCMD == errcode) || (ERR_INVFCN == errcode) 	\
		|| (ERR_INVSVN == errcode) || (ERR_SVNONEW == errcode) || (ERR_SVNOSET == errcode))

/* This macro does an "stx_error" of the input errcode but before that it asserts that the input errcode is one
 * of the known error codes that are to be handled as a compile-time warning (instead of an error). It also set
 * the variable "parse_warn" to TRUE which is relied upon by the functions that invoke this macro. Note that when
 * triggers are included, warnings become errors so bypass the warning stuff.
 */
#ifdef GTM_TRIGGER
#	define	STX_ERROR_WARN(errcode)					\
{									\
	if (!TREF(trigger_compile_and_link))				\
		parse_warn = TRUE;					\
	assert(IS_STX_WARN(errcode));					\
	stx_error(errcode);						\
	if (TREF(trigger_compile_and_link))				\
		return FALSE;						\
}
#else
#	define	STX_ERROR_WARN(errcode)					\
{									\
	parse_warn = TRUE;						\
	assert(IS_STX_WARN(errcode));					\
	stx_error(errcode);						\
}
#endif

#ifdef DEBUG
#	 define COMPDBG(x)	if (gtmDebugLevel & GDL_DebugCompiler) {x}
#else
#	 define COMPDBG(x)
#endif

/* Cutover from simple lists to hash table access - tuned by testing compilation
   of 24K+ Vista M source routines.
*/
#include "copy.h"
#define LIT_HASH_CUTOVER DEBUG_ONLY(4) PRO_ONLY(32)
#define SYM_HASH_CUTOVER DEBUG_ONLY(4) PRO_ONLY(16)
#define COMPLITS_HASHTAB_CLEANUP									\
	{												\
		GBLREF hash_table_str		*complits_hashtab;					\
		if (complits_hashtab && complits_hashtab->base)						\
		{	/* Release hash table itself but leave hash table descriptor if exists */ 	\
			free_hashtab_str(complits_hashtab);						\
		}											\
	}
#define COMPSYMS_HASHTAB_CLEANUP									\
	{												\
		GBLREF hash_table_str		*compsyms_hashtab;					\
		if (compsyms_hashtab && compsyms_hashtab->base)						\
		{	/* Release hash table itself but leave hash table descriptor if exists */ 	\
			free_hashtab_str(compsyms_hashtab);						\
			compsyms_hashtab->base = NULL;							\
		}											\
	}
#define COMPILE_HASHTAB_CLEANUP		\
	COMPLITS_HASHTAB_CLEANUP;	\
	COMPSYMS_HASHTAB_CLEANUP;

typedef struct
{
	triple		*expr_start;
	triple		*expr_start_orig;
	boolean_t	shift_side_effects;
	boolean_t	saw_side_effect;
	triple		tmpchain;
} save_se;

#define START_GVBIND_CHAIN(SS, OLDCHAIN)					\
{										\
	(SS)->expr_start = TREF(expr_start);					\
	(SS)->expr_start_orig = TREF(expr_start_orig);				\
	(SS)->shift_side_effects = TREF(shift_side_effects);			\
	(SS)->saw_side_effect = TREF(saw_side_effect);				\
	TREF(expr_start) = NULL;						\
	TREF(expr_start_orig) = NULL;						\
	TREF(shift_side_effects) = FALSE;					\
	TREF(saw_side_effect) = FALSE;						\
	dqinit(&(SS)->tmpchain, exorder);					\
	OLDCHAIN = setcurtchain(&(SS)->tmpchain);				\
}

#define PLACE_GVBIND_CHAIN(SS, OLDCHAIN)					\
{										\
	newtriple(OC_GVSAVTARG);						\
	TREF(expr_start) = (SS)->expr_start;					\
	TREF(expr_start_orig) = (SS)->expr_start_orig;				\
	TREF(shift_side_effects) = (SS)->shift_side_effects;			\
	TREF(saw_side_effect) = (SS)->saw_side_effect;				\
	setcurtchain(OLDCHAIN);							\
	assert(NULL != TREF(expr_start));					\
	dqadd(TREF(expr_start), &(SS)->tmpchain, exorder);			\
	TREF(expr_start) = (SS)->tmpchain.exorder.bl;				\
	assert(OC_GVSAVTARG == (TREF(expr_start))->opcode);			\
	newtriple(OC_GVRECTARG)->operand[0] = put_tref(TREF(expr_start));	\
}

/* note assignment below */
#define SHIFT_SIDE_EFFECTS	((TREF(saw_side_effect) = TREF(shift_side_effects)) && (GTM_BOOL == TREF(gtm_fullbool)))

#define INITIAL_SIDE_EFFECT_DEPTH 33	/* initial allocation for expression nesting to track side effects */

/* note side effect for boolean shifting temporaries */
#define ENCOUNTERED_SIDE_EFFECT										\
{	/* Needs #include "show_source_line" and #include "fullbool.h" */				\
													\
	if (TREF(shift_side_effects))									\
	{												\
		TREF(saw_side_effect) = TRUE;								\
		if (!run_time && (FULL_BOOL_WARN == TREF(gtm_fullbool)))				\
		{	/* warnings requested by by gtm_fullbool and enabled by eval_expr */		\
			show_source_line(TRUE);								\
			dec_err(VARLSTCNT(1) ERR_BOOLSIDEFFECT);					\
		}											\
	}												\
}

#define SE_WARN_ON	(!run_time && (SE_WARN == TREF(side_effect_handling)))

#define ISSUE_SIDEEFFECTEVAL_WARNING(COLUMN)						\
{											\
	TREF(last_source_column) = (COLUMN);						\
	show_source_line(TRUE);								\
	dec_err(VARLSTCNT(1) ERR_SIDEEFFECTEVAL);					\
}

/* maintain array indexed by expr_depth to track side effects - for subscripts, actuallists, binary expressions and functions */
#define INCREMENT_EXPR_DEPTH													\
{																\
	boolean_t	*TMP_BASE;												\
																\
	if (!(TREF(expr_depth))++)												\
		TREF(expr_start) = TREF(expr_start_orig) = NULL;								\
	else															\
	{	/* expansion is unlikely as it's hard to nest expressions deeply, but we don't want a hard limit */		\
		assertpro(TREF(expr_depth));					/* expr_depth doesn't handle rollover */	\
		assert(TREF(expr_depth) <= TREF(side_effect_depth));								\
		if (TREF(expr_depth) == TREF(side_effect_depth))								\
		{														\
			TMP_BASE = TREF(side_effect_base);									\
			(TREF(side_effect_depth))++;										\
			TREF(side_effect_base) = malloc(SIZEOF(boolean_t) * TREF(side_effect_depth));				\
			memcpy(TREF(side_effect_base), TMP_BASE, SIZEOF(boolean_t) * TREF(expr_depth));				\
			free(TMP_BASE);												\
			(TREF(side_effect_base))[TREF(expr_depth)] = FALSE;			 				\
		}														\
	}															\
	assert(FALSE == (TREF(side_effect_base))[TREF(expr_depth)]);								\
}

/* complement of the above increment - uses the macro just below for assertpto and to clear the level we're leaving */
#define DECREMENT_EXPR_DEPTH													\
{																\
	DISABLE_SIDE_EFFECT_AT_DEPTH;												\
	if (!(--(TREF(expr_depth))))												\
		TREF(saw_side_effect) = TREF(shift_side_effects) = FALSE;							\
}

/* clear the current expr_depth level and propagate down */
#define DISABLE_SIDE_EFFECT_AT_DEPTH												\
{																\
	unsigned int	DEPTH;													\
																\
	DEPTH = TREF(expr_depth);												\
	assertpro(DEPTH);								/* expr_depth shouldn't underflow */	\
	(TREF(side_effect_base))[DEPTH - 1] |= (TREF(side_effect_base))[DEPTH];		/* propagate down */			\
	(TREF(side_effect_base))[DEPTH] = FALSE;										\
}

/* The following macro transfers subscripts from an array to the triple chain for gvn, lvn and name_glvn
* it requires includes for fullbool.m, mdq.h, and show_source_line.h, and also GBLREF of runtime
*/
#define SUBS_ARRAY_2_TRIPLES(REF1, SB1, SB2, SUBSCRIPTS, XTRA)									\
{																\
	boolean_t	PROTECT_LVN, SE_NOTIFY;											\
	triple 		*REF2;													\
																\
	if (PROTECT_LVN = (TREF(side_effect_base))[TREF(expr_depth)])	/* NOTE assignment */					\
		SE_NOTIFY = SE_WARN_ON;												\
	while (SB2 < SB1)													\
	{															\
		if (PROTECT_LVN && (SB2 > (SUBSCRIPTS + XTRA)) && ((SB1 - SB2) > 1)						\
				&& ((OC_VAR == SB2->oprval.tref->opcode) || (OC_GETINDX == SB2->oprval.tref->opcode)))		\
		{	/* protect lvns from side effects: skip 1st (unsubscripted name), and last (nothing following) */	\
			assert(OLD_SE != TREF(side_effect_handling));								\
			REF2 = maketriple(OC_STOTEMP);										\
			REF2->operand[0] = *SB2;										\
			dqins(SB2->oprval.tref, exorder, REF2); 		/* NOTE:this violates information hiding */	\
			if (SE_NOTIFY)												\
				ISSUE_SIDEEFFECTEVAL_WARNING(SB2->oprval.tref->src.column + 1);					\
			*SB2 = put_tref(REF2);											\
		}														\
		REF2 = newtriple(OC_PARAMETER);											\
		REF1->operand[1] = put_tref(REF2);										\
		REF1 = REF2;													\
		REF1->operand[0] = *SB2++;											\
	}															\
}

/* the macro below tucks a code reference into the for_stack so a FOR that's done can move on correctly when done */
#define FOR_END_OF_SCOPE(DEPTH, RESULT)										\
{														\
	oprtype	**Ptr;												\
														\
	assert(0 <= DEPTH);											\
	assert(TREF(for_stack_ptr) < (oprtype **)TADR(for_stack) + MAX_FOR_STACK);				\
	Ptr = (oprtype **)TREF(for_stack_ptr) - DEPTH;								\
	assert(Ptr >= (oprtype **)TADR(for_stack));								\
	if (NULL == *Ptr)											\
		*Ptr = (oprtype *)mcalloc(SIZEOF(oprtype));							\
	RESULT = put_indr(*Ptr);										\
}

#define GOOD_FOR  FALSE	/* single level */
#define BLOWN_FOR (!TREF(xecute_literal_parse))	/* all levels, except only one for our funky friend xecute_literal_parse */

/* Marco to decrement or clear the for_stack, clear the for_temp array
 * and generate code to release run-time malloc'd mvals anchored in the for_saved_indx array
 * The corresponding FOR_PUSH macro is in m_for.c but this one is used in stx_error.c for error cases
 */
#define	FOR_POP(ALL)												\
{														\
	assert(TREF(for_stack_ptr) >= (oprtype **)TADR(for_stack));						\
	assert(TREF(for_stack_ptr) <= (oprtype **)TADR(for_stack) + MAX_FOR_STACK);				\
	if (ALL)												\
	{													\
		(TREF(for_stack_ptr)) = (oprtype **)TADR(for_stack);						\
		*(TREF(for_stack_ptr)) = NULL;									\
	} else if (TREF(for_stack_ptr) > (oprtype **)TADR(for_stack))						\
		--(TREF(for_stack_ptr));									\
}

/* The following macro should be used when doing an OC_FETCH other than at the start of a line because
 * it deals with the possibility that there is a preceding FOR, in which case, we use start_for_fetches to
 * be sure the new fetch includes any FOR loop control variable
 */
#define MID_LINE_REFETCH											\
	if (TREF(for_stack_ptr) == TADR(for_stack))								\
		start_fetches(OC_FETCH);									\
	else	/* if in the body of a FOR loop we need to ensure the control variable is refetched */		\
		start_for_fetches();

/* $TEXT(+n^rtn) fetches from the most recently ZLINK'd version of rtn. $TEXT(+n) fetches from the currently executing version.
 * The compiler converts $TEXT(+n) to $TEXT(+n^!), indicating at runtime $TEXT should fetch from the current executing routine.
 * '!' is not a legal routine name character.
 */
#define CURRENT_RTN_STRING		"!"
#define PUT_CURRENT_RTN			put_str(LIT_AND_LEN(CURRENT_RTN_STRING))
/* Do we want the current routine? Return TRUE.
 * Otherwise, return FALSE; we want the latest version of whichever routine name is specified.
 */
#define WANT_CURRENT_RTN_MSTR(S)	((STR_LIT_LEN(CURRENT_RTN_STRING) == (S)->len)	\
						&& (0 == MEMCMP_LIT((S)->addr, CURRENT_RTN_STRING)))
#define WANT_CURRENT_RTN(R)		WANT_CURRENT_RTN_MSTR(&(R)->str)

#define NEWLINE_TO_NULL(C)		\
{					\
	if ('\n' == (char)(C))		\
		C = '\0';		\
}

/* Macro to clear parts of given compiler mval which could end up in an object file (if they are literals). These bits may not
 * be (re)set by the s2n/n2s calls we do. If not, the mval could have random bits in it which, as far as the mval is concerned
 * is not a problem but interferes with getting a consistent object hash value when the same source is (re)compiled.
 */
#define CLEAR_MVAL_BITS(mvalptr) 			\
{							\
	((mval_b *)(mvalptr))->sgne = 0;		\
	(mvalptr)->fnpc_indx = 0xff;			\
	UNICODE_ONLY((mvalptr)->utfcgr_indx = 0xff);	\
}

/* Macro to put a literal truth value as an operand */
#define PUT_LITERAL_TRUTH(TV, TRIP_REF)									\
MBSTART {												\
	LITREF mval		literal_zero, literal_one;						\
													\
	mval	*V;											\
													\
	V = (mval *)mcalloc(SIZEOF(mval));								\
	*V = (TV) ? literal_one : literal_zero;								\
	assert((1 == literal_one.str.len) && (1 == literal_zero.str.len));				\
	ENSURE_STP_FREE_SPACE(1);									\
	*(char *)stringpool.free = *V->str.addr;							\
	V->str.addr = (char *)stringpool.free;								\
	stringpool.free += 1;										\
	put_lit_s(V, TRIP_REF);										\
} MBEND

/* Macro to decide whether an invocation of unary tail has any promise */
#define UNARY_TAIL(OPR)											\
MBSTART {												\
	assert(TRIP_REF == (OPR)->oprclass);								\
	if (OCT_UNARY & oc_tab[(OPR)->oprval.tref->opcode].octype)					\
		unary_tail(OPR);									\
} MBEND

/* the following structure and macros save and restore parsing state and as of this writing are only used by m_xecute */
typedef struct
{
	boolean_t	source_error_found;
	int		source_column;
	int		director_ident_len;
	int4		block_level;
	mstr_len_t	source_len;
	mval		director_mval;
	unsigned char	ident_buffer[(MAX_MIDENT_LEN * 2) + 1];
	char		director_token;
	char		*lexical_ptr;
	char		window_token;
} parse_save_block;

#define SAVE_PARSE_STATE(SAVE_PARSE_PTR)									\
MBSTART {													\
	SAVE_PARSE_PTR->block_level = TREF(block_level);							\
	SAVE_PARSE_PTR->director_ident_len = (TREF(director_ident)).len;					\
	memcpy(SAVE_PARSE_PTR->ident_buffer, (TREF(director_ident)).addr, SAVE_PARSE_PTR->director_ident_len);	\
	SAVE_PARSE_PTR->director_mval = TREF(director_mval);							\
	SAVE_PARSE_PTR->director_token = TREF(director_token);							\
	SAVE_PARSE_PTR->lexical_ptr = TREF(lexical_ptr);							\
	SAVE_PARSE_PTR->source_column = source_column;								\
	SAVE_PARSE_PTR->source_error_found = TREF(source_error_found);						\
	SAVE_PARSE_PTR->source_len = (TREF(source_buffer)).len;							\
	SAVE_PARSE_PTR->window_token = TREF(window_token);							\
} MBEND

#define RESTORE_PARSE_STATE(SAVE_PARSE_PTR)								\
MBSTART {													\
	TREF(block_level) = SAVE_PARSE_PTR->block_level;							\
	(TREF(director_ident)).len = SAVE_PARSE_PTR->director_ident_len;					\
	memcpy((TREF(director_ident)).addr, SAVE_PARSE_PTR->ident_buffer, SAVE_PARSE_PTR->director_ident_len);	\
	TREF(director_mval) = SAVE_PARSE_PTR->director_mval;							\
	TREF(director_token) = SAVE_PARSE_PTR->director_token;							\
	TREF(lexical_ptr) = SAVE_PARSE_PTR->lexical_ptr;							\
	(TREF(source_buffer)).addr = (char *)&aligned_source_buffer;						\
	(TREF(source_buffer)).len = SAVE_PARSE_PTR->source_len;							\
	source_column = SAVE_PARSE_PTR->source_column;								\
	TREF(source_error_found) = SAVE_PARSE_PTR->source_error_found;						\
	TREF(window_token) = SAVE_PARSE_PTR->window_token;							\
} MBEND

#define RETURN_IF_RTS_ERROR							\
MBSTART {									\
	if (TREF(rts_error_in_parse))						\
		return;								\
} MBEND

#define RETURN_EXPR_IF_RTS_ERROR						\
MBSTART {									\
	if (TREF(rts_error_in_parse))						\
	{									\
		TREF(rts_error_in_parse) = FALSE;				\
		DECREMENT_EXPR_DEPTH;						\
		return EXPR_FAIL;						\
	}									\
} MBEND

#define ALREADY_RTERROR (OC_RTERROR == (TREF(curtchain))->exorder.bl->exorder.bl->exorder.bl->opcode)

/* Autorelink enabled platforms pass a different argument to glue code when calling a non-local M
 * routine. Use put_cdlt() to pass addresses of the items and use put_cdidx() when passing an ofset
 * into the linkage table where the items reside. Also macroize the opcode to use.
 */
#ifdef AUTORELINK_SUPPORTED
# define PUT_CDREF put_cdidx
# define OC_CDREF OC_CDIDX
# define CDREF_REF CDIDX_REF
#else
# define PUT_CDREF put_cdlt
# define OC_CDREF OC_CDLIT
# define CDREF_REF CDLT_REF
#endif

int		actuallist(oprtype *opr);
int		bool_expr(boolean_t sense, oprtype *addr);
void		bx_boollit(triple *t);
void		bx_boolop(triple *t, boolean_t jmp_type_one, boolean_t jmp_to_next, boolean_t sense, oprtype *addr);
void		bx_relop(triple *t, opctype cmp, opctype tst, oprtype *addr);
void		bx_tail(triple *t, boolean_t sense, oprtype *addr);
void		chktchain(triple *head);
void		code_gen(void);
void		coerce(oprtype *a, unsigned short new_type);
int		comp_fini(int status, mstr *obj, opctype retcode, oprtype *retopr, oprtype *dst, mstr_len_t src_len);
void		comp_init(mstr *src, oprtype *dst);
void		comp_indr(mstr *obj);
boolean_t	compiler_startup(void);
void		create_temporaries(triple *sub, opctype put_oc);
triple		*entryref(opctype op1, opctype op2, mint commargcode, boolean_t can_commarg, boolean_t labref, boolean_t textname);
int		eval_expr(oprtype *a);
int		expratom(oprtype *a);
int		exfunc(oprtype *a, boolean_t alias_target);
int		expritem(oprtype *a);
int		expr(oprtype *a, int m_type);
void		ex_tail(oprtype *opr);
int		extern_func(oprtype *a);
int		f_ascii(oprtype *a, opctype op);
int		f_char(oprtype *a, opctype op);
int		f_data(oprtype *a, opctype op);
int		f_extract(oprtype *a, opctype op);
int		f_find(oprtype *a, opctype op);
int		f_fnumber(oprtype *a, opctype op);
int		f_fnzbitfind(oprtype *a, opctype op);
int		f_fnzbitget(oprtype *a, opctype op);
int		f_fnzbitset(oprtype *a, opctype op);
int		f_fnzbitstr(oprtype *a, opctype op);
int		f_get(oprtype *a, opctype op);
int		f_get1(oprtype *a, opctype op);
int		f_incr(oprtype *a, opctype op);
int		f_justify(oprtype *a, opctype op);
int		f_length(oprtype *a, opctype op);
int		f_mint(oprtype *a, opctype op);
int		f_mint_mstr(oprtype *a, opctype op);
int		f_mstr(oprtype *a, opctype op);
int		f_name(oprtype *a, opctype op);
int		f_next(oprtype *a, opctype op);
int		f_one_mval(oprtype *a, opctype op);
int		f_order(oprtype *a, opctype op);
int		f_order1(oprtype *a, opctype op);
int		f_piece(oprtype *a, opctype op);
int		f_qlength(oprtype *a, opctype op);
int		f_qsubscript(oprtype *a, opctype op);
int		f_query (oprtype *a, opctype op);
int		f_reverse(oprtype *a, opctype op);
int		f_select(oprtype *a, opctype op);
int		f_stack(oprtype *a, opctype op);
int		f_text(oprtype *a, opctype op);
int		f_translate(oprtype *a, opctype op);
int		f_two_mstrs(oprtype *a, opctype op);
int		f_two_mval(oprtype *a, opctype op);
int		f_view(oprtype *a, opctype op);
int		f_zahandle(oprtype *a, opctype op);
int		f_zatransform(oprtype *a, opctype op);
int		f_zcall(oprtype *a, opctype op);
int		f_zchar(oprtype *a, opctype op);
int		f_zcollate(oprtype *a, opctype op);
int		f_zconvert(oprtype *a, opctype op);
int		f_zdate(oprtype *a, opctype op);
int		f_zdebug(oprtype *a, opctype op);
int		f_zechar(oprtype *a, opctype op);
int		f_zgetsyi(oprtype *a, opctype op);
int		f_zjobexam(oprtype *a, opctype op);
int		f_zparse(oprtype *a, opctype op);
int		f_zpeek(oprtype *a, opctype op);
int		f_zprevious(oprtype *a, opctype op);
int		f_zqgblmod(oprtype *a, opctype op);
int		f_zrupdate(oprtype *a, opctype op);
int		f_zsearch(oprtype *a, opctype op);
int		f_zsigproc(oprtype *a, opctype op);
int		f_zsocket(oprtype *a, opctype op);
int		f_zsqlexpr (oprtype *a, opctype op);
int		f_zsqlfield (oprtype *a, opctype op);
int		f_zsubstr(oprtype *a, opctype op);
int		f_ztrigger(oprtype *a, opctype op);
int		f_ztrnlnm(oprtype *a, opctype op);
int		f_zwidth(oprtype *a, opctype op);
int		f_zwrite(oprtype *a, opctype op);
mlabel		*get_mladdr(mident *c);
mvar		*get_mvaddr(mident *c);
int		glvn(oprtype *a);
int		gvn(void);
void		ind_code(mstr *obj);
int		indirection(oprtype *a);
void		ins_triple(triple *x);
void		int_label(void);
int		jobparameters (oprtype *c);
boolean_t	line(uint4 *lnc);
int		linetail(void);
int		lkglvn(boolean_t gblvn);
int	lref(oprtype *label, oprtype *offset, boolean_t no_lab_ok, mint commarg_code, boolean_t commarg_ok, boolean_t *got_some);
int		lvn(oprtype *a,opctype index_op,triple *parent);
void		make_commarg(oprtype *x, mint ind);
oprtype		make_gvsubsc(mval *v);
triple		*maketriple(opctype op);
int		name_glvn(boolean_t gblvn, oprtype *a);
triple		*newtriple(opctype op);
int		nref(void);
void		obj_code(uint4 src_lines, void *checksum_ctx);
int		one_job_param(char **parptr);
int		parse_until_rparen_or_space(void);
oprtype		put_ocnt(void);
oprtype		put_tsiz(void);
oprtype		put_cdlt(mstr *x);
oprtype		put_cdidx(mstr *x);
oprtype		put_ilit(mint x);
oprtype		put_indr(oprtype *x);
oprtype		put_lit(mval *x);
oprtype		put_lit_s(mval *x, triple *dst);
oprtype		put_mfun(mident *l);
oprtype		put_mlab(mident *l);
oprtype		put_mnxl(void);
oprtype		put_mvar(mident *x);
oprtype		put_str(char *pt, mstr_len_t n);
oprtype		put_tjmp(triple *x);
oprtype		put_tnxt(triple *x);
oprtype		put_tref(triple *x);
boolean_t	resolve_optimize(triple *curtrip);
int		resolve_ref(int errknt);
void		resolve_tref(triple *, oprtype *);
triple		*setcurtchain(triple *x);
void		unary_tail(oprtype *opr);
/* VMS uses same code generator as USHBIN so treat as USHBIN for these compiler routines */
#		if defined(USHBIN_SUPPORTED) || defined(VMS)
void		shrink_trips(void);
boolean_t	litref_triple_oprcheck(oprtype *operand);
#		else
void		shrink_jmps(void);
#		endif
void		start_fetches(opctype op);
void		start_for_fetches(void);
void		tnxtarg(oprtype *a);
void		tripinit(void);
boolean_t	unuse_literal(mval *x);
void		walktree(mvar *n,void (*f)(),char *arg);
void		wrtcatopt(triple *r, triple ***lpx, triple **lptop);
int		zlcompile(unsigned char len, unsigned char *addr);		/***type int added***/

#endif /* COMPILER_H_INCLUDED */
