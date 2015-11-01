/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * -------------------------------------------------------------
 *	Setup a new connection.
 *	Called by cmj_mbx_ast() upon a CONNECTION request
 *	message reception on a NET level communication link.
 *	When this routine is called, the CLB structure is
 *	allocated and linked into the CLB que.
 *
 *	Structurally, there is one NTD structure, which is
 *	at the head of the list of CLB structures.
 *	There is one CLB structure per separate connection.
 *	The usr pointer of the CLB points to a connection
 *	structure, that is used to process a request from
 *	a client. There is one connection structure per CLB.
 *
 *	The connection structure has a pointer clb_ptr,
 *	pointing back to the CLB structure, for which this
 *	connection request originates.
 *	Upon a receipt of a new connection, we allocate
 *	a new CLB and a new connection structure, and hang a
 *	connection structure off of CLB and vice versa.
 *
 * NOTE:
 *	Effectively, the CLB and connection structures are one
 *	single structure, which needs to be linked into two
 *	separate lists - the CLB list, and the action que.
 *	And, in order to utilize the interlocked insert and
 *	delete features of the insqti, delqti, etc. instructions,
 *	we need to add the relque structures at the beginning
 *	of each structure that needs to be linked in some list,
 *	which makes it impossible to have a single structure
 *	that can be linked into two separate list. That is why
 *	we have separate CLB and the connection structures.
 * End of NOTE.
 *
 *	Later on, when the actual message arrives on the link,
 *	we link the connection structure into the action que.
 *	When connection is processed completely, we unlink
 *	the connection structure from the action que, although
 *	it still remains to be related to the same CLB.
 *	Therefore, for the period while the connection structure
 *	remains in the action que, we cannot accept new requests
 *	from the client, because the CLB and the connection
 *	structure are busy. According to the application layer
 *	of the protocol, the services for the same user are
 *	performed synchronously, and there is only one outstanding
 *	request at a time. This significantly simplifies the
 *	interface and concurrency considerations.
 *
 *	There is a little trick to see if a given connection
 *	structure is linked into the action que during the
 *	processing of some network event, and, therefore,
 *	no requests can be accepted on that link. Since the
 *	action que is a doubly linked list, if an element is
 *	taken out of the list, it will have its forward and
 *	backward links unchanged, but the forward link will point
 *	to the node, whose backward link does not point back
 *	to the original node, being unlined, but will point to
 *	some other node - true previous node of the updated
 *	linked list. Since the ques are self relative,
 *	(see VAX MACRO manual, queue description), and considering
 *	a fact that in the properly linked element, the forward link
 *	and the backward link of the next element should cancell
 *	each other, and a sum of those two links should be 0.
 *	In a node that is unlinked from the list this sum will not
 *	be 0.
 * -------------------------------------------------------------
 */

#include "gtm_stdio.h"

#include "mdef.h"
#include "cmidef.h"
#include "cmmdef.h"

#include "gt_timer.h"

GBLREF int gtcm_users;
GBLREF bool cm_shutdown;
GBLREF bool cm_timeout;

gtcm_init_ast(clb)
struct CLB *clb;
{
	connection_struct *cs;
	void gtcm_read_ast();

		/* Allocate a connection structure for new user */
	cs = (connection_struct *) malloc(sizeof(connection_struct));
	memset(cs,0,sizeof(connection_struct));
	cs->lk_cancel = CM_NOLKCANCEL;

		/* Update time this connection was established*/
	time((time_t *)&cs->connect[0]);
	clb->usr = (void *) cs;
	cs->clb_ptr = clb;
/*
 * Setup a read AST handler
 */
	clb->mbf = (unsigned char *)
		malloc(CM_MSG_BUF_SIZE + CM_BUFFER_OVERHEAD);
	clb->mbl = CM_MSG_BUF_SIZE + CM_BUFFER_OVERHEAD;

	clb->ast = gtcm_read_ast;
	cmj_async_mode(clb);
	gtcm_users++;
	if (cm_timeout)
 		cancel_timer((TID) &cm_shutdown);
#ifdef DP
fprintf(stderr,"Leaving gtcm_init_ast.\n");
#endif
}
