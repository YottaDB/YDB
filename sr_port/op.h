/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef OP_INCLUDED
#define OP_INCLUDED

#include "op_tstart.h"	/* for "op_tstart" prototype */
#include <rtnhdr.h>	/* Avoid changing a few hundred op_* and other modules to put this first */
#include "hashtab_int4.h"
#include "hashtab.h"
#include "lv_val.h"

#ifdef VMS
/* Define a TWO-argument VMS_ONLY macro (first argument is empty string but is needed because of the VMS-only , that follows) */
#define	UNIX1_VMS2(X,Y)	X, Y
#else
#define	UNIX1_VMS2(X,Y)	X
#endif

/* Definitions for mystery numbers in op_fnztranslate and f_translate */
#define NO_VALUE -1
#define DELETE_VALUE -2

/* A state machine that tracks whether a `gvname2naked` optimization is legal at runtime.
 * In particular, a transition from the ININTERRUPT to LEGAL state is always wrong; we have to see at least one GVNAME to get back
 * to a known state before doing an optimization. */
#define NAMENAKED_LEGAL 0             /* Naked reference optimization OK */
#define NAMENAKED_ININTERRUPT 1       /* Can't do a naked reference optimization until we leave the interrupt AND see a GVNAME */
#define NAMENAKED_UNKNOWNREFERENCE 2  /* The next GVNAME will make a naked reference optimization legal */
GBLREF int gv_namenaked_state;

void	op_add(mval *u, mval *v, mval *s);
void	add_mvals(mval *u, mval *v, int subtraction, mval *result);	/* function defined in op_add.c */
void	op_bindparm(UNIX_ONLY_COMMA(int frmc) int frmp_arg, ...);
void	op_break(void);
void	op_cat(UNIX_ONLY_COMMA(int srcargs) mval *dst, ...);
void	op_close(mval *v, mval *p);
void	op_commarg(mval *v, unsigned char argcode);
int	op_currhd(void);
void	op_cvtparm(int iocode, mval *src, mval *dst);
int	op_decrlock(mval *timeout);
void	op_div(mval *u, mval *v, mval *q);
void	op_dmode(void);
void	op_dt_false(void);
int	op_dt_get(void);
void	op_dt_store(int truth_value);
void	op_dt_true(void);
void	op_exfunret(mval *retval);
void	op_exfunretals(mval *retval);
void	op_exp(mval *u, mval *v, mval *p);
/*	op_fetch currently does not exist. Instead gtm_fetch is the name (should be renamed to op_fetch to keep naming scheme) */
void	gtm_fetch(UNIX_ONLY_COMMA(unsigned int cnt_arg) unsigned int indxarg, ...);
int	op_fetchintrrpt();
void	op_fgnlookup(void);
void	op_fnascii(int4 num, mval *in, mval *out);
void	op_fnchar(UNIX_ONLY_COMMA(int cnt) mval *dst, ...);
void	op_fnextract(int last, int first, mval *src, mval *dest);
void	op_fnfgncal(uint4 n_mvals, mval *dst, mval *package, mval *extref, uint4 mask, int4 argcnt, ...);
int4	op_fnfind(mval *src, mval *del, mint first, mval *dst);
void	op_fnfnumber(mval *src, mval *fmt, boolean_t use_fract, int fract, mval *dst);
void	op_fnget1(mval *src, mval *dst);
void	op_fnget2(mval *src, mval *def, mval *dst);
void	op_fngetdvi(mval *device, mval *keyword, mval *ret);
void	op_fngetjpi(mint jpid, mval *kwd, mval *ret);
void	op_fngetlki(mval *lkid_mval, mval *keyword, mval *ret);
void	op_fngetsyi(mval *keyword, mval *node, mval *ret);
void	op_fngvget(mval *dst);
void	op_fngvget1(mval *dst);
void	op_fnj2(mval *src, int len, mval *dst);
void	op_fnj3(mval *src, int width, int fract, mval *dst);
void	op_fnlength(mval *a1, mval *a0);
void	op_fnlvname(mval *src, boolean_t return_undef_alias, symval *sym, mval *dst);
void	op_fnlvnameo2(mval *src, mval *dst, mval *direct);
void	op_fnlvprvname(mval *src, mval *dst);
void	op_fnname(UNIX_ONLY_COMMA(int sub_count) mval *finaldst, ...);
void	op_fnp1(mval *src, int del, int trgpcidx, UNIX1_VMS2(mval *dst, boolean_t srcisliteral));
void	op_fnpiece(mval *src, mval *del, int first, int last, UNIX1_VMS2(mval *dst, boolean_t srcisliteral));
void	op_fnpopulation(mval *arg1, mval *arg2, mval *dst);
void	op_fnqlength(mval *name, mval *subscripts);
void	op_fnqsubscript(mval *name, int seq, mval *subscript);
void	op_fnquery(UNIX_ONLY_COMMA(int sbscnt) mval *dst, ...);
void	op_fnquery_va(int sbscnt, mval *dst, va_list var);
void	op_fnreversequery(UNIX_ONLY_COMMA(int sbscnt) mval *dst, ...);
void	op_fnreversequery_va(int sbscnt, mval *dst, va_list var);
void	op_fnrandom(int4 interval, mval *ret);
void	op_fnreverse(mval *src, mval *dst);
void	op_fnstack1(int level, mval *result);
void	op_fnstack2(int level, mval *info, mval *result);
void	op_fntext(mval *label, int int_exp, mval *rtn, mval *ret);
void	op_fntranslate(mval *src, mval *in_str, mval *out_str, mval *dst);
void	op_fnview(UNIX_ONLY_COMMA(int numarg) mval *dst, ...);
void	op_fnzascii(int4 num, mval *in, mval *out);
void	op_fnzauditlog(mval *src, mval *dst);
void	op_fnzbitand(mval *dst, mval *bitstr1, mval *bitstr2);
void	op_fnzbitcoun(mval *dst, mval *bitstr);
void	op_fnzbitfind(mval *dst, mval *bitstr, int truthval, int pos);
void	op_fnzbitget(mval *dst, mval *bitstr, int pos);
void	op_fnzbitlen(mval *dst, mval *bitstr);
void	op_fnzbitnot(mval *dst, mval *bitstr);
void	op_fnzbitor(mval *dst, mval *bitstr1, mval *bitstr2);
void	op_fnzbitset(mval *dst, mval *bitstr, int pos, int truthval);
void	op_fnzbitstr(mval *bitstr, int size, int truthval);
void	op_fnzbitxor(mval *dst, mval *bitstr1, mval *bitstr2);
void	op_fnzcall(void);	/* stub only */
void	op_fnzchar(UNIX_ONLY_COMMA(int cnt) mval *dst, ...);
void	op_fnzconvert2(mval *str, mval *kase, mval *dst);
void	op_fnzconvert3(mval *str, mval *from_chset, mval *to_chset, mval *dst);
void	op_fnzcollate(mval *src, int col, int reverse, mval *dst);
void	op_fnzdate(mval *src, mval *fmt, mval *mo_str, mval *day_str, mval *dst);
void	op_fnzdebug(mval *cmd, mval *dst);
void	op_fnzechar(UNIX_ONLY_COMMA(int cnt) mval *dst, ...);
void	op_fnzextract(int last, int first, mval *src, mval *dest);
void	op_fnzfile(mval *name, mval *key, mval *ret);
int4	op_fnzfind(mval *src, mval *del, mint first, mval *dst);
void	op_fnzyhash(mval *string, int salt, mval *ret);
void	op_fnzysuffix(mval *string, mval *ret);
void	op_fnzj2(mval *src, int len, mval *dst);
void	op_fnzjobexam(mval *prelimSpec,  mval *fmt, mval *finalSpec);
void	op_fnzlength(mval *a1, mval *a0);
void	op_fnzlkid(mint boolex, mval *retval);
void	op_fnzm(mint x, mval *v);
void	op_fnzp1(mval *src, int del, int trgpcidx, UNIX1_VMS2(mval *dst, boolean_t srcisliteral));
void	op_fnzparse(mval *file, mval *field, mval *def1, mval *def2, mval *type, mval *ret);
void	op_fnzpeek(mval *baseaddr, int offset, int len, mval *format, mval *ret);
void	op_fnzpid(mint boolexpr, mval *ret);
void	op_fnzpiece(mval *src, mval *del, int first, int last, UNIX1_VMS2(mval *dst, boolean_t srcisliteral));
void	op_fnzpopulation(mval *arg1, mval *arg2, mval *dst);
void	op_fnzpriv(mval *prv, mval *ret);
void	op_fnzqgblmod(mval *v);
void	op_fnzreverse(mval *src, mval *dst);
int	op_fnzsearch(mval *file, mint indx, mint mfunc, mval *ret);		/***type int added***/
void	op_fnzsetprv(mval *prv, mval *ret);
void	op_fnzsigproc(int processid, mval *sigval, mval *retcode);
void	op_fnzsocket(UNIX_ONLY_COMMA(int numarg) mval *dst, ...);
void	op_fnzsqlexpr(mval *value, mval *target);
void	op_fnzsqlfield(int findex, mval *target);
void	op_fnzsubstr(mval *src, int start, int bytelen, mval *dest);
void	op_fnzsyslog(mval *src, mval *dst);
void	op_fnztranslate(mval *src, mval *in_str , mval *out_str, mval *dst);
void	op_fnztranslate_fast(mval *src, mval *m_xlate, mval *dst);
void	op_fntranslate_fast(mval *src, mval *rplc, mval *xlate, mval *m_xlate_hash, mval *dst);
void	op_fntranslate_common(mval *src, mval *dst, mval *rplc, int4 *xlate, hash_table_int4 *xlate_hash);
void	op_fnztranslate_common(mval *src, mval *dst, int *xlate);
void	create_byte_xlate_table(mval *srch, mval *rplc, int4 *xlate);
hash_table_int4	*create_utf8_xlate_table(mval *srch, mval *rplc, mstr *m_xlate);
void	op_fnztrigger(mval *func, mval *arg1, mval *arg2, mval *dst);
void	op_fnztrnlnm(mval *name, mval *table, int4 ind, mval *mode, mval *case_blind, mval *item, mval *ret);
void	op_fnztrnlnm(mval *name, mval *table, int4 ind, mval *mode, mval *case_blind, mval *item, mval *ret);
void	op_fnzwidth(mval *str, mval *dst);
void	op_fnzwrite(boolean_t direction, mval *str, mval *dst);
void	op_fnzyissqlnull(mval *src, mval *dst);
void	op_fnzatransform(mval *src, int col, int reverse, int forcenum, mval *dst);
int	op_forchk1();
int	op_forintrrpt();
int	op_forloop();
void	op_gvdata(mval *v);
void	op_gvextnam(UNIX_ONLY_COMMA(int4 count) mval *val1, ...);
void	op_gvextnam_fast(UNIX_ONLY_COMMA(int4 count) int hash_code, mval *val1, ...);
boolean_t op_gvget(mval *v);
void	op_gvincr(mval *increment, mval *result);
void	op_gvkill(void);
void	op_gvnaked(UNIX_ONLY_COMMA(int count_arg) mval *val_arg, ...);
void	op_gvnaked_fast(UNIX_ONLY_COMMA(int count_arg) int hash_code, mval *val_arg, ...);
void	op_gvnaked_common(int count, int hash_code_dummy, mval *val_arg, va_list var);
void	op_gvname(UNIX_ONLY_COMMA(int count_arg) mval *val_arg, ...);
void	op_gvname_fast(UNIX_ONLY_COMMA(int count_arg) int hash_code, mval *val_arg, ...);
void	op_gvnamenaked(int count_arg, int hash_code, int subs, mval *opcodes, mval *val_arg, ...);
void	op_gvnext(mval *v);
void	op_gvo2(mval *dst, mval *direct);
void	op_gvorder(mval *v);
void	op_gvput(mval *var);
void	op_gvquery(mval *v);
void	op_gvreversequery(mval *v);
boolean_t op_gvqueryget(mval *key, mval *val);
void	op_gvq2(mval *dst, mval *direct);
void	op_gvrectarg(mval *v); /* Restore gv_currkey */
void	op_gvsavtarg(mval *v); /* Save gv_currkey*/
void	op_gvzwithdraw(void);
void	op_gvzwrite(UNIX_ONLY_COMMA(int4 count) int4 pat, ...);
void	op_hang(mval *num);
void	op_hardret(void);
void	op_idiv(mval *u, mval *v, mval *q);
mval	*op_igetdst(void);
void	op_igetsrc(mval *v);
int	op_incrlock(mval *timeout);
int	op_incrlock_common(uint8 timeout);
void	op_inddevparms(mval *devpsrc, int4 ok_iop_parms, mval *devpiopl);
void	op_indfnname(mval *dst, mval *target, mval *value);
void	op_indfnname2(mval *finaldst, mval *depthval, mval *prechomp);
void	op_indfun(mval *v, mint argcode, mval *dst);
void	op_indget1(uint4 indx, mval *dst);					/* Used by [SET] */
void	op_indget2(mval *dst, uint4 indx);
void	op_indglvn(mval *v, mval *dst);
void	op_indincr(mval *dst, mval *increment, mval *target);
void	op_indlvadr(mval *target);
void	op_indlvarg(mval *v, mval *dst);
void	op_indlvnamadr(mval *target);
void	op_indmerge(mval *glvn_mv, mval *arg1_or_arg2);
void	op_indmerge2(uint4 indx);
void	op_indname(mval *dst, mval *target, mval *subs);
void	op_indo2(mval *dst, uint4 indx, mval *value);
void	op_indpat(mval *v, mval *dst);
void	op_indq2(mval *dst, uint4 indx, mval *value);
void	op_indrzshow(mval *s1, mval *s2, mint level);
void	op_indset(mval *target, mval *value);
void	op_indtext(mval *lab, mint offset, mval *rtn, mval *dst);
void	op_iocontrol(UNIX_ONLY_COMMA(int4 n) mval *vparg, ...);
void	op_iretmval(mval *v, mval *dst);
int	op_job(int4 argcnt, ...);
void	op_killaliasall(void);
void	op_killall(void);
void	op_killall(void);
int	op_linefetch();
int	op_linestart();
void	op_litc(mval *dst, mval *src);
void	op_lkinit(void);
void	op_lkname(UNIX_ONLY_COMMA(int subcnt) mval *extgbl1, ...);
int	op_lock(mval *timeout);
int	op_lock2(mval *timeout, unsigned char laflag);
int	op_lock2_common(uint8 timeout, unsigned char laflag);
void	op_lvpatwrite(UNIX_ONLY_COMMA(int4 count) UINTPTR_T arg1, ...);
void	op_lvzwrite(UNIX_ONLY_COMMA(int4 count) long arg1, ...);
/*	op_merge     : prototype defined separately in op_merge.h */
/*	op_merge_arg : prototype defined separately in op_merge.h */
int	op_mprofforchk1();
int	op_mproflinefetch();
int	op_mproflinestart();
void	op_mul(mval *u, mval *v, mval *p);
void	op_newintrinsic(int intrtype);
void	op_newvar(uint4 arg1);
mval	*op_nullexp(void);
void	op_oldvar(void);
int	op_open(mval *device, mval *devparms, mval *timeout, mval *mspace);
int	op_open_dummy(mval *v, mval *p, mval *timeout, mval *mspace);
int	op_rdone(mval *v, mval *timeout);

#define	OP_READ_MORE_READ_TIME	100	/* msec */

int	op_read(mval *v, mval *timeout);
int	op_readfl(mval *v, int4 length, mval *timeout);
int	op_ret();
int	op_retarg();
void	op_rterror(int4 sig, boolean_t subrtn);
void	op_setextract(mval *src, mval *expr, int schar, int echar, mval *dst);
void	op_setp1(mval *src, int delim, mval *expr, int ind, mval *dst);
void	op_setpiece(mval *src, mval *del, mval *expr, int4 first, int4 last, mval *dst);
void	op_setzbrk(mval *rtn, mval *lab, int offset, mval *act, int cnt, boolean_t count_specified);
void	op_setzextract(mval *src, mval *expr, int schar, int echar, mval *dst);
void	op_setzp1(mval *src, int delim, mval *expr, int ind, mval *dst);
void	op_setzpiece(mval *src, mval *del, mval *expr, int4 first, int4 last, mval *dst);
void	op_sqlinddo(mstr *m_init_rtn);
int	op_startintrrpt();
void	op_stolitc(mval *val);
void	op_sub(mval *u, mval *v, mval *s);
void	op_sub(mval *u, mval *v, mval *s);
void	op_svget(int varnum, mval *v);
void	op_svput(int varnum, mval *v);
/*	op_tcommit : prototype defined separately in op_tcommit.h since it returns "enum cdb_sc" type. */
void	op_trestart(int newlevel);

/* Macro to be called by C Runtime code to invoke op_trollback. Sets implicit_trollback to TRUE. Note: The interface of
 * OP_TROLLBACK macro and op_trollback function needs to be maintained in parallel.
 */
#define OP_TROLLBACK(RB_LEVELS)													\
{																\
	GBLREF	boolean_t		implicit_trollback;									\
																\
	assert(!implicit_trollback); 												\
	implicit_trollback = TRUE;												\
	op_trollback(RB_LEVELS);/*BYPASSOK*/											\
	/* Should have been reset by op_trollback at the beginning of the function entry */					\
	assert(!implicit_trollback);												\
}

void	op_trollback(int rb_levels);/*BYPASSOK*/
void	op_unlock(void);
void	op_unwind(void);
void	op_use(mval *v, mval *p);
void	op_view(UNIX_ONLY_COMMA(int numarg) mval *keyword, ...);
void	op_write(mval *v);
void	op_wteol(int4 n);
void	op_wtff(void);
void	op_wtone(int c);
void	op_wttab(mint x);
void	op_xkill(UNIX_ONLY_COMMA(int n) mval *lvname_arg, ...);
void	op_xnew(UNIX_ONLY_COMMA(unsigned int argcnt_arg) mval *s_arg, ...);
int	op_zallocate(mval *timeout);
void	op_zattach(mval *);
int	op_zbfetch();
int	op_zbstart();
void	op_zcompile(mval *v, boolean_t ignore_dollar_zcompile);
void	op_zcont(void);
void    op_zcollate(mval *v, int c, mval *p);
void	op_zdealloc2(mval *timeout, UINTPTR_T auxown);
void	op_zdeallocate(mval *timeout);
void	op_zedit(mval *v, mval *p);
void	op_zg1(int4 level);
void	op_zgoto(mval *rtnname, mval *lblname, int offset, int level);
void    op_zrupdate(int argcnt, ...);
void	op_ztrigger(void);
void	op_zhalt(int4 is_zhalt, boolean_t retcode);
void	op_zhelp_xfr(mval *subject, mval *lib);
void	op_zhorolog(mval *s, boolean_t z);
void	op_zlink(mval *v, mval *quals);
void	op_zmess(unsigned int cnt, ...);
void	op_zprevious(mval *v);
void	op_zprint(mval *rtn, mval *start_label, int start_int_exp, mval *end_label, int end_int_exp);
void	op_zst_break(void);
int	op_zst_fet_over();
void	op_zst_over(void);
int	op_zst_st_over();
void	op_zstep(uint4 code, mval *action);
int	op_zstepfetch();
void	op_zstepret(void);
int	op_zstepstart();
int	op_zstzb_fet_over();
int	op_zstzb_st_over();
int	op_zstzbfetch();
int	op_zstzbstart();
void	op_zsystem(mval *v);
void	op_ztcommit(int4 n);
void	op_ztstart(void);
int	opp_ret();
int	opp_zst_over_ret();
int	opp_zst_over_retarg();
int	opp_zstepret();
int	opp_zstepretarg();
void	op_zut(mval *s);
void	op_zwritesvn(int svn);
void	op_zst_over_retarg_helper(void);
void	op_zstepretarg_helper(void);

void	op_equnul_retmval(mval *u, mval *ret);
void	op_nequnul_retmval(mval *u, mval *ret);
void	opp_equnul_retbool();				/* Prototype of opp_equnul_retbool.s */
int	op_equnul_retbool(mval *u);			/* Prototype of op_equnul_retbool.c */
void	opp_nequnul_retbool();				/* Prototype of opp_nequnul_retbool.s */
int	op_nequnul_retbool(mval *u);			/* Prototype of op_nequnul_retbool.c */
void	opp_equ_retbool();				/* Prototype of opp_equ_retbool.s */
int	op_equ_retbool(mval *u, mval *v);		/* Prototype of op_equ_retbool.c */
void	opp_nequ_retbool();				/* Prototype of opp_nequ_retbool.s */
int	op_nequ_retbool(mval *u, mval *v);		/* Prototype of op_nequ_retbool.c */
void	op_equ_retmval(mval *u, mval *v, mval *ret);
void	op_nequ_retmval(mval *u, mval *v, mval *ret);
void	opp_contain_retbool();				/* Prototype of opp_contain_retbool.s */
int	op_contain_retbool(mval *u, mval *v);		/* Prototype of op_contain_retbool.c */
void	opp_ncontain_retbool();				/* Prototype of opp_ncontain_retbool.s */
int	op_ncontain_retbool(mval *u, mval *v);		/* Prototype of op_ncontain_retbool.c */
void	op_contain_retmval(mval *u, mval *v, mval *ret);
void	op_ncontain_retmval(mval *u, mval *v, mval *ret);
void	opp_follow_retbool();				/* Prototype of opp_follow_retbool.s */
int	op_follow_retbool(mval *u, mval *v);		/* Prototype of op_follow_retbool.c */
void	opp_nfollow_retbool();				/* Prototype of opp_nfollow_retbool.s */
int	op_nfollow_retbool(mval *u, mval *v);		/* Prototype of op_nfollow_retbool.c */
void	op_follow_retmval(mval *u, mval *v, mval *ret);
void	op_nfollow_retmval(mval *u, mval *v, mval *ret);
void	opp_pattern_retbool();				/* Prototype of opp_pattern_retbool.s */
int	op_pattern_retbool(mval *u, mval *v);		/* Prototype of op_pattern_retbool.c */
void	opp_npattern_retbool();				/* Prototype of opp_npattern_retbool.s */
int	op_npattern_retbool(mval *u, mval *v);		/* Prototype of op_npattern_retbool.c */
void	op_pattern_retmval(mval *u, mval *v, mval *ret);
void	op_npattern_retmval(mval *u, mval *v, mval *ret);
void	opp_sortsafter_retbool();				/* Prototype of opp_sortsafter_retbool.s */
int	op_sortsafter_retbool(mval *u, mval *v);		/* Prototype of op_sortsafter_retbool.c */
void	opp_nsortsafter_retbool();				/* Prototype of opp_nsortsafter_retbool.s */
int	op_nsortsafter_retbool(mval *u, mval *v);		/* Prototype of op_nsortsafter_retbool.c */
void	op_sortsafter_retmval(mval *u, mval *v, mval *ret);
void	op_nsortsafter_retmval(mval *u, mval *v, mval *ret);
void	opp_gt_retbool();				/* Prototype of opp_gt_retbool.s */
int	op_gt_retbool(mval *u, mval *v);		/* Prototype of op_gt_retbool.c */
void	opp_ngt_retbool();				/* Prototype of opp_ngt_retbool.s */
int	op_ngt_retbool(mval *u, mval *v);		/* Prototype of op_ngt_retbool.c */
void	op_gt_retmval(mval *u, mval *v, mval *ret);
void	op_ngt_retmval(mval *u, mval *v, mval *ret);
void	opp_lt_retbool();				/* Prototype of opp_lt_retbool.s */
int	op_lt_retbool(mval *u, mval *v);		/* Prototype of op_lt_retbool.c */
void	opp_nlt_retbool();				/* Prototype of opp_nlt_retbool.s */
int	op_nlt_retbool(mval *u, mval *v);		/* Prototype of op_nlt_retbool.c */
void	op_lt_retmval(mval *u, mval *v, mval *ret);
void	op_nlt_retmval(mval *u, mval *v, mval *ret);
void	opp_bxrelop_equ();				/* Prototype of opp_bxrelop_equ.s */
void	opp_bxrelop_nequ();				/* Prototype of opp_bxrelop_nequ.s */
void	opp_bxrelop_contain();				/* Prototype of opp_bxrelop_contain.s */
void	opp_bxrelop_ncontain();				/* Prototype of opp_bxrelop_ncontain.s */
void	opp_bxrelop_sortsafter();			/* Prototype of opp_bxrelop_sortsafter.s */
void	opp_bxrelop_nsortsafter();			/* Prototype of opp_bxrelop_nsortsafter.s */
void	opp_bxrelop_pattern();				/* Prototype of opp_bxrelop_pattern.s */
void	opp_bxrelop_npattern();				/* Prototype of opp_bxrelop_npattern.s */
void	opp_bxrelop_follow();				/* Prototype of opp_bxrelop_follow.s */
void	opp_bxrelop_nfollow();				/* Prototype of opp_bxrelop_nfollow.s */
void	opp_bxrelop_gt();				/* Prototype of opp_bxrelop_gt.s */
void	opp_bxrelop_ngt();				/* Prototype of opp_bxrelop_ngt.s */
void	opp_bxrelop_lt();				/* Prototype of opp_bxrelop_lt.s */
void	opp_bxrelop_nlt();				/* Prototype of opp_bxrelop_nlt.s */
int	op_bxrelop_equ(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_nequ(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_contain(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_ncontain(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_sortsafter(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_nsortsafter(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_pattern(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_npattern(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_follow(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_nfollow(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_gt(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_ngt(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_lt(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
int	op_bxrelop_nlt(mval *lhs, mval *rhs, int this_bool_depth, uint4 combined_opcode);
void    op_fnzycompile(mval *string, mval *ret);
#endif
