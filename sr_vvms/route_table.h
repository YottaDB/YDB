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

#ifndef ROUTE_TABLE_H_INCLUDED
#define ROUTE_TABLE_H_INCLUDED

void 		remove_circuits(ddp_hdr_t *dp);
boolean_t	enter_circuits(ddp_hdr_t *dp);
unsigned short	find_circuit(unsigned short vol);
routing_tab	*find_route(unsigned short ckt);
void		reset_user_count(int jobindex);
boolean_t 	enter_vug(unsigned short vol, unsigned short uci, mstr *gld);
void 		clear_volset_table(void);
mstr 		*find_gld(unsigned short vol, unsigned short uci);

#endif /* ROUTE_TABLE_H_INCLUDED */
