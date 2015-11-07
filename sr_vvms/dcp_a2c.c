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

#include <descrip.h>
#include <secdef.h>
#include <jpidef.h>
#include <ssdef.h>
#include <stddef.h>

#include "ddphdr.h"
#include "ddpcom.h"
#include "decddp.h"
#include "longset.h"
#include "dcp_a2c.h"
#include "crit_wake.h"
#include "is_proc_alive.h"

GBLDEF com_hdr_t	*com_area = NULL;
GBLDEF com_slot_t	*com_ptr = NULL;
GBLDEF int4		ddp_slot_size;

GBLREF mstr		my_circuit_name;
GBLREF int4		ddp_max_rec_size;

static int4		queue_retry_count = 20000;
static boolean_t	transmit_timer_expired;

static condition_code	init_section(boolean_t agent);
static condition_code	get_pid(int4 *pid);
static void		transmit_timer_ast(void);
static void		set_transmit_timer(void);

static condition_code init_section(boolean_t agent)
{
	condition_code		status;
	int4			inadr[2], retadr[2], flags, com_pagcnt, nproc;
	com_slot_t		*nthslot;
	struct dsc$descriptor	section_name;
	char section_name_buffer[] = DDP_AGENT_BUFF_NAME;

	/* Global section layout ****
	 *
	 * COM_HDR								<- com_area
	 * MAXIMUM_PROCESSES * COM_SLOT	(slot 0 thru MAXIMUM_PROCESSES - 1)	<- com_area->slot array
	 */
	INIT_DESCRIP(section_name, section_name_buffer);
	section_name.dsc$w_length = STR_LIT_LEN(section_name_buffer);
	assert(DDP_CIRCUIT_NAME_LEN < section_name.dsc$w_length);
	memcpy(&section_name.dsc$a_pointer[section_name.dsc$w_length - DDP_CIRCUIT_NAME_LEN],
			my_circuit_name.addr, my_circuit_name.len);
	inadr[0] = inadr[1] = 0; /* indicates that data is to be mapped into p0 space. Does not specify starting address, as
				  * SEC$M_EXPREG is set in the flags */
	ddp_slot_size = ROUND_UP(offsetof(com_slot_t, text[0]) + DDP_MSG_HDRLEN + ddp_max_rec_size, OS_PAGELET_SIZE);
	assert(0 == (ddp_slot_size & 1)); /* even slot size for padding odd length outbound message */
	com_pagcnt = DIVIDE_ROUND_UP(SIZEOF(com_hdr_t) + MAXIMUM_PROCESSES * ddp_slot_size, OS_PAGELET_SIZE);
	flags = SEC$M_GBL | SEC$M_SYSGBL | SEC$M_WRT | SEC$M_PAGFIL | SEC$M_EXPREG | SEC$M_DZRO;
	status = sys$crmpsc(inadr, retadr, 0, flags, &section_name, 0, 0, 0, com_pagcnt, 0, 0, 0);
	if (0 == (status & 1))
		return status;
	com_area = retadr[0];
	if (agent && 0 == com_area->server_pid)
	{
		for (nproc = 0; MAXIMUM_PROCESSES > nproc; nproc++)
		{
			nthslot = (com_slot_t *)((unsigned char *)com_area->slot + (nproc * ddp_slot_size));
			lib$insqti(&(nthslot->q), &(com_area->unused_slots), &queue_retry_count);
		}
	}
	return status;
}

static condition_code get_pid(int4 *pid)
{
	condition_code	status;
	int4		item_code;

	item_code = JPI$_PID;
	status = lib$getjpi(&item_code, 0, 0, pid, 0, 0);
	return status;
}

/* Initialize shared memory: agent's version */
condition_code dcpa_shm_init(void)
{
	condition_code status;

	status = init_section(TRUE);
	if (0 == (status & 1))
		return status;
	status = get_pid(&(com_area->server_pid));
	return status;
}

/* routine to get message from client...0 means there is none*/
com_slot_t *dcpa_read(void)
{
	com_slot_t *p;

	lib$remqhi(&(com_area->outbound_pending), &p, &queue_retry_count);
	return (p == &(com_area->outbound_pending)) ? 0 : p;
}

void dcpa_send(com_slot_t *p)
{
	p->state = 0;
	crit_wake(&(p->pid));
	return;
}

void dcpa_free_user(com_slot_t *user)
{
	user->pid = 0;
	lib$insqti(&(user->q), &(com_area->unused_slots), &queue_retry_count);
}

/* Initialize shared memory: client's version */
condition_code dcpc_shm_init(boolean_t init_shm)
{
	condition_code	status;
	uint4		server;
	error_def(ERR_DDPTOOMANYPROCS);
	error_def(ERR_DDPNOSERVER);

	if (FALSE != init_shm)
	{
		status = init_section(FALSE);
		if (0 == (status & 1))
			return status;
	}
	if (0 == (server = com_area->server_pid) /* server not running, client had enough privs. to create shm memory */
	    || !is_proc_alive(server, 0))
		return ERR_DDPNOSERVER;
	lib$remqhi(&(com_area->unused_slots), &com_ptr, &queue_retry_count);
	if (com_ptr == &com_area->unused_slots)
	{
		com_ptr = NULL;
		return ERR_DDPTOOMANYPROCS;
	}
	status = get_pid(&(com_ptr->pid));
	return status;
}

/* Client routine to send-to-agent */
void dcpc_send2agent(void)
{
	com_ptr->state = 1;
	lib$insqti(&(com_ptr->q), &(com_area->outbound_pending), &queue_retry_count);
	crit_wake(&(com_area->server_pid));
	return;
}

static void transmit_timer_ast()
{
	transmit_timer_expired = TRUE;
	sys$wake(0, 0);
}

static void set_transmit_timer(void)
{
	static readonly int4 timeout[2] = {-50000000, -1}; /* 5 s */

	transmit_timer_expired = FALSE;
	sys$setimr(0, timeout, transmit_timer_ast, &transmit_timer_expired, 0);
}

/* Client routine to receive from agent */
/* Returns 1 if timed out, 0 if got some data */
int dcpc_rcv_from_agent(void)
{
	set_transmit_timer();
	while (com_ptr->state == 1)
	{
		sys$hiber();
		if (transmit_timer_expired)
		{
			com_ptr->state = 0;
			return 1;
		}
	}
	sys$cantim(&transmit_timer_expired, 0);
	return 0;
}

void dcpc_shm_rundown(void)
{
	com_ptr->pid = 0;
	lib$insqti(&(com_ptr->q), &(com_area->unused_slots), &queue_retry_count);
}
