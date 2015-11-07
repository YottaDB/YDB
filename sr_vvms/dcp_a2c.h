/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef DCP_A2C_H_INCLUDED
#define DCP_A2C_H_INCLUDED

condition_code	dcpa_shm_init(void);
com_slot_t	*dcpa_read(void);
void		dcpa_send(struct com_slot *p);
void		dcpa_free_user(struct com_slot *user);
condition_code	dcpc_shm_init(boolean_t init_shm);
void		dcpc_send2agent(void);
int		dcpc_rcv_from_agent(void);
void		dcpc_shm_rundown(void);

#endif /* DCP_A2C_H_INCLUDED */
