/****************************************************************
 *								*
 *	Copyright 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTCMTR_PROTOS_H_INCLUDED
#define GTCMTR_PROTOS_H_INCLUDED

bool	gtcmtr_bufflush(void);
bool	gtcmtr_data(void);
bool	gtcmtr_get(void);
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

#endif
