/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2004 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2017-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2004-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 451ab477 (GT.M V7.0-000)
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTCMTR_PROTOS_H_INCLUDED
#define GTCMTR_PROTOS_H_INCLUDED

<<<<<<< HEAD
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
=======
bool	gtcmtr_bufflush(void);
bool	gtcmtr_data(void);
bool	gtcmtr_get(void);
unsigned char *gtcmtr_get_key(void *gvkey, unsigned char *ptr, unsigned short len);
bool	gtcmtr_increment(void);
bool	gtcmtr_initproc(void);
bool	gtcmtr_initreg(void);
bool	gtcmtr_kill(void);
bool	gtcmtr_lkacquire(void);
bool	gtcmtr_lkcanall(void);
bool	gtcmtr_lkcancel(void);
bool	gtcmtr_lkdelete(void);
char	gtcmtr_lke_clearrep(struct CLB *lnk, clear_request *creq);
bool	gtcmtr_lke_clearreq(struct CLB *lnk, char rnum, bool all, bool interactive, int4 pid, mstr *node);
char	gtcmtr_lke_showrep(struct CLB *lnk, show_request *sreq);
bool	gtcmtr_lke_showreq(struct CLB *lnk, char rnum, bool all, bool wait, int4 pid, mstr *node);
bool	gtcmtr_lkreqimmed(void);
bool	gtcmtr_lkreqnode(void);
bool	gtcmtr_lkrequest(void);
bool	gtcmtr_lkresume(void);
bool	gtcmtr_lksuspend(void);
bool	gtcmtr_order(void);
bool	gtcmtr_put(void);
bool	gtcmtr_query(void);
bool	gtcmtr_terminate(bool cm_err);
void	gtcmtr_terminate_free(connection_struct *ce);
bool	gtcmtr_zprevious(void);
bool	gtcmtr_zwithdraw(void);
>>>>>>> 451ab477 (GT.M V7.0-000)

#endif
