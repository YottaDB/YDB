/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef OP_INCLUDED
#define OP_INCLUDED

boolean_t op_gvqueryget(mval *key, mval *val);
int	op_dt_get(void);
void	op_fnzdate(mval *src, mval *fmt, mval *mo_str, mval *day_str, mval *dst);
int	op_fnzsearch(mval *file, mint indx, mval *ret);		/***type int added***/
bool	op_gvget(mval *v);
int	op_incrlock(int timeout);
int	op_lock(int timeout);
int	op_open(mval *device, mval *devparms, int timeout, mval *mspace);
int	op_rdone(mval *v, int4 timeout);
int	op_readfl(mval *v, int4 length, int4 timeout);
int	op_read(mval *v, int4 timeout);
int	op_zallocate(int timeout);		/***type int added***/
void	op_fngvget1(mval *v);
int	op_zalloc2(int4 timeout, uint4 auxown);
int	op_zallocate(int timeout);
void	op_unwind(void);
void	op_break(void);
void	op_lvpatwrite(UNIX_ONLY_COMMA(int4 count) int4 arg1, ...);
void	op_killall(void);
void	op_gvzwithdraw(void);
void	op_gvkill(void);
void	op_cat(UNIX_ONLY_COMMA(int srcargs) mval *dst, ...);
void	op_close(mval *v, mval *p);
void	op_commarg(mval *v, unsigned char argcode);
void	op_decrlock(int4 timeout);
void	op_dmoe(void);
void	op_div(mval *u, mval *v, mval *q);
void	op_exp(mval *u, mval* v, mval *p);
int4	op_fnfind(mval *src, mval *del, mint first, mval *dst);
void	op_fnfnumber(mval *src,mval *fmt,mval *dst);
void	op_fnj2(mval *src, int len, mval *dst);
void	op_fnj3(mval *src, int width, int fract, mval *dst);
void	op_fnlvname(mval *src, mval *dst);
void	op_fnlvnameo2(mval *src, mval *dst, mval *direct);
#ifdef __sun
void	op_fnfgncal(uint4 n_mvals, ...);
int	op_fnfgncal_rpc(unsigned int n_mvals, ...); /* typ to keep the compiler happy as set into xfer_table, which is int */
#elif defined(UNIX)
void  op_fnfgncal (uint4 n_mvals, mval *dst, mval *package, mval *extref, uint4 mask, int4 argcnt, ...);
#elif defined(VMS)
void op_fnfgncal (mval *dst, ...);
#else
#error unsupported platform
#endif
void	op_fngvget(mval *v, mval *def);
void	op_fngetjpi(mint jpid, mval *kwd, mval *ret);
void	op_fnlvprvname(mval *src, mval *dst);
void	op_fnname(UNIX_ONLY_COMMA(int sub_count) mval *finaldst, ...);
void	op_fnqlength(mval *name, mval *subscripts);
void	op_fnqsubscript(mval *name, int seq, mval *subscript);
void	op_fnpopulation(mval *arg1, mval *arg2, mval *dst);
void	op_fnrandom(int4 interval, mval *ret);
void	op_fnreverse(mval *src, mval *dst);
void	op_fnstack1(int level, mval *result);
void	op_fnstack2(int level, mval *info, mval *result);
void	op_fnview(UNIX_ONLY_COMMA(int numarg) mval *dst, ...);
UNIX_ONLY(void op_fnpiece(mval *src, mval *del, int first, int last, mval *dst);)
VMS_ONLY( void op_fnpiece(mval *src, mval *del, int first, int last, mval *dst, boolean_t srcisliteral);)
void	op_fnquery(UNIX_ONLY_COMMA(int sbscnt) mval *dst, ...);
void	op_fntext(mval *label, int int_exp, mval *rtn, mval *ret);
void	op_fnzbitand(mval *dst, mval *bitstr1, mval *bitstr2);
void	op_fnzbitcoun(mval *dst, mval *bitstr);
void	op_fnzbitget(mval *dst, mval *bitstr, int pos);
void	op_fnzbitlen(mval *dst, mval *bitstr);
void	op_fnzbitor(mval *dst, mval *bitstr1, mval *bitstr2);
void	op_fnzbitstr(mval *bitstr, int size, int truthval);
void	op_fnzjobexam(mval *prelimSpec, mval *finalSpec);
void	op_fnzsigproc(int processid, int signum, mval *retcode);
void	op_fnzlkid(mint boolex, mval *retval);
void	op_fnzqgblmod(mval *v);
void	op_fnztrnlnm(mval *name, mval *table, int4 ind, mval *mode, mval *case_blind, mval *item, mval *ret);
#ifdef __sun
void	op_fnzcall(unsigned int n_mvals, ...);
#elif defined(VMS)
void	op_fnzcall(mval *dst, ...);
#elif defined(UNIX)
void	op_fnzcall(void);	/* stub only */
#else
#error unsupported platform
#endif
void	op_fnzpid(mint boolexpr,mval *ret);
void	op_fnzpriv(mval *prv,mval *ret);
void	op_fngetsyi(mval *keyword,mval *node,mval *ret);
void	op_gvdata(mval *v);
void	op_gvextnam(UNIX_ONLY_COMMA(int4 count) mval *val1, ...);
void	op_gvincr(mval *increment, mval *result);
void	op_gvnaked(UNIX_ONLY_COMMA(int count_arg) mval *val_arg, ...);
void	op_gvname(UNIX_ONLY_COMMA(int count_arg) mval *val_arg, ...);
void	op_gvnext(mval *v);
void	op_gvorder(mval *v);
void	op_gvo2(mval *dst, mval *direct);
void	op_gvput(mval *var);
void	op_gvquery(mval *v);
void	op_gvrectarg(mval *v);
void	op_halt(void);
void	op_hang(mval *num);
void	op_hardret(void);
void	op_horolog(mval *s);
int	op_incrlock(int timeout);
void	op_iocontrol(UNIX_ONLY_COMMA(int4 n) mval *vparg, ...);
void	op_indrzshow(mval *s1,mval *s2);
void	op_iretmval(mval *v);
int	op_job(UNIX_ONLY(int4 argcnt) VMS_ONLY(mval *label), ...);
void	op_killall(void);
void	op_lkinit(void);
void	op_lkname(UNIX_ONLY_COMMA(int subcnt) mval *extgbl1, ...);
void	op_mul(mval *u, mval *v, mval *p);
void	op_newvar(uint4 arg1);
void	op_newintrinsic(int intrtype);
void	op_oldvar(void);
void	op_rterror(int4 sig, boolean_t subrtn);
void	op_setp1(mval *src, int delim, mval *expr, int ind, mval *dst);
void	op_setpiece(mval *src, mval *del, mval *expr, int4 first, int4 last, mval *dst);
void	op_fnzsetprv(mval *prv,mval *ret);
void	op_fnztrnlnm(mval *name,mval *table,int4 ind,mval *mode,mval *case_blind,mval *item,mval *ret);
void	op_setzbrk(mval *rtn, mval *lab, int offset, mval *act, int cnt);
void	op_sqlinddo(mstr *m_init_rtn);
void	op_sub(mval *u, mval *v, mval *s);
void	op_svget(int varnum, mval *v);
void	op_svput(int varnum, mval *v);
void	op_tcommit(void);
void	op_trollback(int rb_levels);
void	op_tstart(int dollar_t, ...);
void	op_unlock(void);
void	op_use(mval *v, mval *p);
void	op_view(UNIX_ONLY_COMMA(int numarg) mval *keyword, ...);
void	op_write(mval *v);
void	op_wteol(int4 n);
void	op_wtff(void);
void	op_wtone(int c);
void	op_wttab(mint x);
void	op_xkill(UNIX_ONLY_COMMA(int n) mval *lvname_arg, ...);
void	op_xnew(UNIX_ONLY_COMMA(unsigned int argcnt_arg) mval *s_arg, ...);
void	op_zattach(mval *);
void	op_zcompile(mval *v);
void	op_zcont(void);
void	op_zdealloc2(int4 timeout, uint4 auxown);
void	op_zdeallocate(int4 timeout);
void	op_zedit(mval *v, mval *p);
void	op_zhelp_xfr(mval *subject, mval *lib);
void	op_zlink(mval *v, mval *quals);
void	op_zmess(UNIX_ONLY(unsigned int cnt) VMS_ONLY(int4 errnum), ...);
void	op_zprevious(mval *v);
void	op_zprint(mval *rtn, mval *start_label, int start_int_exp, mval *end_label, int end_int_exp);
void	op_zst_break(void);
void	op_zstep(uint4 code, mval *action);
void	op_zsystem(mval *v);
void	op_ztcommit(int4 n);
void	op_ztstart(void);
UNIX_ONLY(int) VMS_ONLY(void)	op_fetchintrrpt(), op_startintrrpt(), op_forintrrpt();
int	op_forchk1(), op_forloop();
int	op_zstepfetch(), op_zstepstart(), op_zstzbfetch(), op_zstzbstart();
int	op_mproflinestart(), op_mproflinefetch(), op_mprofforloop();
int	op_linefetch(), op_linestart(), op_zbfetch(), op_zbstart(), op_ret(), op_retarg();
int	opp_ret();
int	op_zst_fet_over(), op_zst_st_over(), op_zstzb_st_over(), opp_zstepret(), opp_zstepretarg();
int	op_zstzb_fet_over(), opp_zst_over_ret(), opp_zst_over_retarg();
#ifdef __MVS__
void	gtm_fetch(unsigned int cnt_arg, unsigned int indxarg, ...);
#else
void	fetch(UNIX_ONLY_COMMA(unsigned int cnt_arg) unsigned int indxarg, ...);
#endif
void	add_mvals(mval *u, mval *v, int subtraction, mval *result);
void	op_bindparm(UNIX_ONLY_COMMA(int frmc) int frmp_arg, ...);
void	op_add(mval *u, mval *v, mval *s);
void	op_sub(mval *u, mval *v, mval *s);
void	op_cvtparm(int iocode, mval *src, mval *dst);
void	op_dmode(void);
void	op_dt_false(void);
void	op_dt_store(int truth_value);
void	op_dt_true(void);
void	op_fnascii(int4 num, mval *in, mval *out);
void	op_fnchar(UNIX_ONLY_COMMA(int cnt) mval *dst, ...);
void	op_fnget2(mval *dst, mval *src, mval *defval);
void	op_fngetdvi(mval *device, mval *keyword, mval *ret);
void	op_fngetlki(mval *lkid_mval, mval *keyword, mval *ret);
int	op_fngvget2(mval *res, mval *val, mval *optional);
UNIX_ONLY(void	op_fnp1(mval *src, int del, int trgpcidx, mval *dst);)
VMS_ONLY( void	op_fnp1(mval *src, int del, int trgpcidx, mval *dst, boolean_t srcisliteral);)
void	op_fntranslate(mval *src,mval *in_str,mval *out_str,mval *dst);
void	op_fnzbitfind(mval *dst, mval *bitstr, int truthval, int pos);
void	op_fnzbitnot(mval *dst, mval *bitstr);
void	op_fnzbitset(mval *dst, mval *bitstr, int pos, int truthval);
void	op_fnzbitxor(mval *dst, mval *bitstr1, mval *bitstr2);
void	op_fnzfile(mval *name,mval *key,mval *ret);
void	op_fnzm(mint x,mval *v);
void	op_fnzparse(mval *file, mval *field, mval *def1, mval *def2, mval *type, mval *ret);
void	op_fnzsqlexpr(mval *value, mval *target);
void	op_fnzsqlfield(int findex, mval *target);
void	op_gvsavtarg(mval *v);
void	op_gvzwrite(UNIX_ONLY_COMMA(int4 count) int4 pat, ...);
void	op_idiv(mval *u, mval *v, mval *q);
void	op_igetsrc(mval *v);
int	op_lock2(int4 timeout, unsigned char laflag);
void	op_inddevparms(mval *devpsrc, int4 ok_iop_parms, mval *devpiopl);
void	op_indfnname(mval *dst, mval *target, mval *value);
void	op_indfun(mval *v, mint argcode, mval *dst);
void	op_indget(mval *dst, mval *target, mval *value);
void	op_indglvn(mval *v,mval *dst);
void	op_indincr(mval *dst, mval *increment, mval *target);
void	op_indlvadr(mval *target);
void	op_indlvarg(mval *v,mval *dst);
void	op_indlvnamadr(mval *target);
void	op_indmerge(mval *glvn_mv, mval *arg1_or_arg2);
void	op_indname(UNIX_ONLY_COMMA(int argcnt) mval *dst, ...);
void	op_indpat(mval *v, mval *dst);
void	op_indo2(mval *dst, mval *target, mval *value);
void	op_indset(mval *target, mval *value);
void	op_indtext(mval *lab, mint offset, mval *rtn, mval *dst);
void	op_lvzwrite(UNIX_ONLY_COMMA(int4 count) int4 arg1, ...);
void	op_nullexp(mval *v);
int	op_open_dummy(mval *v, mval *p, int t, mval *mspace);
void	op_setextract(mval *src, mval *expr, int schar, int echar, mval *dst);
void	op_trestart(int newlevel);
void	op_zst_over(void);
void	op_zstepret(void);
void    op_fnextract (int last, int first, mval *src, mval *dest);
void	op_fnlength(mval *a1, mval *a0);
void	op_fnzascii(int4 num, mval *in, mval *out);
void	op_fnzchar(UNIX_ONLY_COMMA(int cnt) mval *dst, ...);
UNIX_ONLY(void	op_fnzp1(mval *src, int del, int trgpcidx, mval *dst);)
VMS_ONLY( void	op_fnzp1(mval *src, int del, int trgpcidx, mval *dst, boolean_t srcisliteral);)
void	op_fnztranslate(mval *src, mval *in_str ,mval *out_str, mval *dst);
void	op_setzextract(mval *src, mval *expr, int schar, int echar, mval *dst);
void    op_fnzextract (int last, int first, mval *src, mval *dest);
void	op_fnzpopulation(mval *arg1, mval *arg2, mval *dst);
void	op_setzp1(mval *src, int delim, mval *expr, int ind, mval *dst);
void	op_setzpiece(mval *src, mval *del, mval *expr, int4 first, int4 last, mval *dst);
UNIX_ONLY(void	op_fnzpiece(mval *src, mval *del, int first, int last, mval *dst);)
VMS_ONLY( void	op_fnzpiece(mval *src, mval *del, int first, int last, mval *dst, boolean_t srcisliteral);)
int4	op_fnzfind(mval *src, mval *del, mint first, mval *dst);
void	op_fnzj2(mval *src,int len,mval *dst);
void	op_fnzlength(mval *a1, mval *a0);
UNIX_ONLY(void	op_fnzconvert2(mval* str, mval* kase, mval* dst);)
UNIX_ONLY(void 	op_fnzconvert3(mval* str, mval* from_chset, mval* to_chset, mval* dst);)
UNIX_ONLY(void	op_fnzsubstr(mval* src, int start, int bytelen, mval* dest);)
UNIX_ONLY(void	op_fnzwidth(mval* str, mval* dst);)
void	op_fnzechar(UNIX_ONLY_COMMA(int cnt) mval *dst, ...);
void	op_fnzreverse(mval *src, mval *dst);
#endif
