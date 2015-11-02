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

#ifndef GVCMZ_INCLUDED
#define GVCMZ_INCLUDED

void gvcmz_error(char code, uint4 status);
void gvcmz_neterr(INTPTR_T *error);
void gvcmz_bunch(mval *v);
void gvcmz_clrlkreq(void);
void gvcmz_doop(unsigned char query_code, unsigned char reply_code, mval *v);
void gvcmz_zflush(void);
void gvcmz_neterr_set(struct CLB *);
void gvcmz_errmsg(struct CLB *, bool);
struct CLB *gvcmz_netopen(struct CLB *c, cmi_descriptor *node, cmi_descriptor *task);
void gvcmz_netopen_attempt(struct CLB *c);
void gvcmz_zdefw_ast(struct CLB *lnk);
void gvcmz_zdefr_ast(struct CLB *lnk);
void gvcmz_int_lkcancel(void);
void gvcmz_lkacquire_ast(struct CLB *lnk);
void gvcmz_lkcancel_ast(struct CLB *lnk);
void gvcmz_lkread_ast(struct CLB *lnk);
void gvcmz_lksublist(struct CLB *lnk);
void gvcmz_lksuspend_ast(struct CLB *lnk);
void gvcmz_sndlkremove(struct CLB *lnk, unsigned char oper, unsigned char cancel);

#endif /* GVCMZ_INCLUDED */
