/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "ccp.h"
#include "ccp_retchan_manager.h"
#include "ast.h"
#include <psldef.h>
#include <ssdef.h>

/* ccp_queue_manager - A package to manage ccp queue entries */

#define NUM_QUE_SLOTS	100
#define MIN_QUE_FREE	5
#define QUE_EXTEND_SIZE	(NUM_QUE_SLOTS / 2)
#define MAX_QUE_SIZE	1000

GBLDEF	ccp_que_entry	*current_item;
GBLDEF	ccp_relque	*ccp_action_que;

error_def(ERR_CCPINTQUE);

static	ccp_relque	*ccp_free_que;
static	short int	ccp_que_entry_count, ccp_que_free_count, ccp_que_free_minimum;

#define CCP_MAX_PRIORITY 3
#define CCPR_HI		2
#define CCPR_NOR	1
#define CCPR_LOW	0

#define CCP_TABLE_ENTRY(A,B,C,D) D,
static	const unsigned char	priority[] =
{
#include "ccpact_tab.h"
};
#undef CCP_TABLE_ENTRY

/* Get the first action available to process;  if NULL, queues were empty */

ccp_action_record *ccp_act_select(void)
{
	ccp_relque	*p;
	ccp_que_entry	*result;


	assert(current_item == NULL);

	for (p = &ccp_action_que[CCP_MAX_PRIORITY];  p > ccp_action_que;)
	{
		result = remqhi(--p);
		if (result == -1)
			lib$signal(ERR_CCPINTQUE);
		else
			if (result != NULL)
				break;
	}

	if (result == NULL)
		return NULL;

	current_item = result;

	return &result->value;
}


void ccp_act_complete(void)
{
	int4	status;


	assert(current_item != NULL);
	sys$gettim(&current_item->process_time);
	status = insqti(current_item, ccp_free_que);
	if (status == -1)
		lib$signal(ERR_CCPINTQUE);

	current_item = NULL;
	adawi(1, &ccp_que_free_count);

	if (ccp_que_free_count == ccp_que_free_minimum + MIN_QUE_FREE)
	{
		status = sys$dclast(ccp_mbx_start, 0, PSL$C_USER);
		if (status != SS$_NORMAL)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
	}

	return;
}

bool ccp_act_request(ccp_action_record *rec)
{
	ccp_action_code	act;
	ccp_que_entry	*ptr;
	/* need to deal with starvation */


	act = rec->action;

	if (act == CCTR_NULL)
		return (ccp_que_free_count > ccp_que_free_minimum);

	if (act <= 0 || act >= CCPACTION_COUNT)
		GTMASSERT;

	ptr = remqhi(ccp_free_que);
	if (ptr == -1)
		lib$signal(ERR_CCPINTQUE);
	if (ptr == 0)
		GTMASSERT;

	adawi(-1, &ccp_que_free_count);

	sys$gettim(&ptr->request_time);
	ptr->value = *rec;
	insqti(ptr, &ccp_action_que[priority[act]]);

	sys$wake(NULL, NULL);

	return (ccp_que_free_count > ccp_que_free_minimum);
}


/* Places the request at the head of the high priority queue */

bool ccp_priority_request(ccp_action_record *rec)
{
	ccp_action_code	act;
	ccp_que_entry	*ptr;
	/* need to deal with starvation */


	act = rec->action;

	if (act == CCTR_NULL)
		return (ccp_que_free_count > ccp_que_free_minimum);

	if (act <= 0 || act >= CCPACTION_COUNT)
		GTMASSERT;

	ptr = remqhi(ccp_free_que);
	if (ptr == -1)
		lib$signal(ERR_CCPINTQUE);
	if (ptr == 0)
		GTMASSERT;

	adawi(-1, &ccp_que_free_count);

	sys$gettim(&ptr->request_time);
	ptr->value = *rec;
	insqhi(ptr, &ccp_action_que[priority[act]]);

	sys$wake(NULL, NULL);

	return (ccp_que_free_count > ccp_que_free_minimum);
}

void ccp_act_init(void)
{
	unsigned char	*ptr;
	int		n, siz;


	ccp_free_que = malloc(SIZEOF(ccp_relque));
	memset(ccp_free_que, 0, SIZEOF(ccp_relque));

	ccp_action_que = malloc(SIZEOF(ccp_relque) * CCP_MAX_PRIORITY);
	memset(ccp_action_que, 0, SIZEOF(ccp_relque) * CCP_MAX_PRIORITY);

	ccp_que_free_count = ccp_que_entry_count = NUM_QUE_SLOTS;
	ccp_que_free_minimum = MIN_QUE_FREE;

	if (SIZEOF(ccp_que_entry) & 7)
		siz = (SIZEOF(ccp_que_entry) & ~7) + 8;
	else
		siz = SIZEOF(ccp_que_entry) ;
	ptr = malloc(siz * ccp_que_entry_count);
	/* set buffer space to zero in order to distinguish history records from never used records */
	memset(ptr, 0, siz * ccp_que_entry_count);

	for (n = 0;  n < ccp_que_entry_count;  n++, ptr += siz)
	{
		insqti(ptr, ccp_free_que);
		/* could check for success here */
	}

	return;
}


void ccp_quemin_adjust(char oper)
{
	if (oper == CCP_OPEN_REGION && ccp_que_free_minimum < ccp_que_entry_count / 2)
		ccp_que_free_minimum += 3;
	else
		if (oper == CCP_CLOSE_REGION && ccp_que_free_minimum > MIN_QUE_FREE)
			ccp_que_free_minimum -= 3;

	return;
}

#define PUTLIT(X)	ccp_retchan_text(context, LIT_AND_LEN(X));
#define GETTAIL(X)	(((unsigned char *) (X)) + (X)->bl)


void ccp_quedump1(ccp_action_record *tr)
{
	unsigned char	*endptr, *context, buffer[128];
	ccp_que_entry	*qe;


	context = ccp_retchan_init(&tr->v);

	PUTLIT("HISTORY QUEUE CONTENTS:");

	for (qe = GETTAIL(ccp_free_que);  qe != ccp_free_que;  qe = GETTAIL(&(qe->q)))
		if (qe->value.action != CCTR_NULL)
		{
			endptr = ccp_format_querec(qe, buffer, SIZEOF(buffer));
			ccp_retchan_text(context, buffer, endptr - buffer);
		}

	ccp_retchan_fini(context);

	return;
}


void ccp_tr_quedump(ccp_action_record *tr)
{
	uint4	status;

	/* Note: should add other requests here, also syntax in ccp might be sho que/history */

	/* ccp_quedump1 needs to be in an AST to prevent ccp_mbx_interrupt from changing the queue */
	status = sys$dclast(ccp_quedump1, tr, PSL$C_USER);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	return;
}
