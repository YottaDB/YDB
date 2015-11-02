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
#ifndef COMPILER_H_INCLUDED
#define COMPILER_H_INCLUDED

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
	mval			v;
} mliteral;

typedef struct	triplesize
{
	struct tripletype	*ct;
	int4			size;
} tripsize;

typedef struct	oprtypestruct
{
	char			oprclass;
	union
	{
		struct	oprtypestruct	*indr;
		struct	tripletype	*tref;
		struct	triplesize	*tsize;
		mlabel			*lab;
		mline			*mlin;
		mliteral		*mlit;
		mstr			*cdlt;
		mvar			*vref;
		int4			temp;
		int4			ilit;
		int4			offset;
		unsigned char		vreg;
	} oprval;
} oprtype;

/* Values for oprclass */
#define NOCLASS		0
#define TVAR_REF	1
#define TVAL_REF	2
#define TINT_REF	3
#define TVAD_REF	4
#define TCAD_REF	5
#define VALUED_REF_TYPES 6
#define VREG_REF	6
#define MLIT_REF	7
#define MVAR_REF	8
#define TRIP_REF	9
#define TNXT_REF	10
#define TJMP_REF	11
#define INDR_REF	12
#define MLAB_REF	13
#define ILIT_REF	14
#define CDLT_REF	15
#define TEMP_REF	16
#define MFUN_REF	17
#define MNXL_REF	18	/* refer to internalentry of child line */
#define TSIZ_REF	19	/* ilit refering to size of given triple codegen */
#define OCNT_REF	20	/* Offset from Call to Next Triple */


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
	unsigned short		octype;
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

typedef struct
{
	char			name[20];
	opctype			bo_type;
	char			uo_type;
	unsigned short		opr_type;
} toktabtype;

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
#define	NO_FORMALLIST	(-1)

/* Some errors should not cause stx_error to issue an rts_error. These are the errors related to
 *	a) Invalid Intrinsic Commands
 *	b) Invalid Intrinsic Function Names
 *	c) Invalid Intrinsic Special Variables
 *	d) Invalid Deviceparameters for IO commands
 * These should cause an error at runtime if and only if that codepath is reached.
 * PostConditionals can cause this path to be avoided in which case we do not want to issue an error at compile time.
 * Therefore issue only a warning at compile-time and proceed with compilation as if this codepath will not be reached at runtime.
 */
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
	if (!TREF(trigger_compile))					\
		parse_warn = TRUE;					\
	assert(IS_STX_WARN(errcode));					\
	stx_error(errcode);						\
	if (TREF(trigger_compile))					\
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

/* Macro to compute running checksum of a routine one line at a time. */
#define RTN_SRC_CHKSUM(srcptr, srclen, chksum)								\
{													\
	char	*chkcalc, *cptr;									\
	uint4	srcint;											\
	for (chkcalc = srcptr, cptr = srcptr + srclen; chkcalc < cptr; )				\
	{												\
		srcint = 0;										\
		if (INTCAST(cptr - chkcalc) < SIZEOF(uint4))						\
		{											\
			memcpy(&srcint, chkcalc, cptr - chkcalc);					\
			chkcalc = cptr;		/* Stops loop after this iteration is complete */	\
		} else											\
		{											\
			GET_ULONG(srcint, chkcalc);							\
			chkcalc += SIZEOF(uint4);							\
		}											\
		chksum ^= srcint;									\
		chksum >>= 1;										\
	}												\
}

#define ENCOUNTERED_SIDE_EFFECT										\
{	/* Needs #include "show_source_line" and #include "fullbool.h" */				\
	char		source_line_buff[MAX_SRCLINE + SIZEOF(ARROW)];					\
													\
	if (TREF(shift_side_effects))									\
	{												\
		TREF(saw_side_effect) = TRUE;								\
		if (!run_time && (FULL_BOOL_WARN == TREF(gtm_fullbool)))				\
		{	/* warnings requested by by gtm_fullbool and enabled by eval_expr */		\
			show_source_line(source_line_buff, SIZEOF(source_line_buff), TRUE);		\
			dec_err(VARLSTCNT(1) ERR_BOOLSIDEFFECT);					\
		}											\
	}												\
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
#define BLOWN_FOR TRUE	/* all levels */

/* Marco to decrement or clear the for_stack, clear the for_temp array
 * and generate code to release run-time malloc'd mvals anchored in the for_saved_indx array
 * The corresponding FOR_PUSH macro is in m_for.c but this one is used in stx_error.c for error cases
 */
#define	FOR_POP(ALL)												\
{														\
	unsigned int	For_stack_level;									\
	boolean_t	Seen_indx;										\
														\
	assert(TREF(for_stack_ptr) >= (oprtype **)TADR(for_stack));						\
	assert(TREF(for_stack_ptr) <= (oprtype **)TADR(for_stack) + MAX_FOR_STACK);				\
	if (TREF(for_stack_ptr) > (oprtype **)TADR(for_stack))							\
		--(TREF(for_stack_ptr));										\
	if (ALL)												\
	{													\
		while (TREF(for_stack_ptr) > (oprtype **)TADR(for_stack))					\
			(TREF(for_stack_ptr))--;								\
		*(TREF(for_stack_ptr)) = NULL;									\
	}													\
	if (TREF(for_stack_ptr) == (oprtype **)TADR(for_stack))							\
	{													\
		for (Seen_indx = FALSE, For_stack_level = MAX_FOR_STACK; --For_stack_level; )			\
		{												\
			if (!Seen_indx && (TRUE_WITH_INDX == TAREF1(for_temps, For_stack_level)))		\
			{											\
				(void)newtriple(OC_FORFREEINDX);						\
				Seen_indx = TRUE;								\
			}											\
			TAREF1(for_temps, For_stack_level) = FALSE;						\
		}												\
	} else													\
		assert(TREF(for_stack_ptr) > (oprtype **)TADR(for_stack));					\
}

/* value used to make for_temps entries a little more than boolean */
#define TRUE_WITH_INDX 2

int		actuallist(oprtype *opr);
int		bool_expr(boolean_t op, oprtype *addr);
void		bx_boolop(triple *t, boolean_t jmp_type_one, boolean_t jmp_to_next, boolean_t sense, oprtype *addr);
void		bx_relop(triple *t, opctype cmp, opctype tst, oprtype *addr);
void		bx_tail(triple *t, boolean_t sense, oprtype *addr);
void		chktchain(triple *head);
void		code_gen(void);
void		coerce(oprtype *a, unsigned short new_type);
int		comp_fini(int status, mstr *obj, opctype retcode, oprtype *retopr, mstr_len_t src_len);
void		comp_init(mstr *src);
void		comp_indr(mstr *obj);
boolean_t	compiler_startup(void);
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
int		f_zcall(oprtype *a, opctype op);
int		f_zchar(oprtype *a, opctype op);
int		f_zconvert(oprtype *a, opctype op);
int		f_zdate(oprtype *a, opctype op);
int		f_zdebug(oprtype *a, opctype op);
int		f_zechar(oprtype *a, opctype op);
int		f_zgetsyi(oprtype *a, opctype op);
int		f_zjobexam(oprtype *a, opctype op);
int		f_zparse(oprtype *a, opctype op);
int		f_zprevious(oprtype *a, opctype op);
int		f_zqgblmod(oprtype *a, opctype op);
int		f_zsearch(oprtype *a, opctype op);
int		f_zsigproc(oprtype *a, opctype op);
int		f_zsqlexpr (oprtype *a, opctype op);
int		f_zsqlfield (oprtype *a, opctype op);
int		f_zsubstr(oprtype *a, opctype op);
int		f_ztrigger(oprtype *a, opctype op);
int		f_ztrnlnm(oprtype *a, opctype op);
int		f_zwidth(oprtype *a, opctype op);
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
void		obj_code(uint4 src_lines, uint4 checksum);
int		one_job_param(char **parptr);
int		parse_until_rparen_or_space(void);
oprtype		put_ocnt(void);
oprtype		put_tsiz(void);
oprtype		put_cdlt(mstr *x);
oprtype		put_ilit(mint x);
oprtype		put_indr(oprtype *x);
oprtype		put_lit(mval *x);
oprtype		put_mfun(mident *l);
oprtype		put_mlab(mident *l);
oprtype		put_mnxl(void);
oprtype		put_mvar(mident *x);
oprtype		put_str(char *pt, mstr_len_t n);
oprtype		put_tjmp(triple *x);
oprtype		put_tnxt(triple *x);
oprtype		put_tref(triple *x);
int		resolve_ref(int errknt);
void		resolve_tref(triple *, oprtype *);
triple		*setcurtchain(triple *x);
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
void		walktree(mvar *n,void (*f)(),char *arg);
void		wrtcatopt(triple *r, triple ***lpx, triple **lptop);
int		zlcompile(unsigned char len, unsigned char *addr);		/***type int added***/

#endif /* COMPILER_H_INCLUDED */
