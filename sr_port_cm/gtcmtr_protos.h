/****************************************************************
 *								*
 * Copyright (c) 2004-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTCMTR_PROTOS_H_INCLUDED
#define GTCMTR_PROTOS_H_INCLUDED

cm_op_t	gtcmtr_bufflush(void);
cm_op_t	gtcmtr_data(void);
cm_op_t	gtcmtr_get(void);
cm_op_t	gtcmtr_increment(void);
cm_op_t	gtcmtr_initproc(void);
cm_op_t	gtcmtr_initreg(void);
cm_op_t	gtcmtr_kill(void);
cm_op_t	gtcmtr_lkacquire(void);
cm_op_t	gtcmtr_lkcanall(void);
cm_op_t	gtcmtr_lkcancel(void);
cm_op_t	gtcmtr_lkdelete(void);
cm_op_t	gtcmtr_lkreqimmed(void);
cm_op_t	gtcmtr_lkreqnode(void);
cm_op_t	gtcmtr_lkrequest(void);
cm_op_t	gtcmtr_lkresume(void);
cm_op_t	gtcmtr_lksuspend(void);
cm_op_t	gtcmtr_order(void);
cm_op_t	gtcmtr_put(void);
cm_op_t	gtcmtr_query(void);
cm_op_t	gtcmtr_reversequery(void);
cm_op_t	gtcmtr_terminate(bool cm_err);
cm_op_t	gtcmtr_zprevious(void);
cm_op_t	gtcmtr_zwithdraw(void);
cm_op_t	gtcmtr_lke_clearrep(struct CLB *lnk, clear_request *creq);
cm_op_t	gtcmtr_lke_showrep(struct CLB *lnk, show_request *sreq);

bool gtcmtr_lke_clearreq(struct CLB *lnk, char rnum, bool all, bool interactive, int4 pid, mstr *node);
bool gtcmtr_lke_showreq(struct CLB *lnk, char rnum, bool all, bool wait, int4 pid, mstr *node);

unsigned char *gtcmtr_get_key(void *gvkey, unsigned char *ptr, unsigned short len);

#endif
