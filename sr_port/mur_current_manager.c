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

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "buddy_list.h"

buddy_list	*current_buddy_list;

void	mur_current_initialize(void)
{
	current_buddy_list = (buddy_list *)malloc(sizeof(buddy_list));
	initialize_list(current_buddy_list, sizeof(struct current_struct), MUR_CURRENT_LIST_INIT_ALLOC);
}

void	mur_cre_current(ctl_list *ctl, uint4 pini_addr, token_num token, jnl_process_vector *pv, bool broken)
{
	struct current_struct	*p;

	p = (struct current_struct *)get_new_free_element(current_buddy_list);
	p->next = ctl->current_list;
	ctl->current_list = p;

	p->pini_addr = pini_addr;
	QWASSIGN(p->token, token);
	p->pid = pv->jpv_pid;
	memcpy(p->process, pv->jpv_prcnam, JPV_LEN_PRCNAM);
	memcpy(p->user, pv->jpv_user, JPV_LEN_USER);
	p->broken = broken;
}

struct current_struct	*mur_lookup_current(ctl_list *ctl, uint4 pini_addr)
{
	struct current_struct	*p;

	for (p = ctl->current_list;  p != NULL  &&  p->pini_addr != pini_addr;  p = p->next)
		;
	return p;
}

void	mur_delete_current(ctl_list *ctl, uint4 pini_addr)
{
	struct current_struct	*p, *prev;

	for (p = ctl->current_list, prev = NULL;  p != NULL;  prev = p, p = p->next)
	{
		if (p->pini_addr == pini_addr)
		{
			if (prev == NULL)
				ctl->current_list = p->next;
			else
				prev->next = p->next;

			free_element(current_buddy_list, (char *)p);
			return;
		}
	}
}

void	mur_empty_current(ctl_list *ctl)
{
	struct current_struct	*next;

	while (ctl->current_list != NULL)
	{
		next = ctl->current_list->next;
		free_element(current_buddy_list, (char *)ctl->current_list);
		ctl->current_list = next;
	}
}

void	mur_cre_broken(ctl_list *ctl, uint4 pini_addr, token_num token)
{
	struct current_struct	*p;

	p = (struct current_struct *)get_new_free_element(current_buddy_list);
	memset(p, 0, sizeof(struct current_struct));
	p->next = ctl->broken_list;
	ctl->broken_list = p;

	p->pini_addr = pini_addr;
	QWASSIGN(p->token, token);
}


bool	mur_lookup_broken(ctl_list *ctl, uint4 pini_addr, token_num token)
{
	struct current_struct	*p;

	for (p = ctl->broken_list;  p != NULL;  p = p->next)
	{
		if (p->pini_addr == pini_addr  &&  QWEQ(p->token, token))
			return TRUE;
	}
	return FALSE;
}


void	mur_include_broken(ctl_list *ctl)
{
	struct current_struct	*p;
	show_list_type		*slp;

	for (slp = ctl->show_list;  slp != NULL;  slp = slp->next)
	{
		if (slp->broken)
		{
			for (p = ctl->current_list;  p != NULL;  p = p->next)
			{
				if (p->broken  &&  p->pid == slp->jpv.jpv_pid)
				{
					slp->recovered = TRUE;
					break;
				}
			}
		}
	}
}


void	mur_move_curr_to_broken(ctl_list *ctl, struct current_struct *curr)
{
	struct current_struct	*p, *prev;

	for (p = ctl->current_list, prev = NULL;  p != NULL;  prev = p, p = p->next)
	{
		if (p == curr)
		{
			if (prev == NULL)
				ctl->current_list = p->next;
			else
				prev->next = p->next;

			p->next = ctl->broken_list;
			ctl->broken_list = p;

			return;
		}
	}
}

