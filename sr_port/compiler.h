/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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
	short int		line_number;	/* ...operation on this line */
	short int		table;		/* put in table or not */
} mline;

typedef struct	mlabstruct
{
	struct	mlabstruct	*lson,
				*rson;
	mline			*ml;
	mident			mvname;
	short			formalcnt;
	bool			gbl;
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
	unsigned short		line;
	unsigned short		column;
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

#ifdef DEBUG
#  define COMPDBG(x)	if (gtmDebugLevel & GDL_DebugCompiler) {x}
#else
#  define COMPDBG(x)
#endif

/* Some errors should not cause stx_error to issue an rts_error. These are the errors related to
 * 	a) Invalid Intrinsic Special Variables
 *	b) Invalid Intrinsic Function Names
 *	c) Invalid Deviceparameters for IO commands
 * These should cause an error at runtime if and only if that codepath is reached.
 * PostConditionals can cause this path to be avoided in which case we do not want to issue an error at compile time.
 * Therefore issue only a warning at compile-time and proceed with compilation as if this codepath will not be reached at runtime.
 */
error_def(ERR_FNOTONSYS);
error_def(ERR_INVFCN);
error_def(ERR_INVSVN);
error_def(ERR_SVNONEW);
error_def(ERR_SVNOSET);
error_def(ERR_DEVPARUNK);
error_def(ERR_DEVPARINAP);
error_def(ERR_DEVPARVALREQ);

#define	IS_STX_WARN(errcode)											\
	((ERR_INVFCN == errcode) || (ERR_FNOTONSYS == errcode) || (ERR_INVSVN == errcode)			\
		|| (ERR_SVNONEW == errcode) || (ERR_SVNOSET == errcode)						\
		|| (ERR_DEVPARUNK == errcode) || (ERR_DEVPARINAP == errcode) || (ERR_DEVPARVALREQ == errcode))

/* This macro does an "stx_error" of the input errcode but before that it asserts that the input errcode is one
 * of the known error codes that are to be handled as a compile-time warning (instead of an error). It also set
 * the variable "parse_warn" to TRUE which is relied upon by the functions that invoke this macro.
 */
#define	STX_ERROR_WARN(errcode)						\
{									\
	parse_warn = TRUE;						\
	assert(IS_STX_WARN(errcode));					\
	stx_error(errcode);						\
}

#define MAX_SRCLINE	8192	/* maximum length of a program source or indirection line - increased for Iselin */

#define EXPR_FAIL	0	/* expression had syntax error */
#define EXPR_GOOD	1	/* expression ok, no indirection at root */
#define EXPR_INDR	2	/* expression ok, indirection at root */
#define EXPR_SHFT	4	/* expression ok, involved shifted GV references */

#define MAX_FOR_STACK	32
#define MAX_FORARGS	127

#define CHARMAXARGS	256

#define	NO_FORMALLIST	(-1)
#define	MAX_ACTUALS	32

int	parse_until_rparen_or_space(void);

triple *maketriple(opctype op);
triple *newtriple(opctype op);
void  ins_triple(triple *x);
triple *setcurtchain(triple *x);
int comp_fini(bool status, mstr *obj, opctype retcode, oprtype *retopr, mstr_len_t src_len);
void comp_init(mstr *src);
void comp_indr(mstr *obj);
bool compiler_startup(void);

oprtype put_ocnt(void);
oprtype put_tsiz(void);
oprtype put_cdlt(mstr *x);
oprtype put_ilit(mint x);
oprtype put_indr(oprtype *x);
oprtype put_lit(mval *x);
oprtype put_mfun(mident *l);
oprtype put_mlab(mident *l);
oprtype put_mnxl(void);
oprtype put_mvar(mident *x);
oprtype put_str(char *pt, mstr_len_t n);
oprtype put_tjmp(triple *x);
oprtype put_tnxt(triple *x);
oprtype put_tref(triple *x);

void	chktchain(triple *head);

int bool_expr(bool op, oprtype *addr);
int eval_expr(oprtype *a);
int expratom(oprtype *a);
int expritem(oprtype *a);
int expr(oprtype *a);
int intexpr(oprtype *a);
int numexpr(oprtype *a);
int strexpr(oprtype *a);

int indirection(oprtype *a);

void coerce(oprtype *a, unsigned short new_type);
void ex_tail(oprtype *opr);

void bx_boolop(triple *t, bool jmp_type_one, bool jmp_to_next, bool sense, oprtype *addr);
void bx_relop(triple *t, opctype cmp, opctype tst, oprtype *addr);
void bx_tail(triple *t, bool sense, oprtype *addr);

void tnxtarg(oprtype *a);

void make_commarg(oprtype *x, mint ind);
int lvn(oprtype *a,opctype index_op,triple *parent);
int gvn(void);

void ind_code(mstr *obj);
int resolve_ref(int errknt);
void resolve_tref(triple *, oprtype *);
void start_fetches(opctype op);

int actuallist(oprtype *opr);

int exfunc(oprtype *a);
int extern_func(oprtype *a);

int glvn(oprtype *a);

int linetail(void);
int nref(void);
int for_push(void);
void int_label(void);
int line(uint4 *lnc);
int lkglvn(bool gblvn);
int lref(oprtype *label, oprtype *offset, bool no_lab_ok, mint commarg_code, bool commarg_ok,
	bool *got_some);

int name_glvn(bool gblvn, oprtype *a);		/***type int added***/
oprtype for_end_of_scope(int depth);
oprtype make_gvsubsc(mval *v);
void for_declare_addr(oprtype x);
triple *entryref(opctype op1, opctype op2, mint commargcode, boolean_t can_commarg, boolean_t labref);

void  start_for_fetches(void);
int zlcompile(unsigned char len, unsigned char *addr);		/***type int added***/
mlabel *get_mladdr(mident *c);
mvar *get_mvaddr(mident *c);

void code_gen(void);

void obj_code(uint4 src_lines, uint4 checksum);

void for_pop(void);

void walktree(mvar *n,void (*f)(),char *arg);

/* VMS uses same code generator as USHBIN so treat as USHBIN for these compiler routines */
#if defined(USHBIN_SUPPORTED) || defined(VMS)
void shrink_trips(void);
boolean_t litref_triple_oprcheck(oprtype *operand);
#else
void shrink_jmps(void);
#endif

void tripinit(void);
void wrtcatopt(triple *r, triple ***lpx, triple **lptop);

int jobparameters (oprtype *c);
int one_job_param(char **parptr);

int f_ascii(oprtype *a, opctype op);
int f_char(oprtype *a, opctype op);
int f_data(oprtype *a, opctype op);
int f_extract(oprtype *a, opctype op);
int f_find(oprtype *a, opctype op);
int f_fnumber(oprtype *a, opctype op);
int f_fnzbitfind(oprtype *a, opctype op);
int f_fnzbitget(oprtype *a, opctype op);
int f_fnzbitset(oprtype *a, opctype op);
int f_fnzbitstr(oprtype *a, opctype op);
int f_get(oprtype *a, opctype op);
int f_incr(oprtype *a, opctype op);
int f_justify(oprtype *a, opctype op);
int f_length(oprtype *a, opctype op);
int f_mint(oprtype *a, opctype op);
int f_mint_mstr(oprtype *a, opctype op);
int f_mstr(oprtype *a, opctype op);
int f_name(oprtype *a, opctype op);
int f_next(oprtype *a, opctype op);
int f_one_mval(oprtype *a, opctype op);
int f_order(oprtype *a, opctype op);
int f_order1(oprtype *a, opctype op);
int f_piece(oprtype *a, opctype op);
int f_qlength(oprtype *a, opctype op);
int f_qsubscript(oprtype *a, opctype op);
int f_query (oprtype *a, opctype op);
int f_reverse(oprtype *a, opctype op);
int f_select(oprtype *a, opctype op);
int f_stack(oprtype *a, opctype op);
int f_text(oprtype *a, opctype op);
int f_translate(oprtype *a, opctype op);
int f_two_mstrs(oprtype *a, opctype op);
int f_two_mval(oprtype *a, opctype op);
int f_view(oprtype *a, opctype op);
int f_zcall(oprtype *a, opctype op);
int f_zchar(oprtype *a, opctype op);
int f_zdate(oprtype *a, opctype op);
int f_zechar(oprtype *a, opctype op);
int f_zgetsyi(oprtype *a, opctype op);
int f_zjobexam(oprtype *a, opctype op);
int f_zparse(oprtype *a, opctype op);
int f_zprevious(oprtype *a, opctype op);
int f_zqgblmod(oprtype *a, opctype op);
int f_zsearch(oprtype *a, opctype op);
int f_zsigproc(oprtype *a, opctype op);
int f_zsqlexpr (oprtype *a, opctype op);
int f_zsqlfield (oprtype *a, opctype op);
int f_ztrnlnm(oprtype *a, opctype op);
int f_zconvert(oprtype *a, opctype op);
int f_zwidth(oprtype *a, opctype op);
int f_zsubstr(oprtype *a, opctype op);

#endif /* COMPILER_H_INCLUDED */
