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

#ifndef __CCP_RETCHAN_MANAGER_H__
#define __CCP_RETCHAN_MANAGER_H__


struct retchan_txt
{
	struct retchan_txt *next;
	unsigned char len;
	unsigned char txt[1];
};

struct retchan_header
{
	struct retchan_txt *head;
	struct retchan_txt **tail;
	uint4 chan;
	ccp_action_aux_value mbxnam;
};

struct retchan_header *ccp_retchan_init(ccp_action_aux_value *v);
void ccp_retchan_text(struct retchan_header *p,unsigned char *addr,unsigned short len);
void ccp_retchan_fini(struct retchan_header *p);

#endif
