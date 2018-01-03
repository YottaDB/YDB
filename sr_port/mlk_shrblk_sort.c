/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "mlkdef.h"
#include "mmemory.h"

mlk_shrblk_ptr_t mlk_shrblk_sort(mlk_shrblk_ptr_t head);

void mlk_shrblk_split(mlk_shrblk_ptr_t head, mlk_shrblk_ptr_t *lhead, mlk_shrblk_ptr_t *rhead);
mlk_shrblk_ptr_t mlk_shrblk_merge(mlk_shrblk_ptr_t lhead, mlk_shrblk_ptr_t rhead);

/* Do an in-place sort of this level of shrblks, starting from head, returning the new first shrblk. */
mlk_shrblk_ptr_t mlk_shrblk_sort(mlk_shrblk_ptr_t head)
{
	mlk_shrblk_ptr_t	lhead, rhead;

	if ((mlk_shrblk_ptr_t)R2A(head->rsib) == head)
	{
		assert((mlk_shrblk_ptr_t)R2A(head->lsib) == head);
		return head;
	}
	assert((mlk_shrblk_ptr_t)R2A(head->lsib) != head);
	mlk_shrblk_split(head, &lhead, &rhead);
	lhead = mlk_shrblk_sort(lhead);
	rhead = mlk_shrblk_sort(rhead);
	return mlk_shrblk_merge(lhead, rhead);
}

void mlk_shrblk_split(mlk_shrblk_ptr_t head, mlk_shrblk_ptr_t *lhead, mlk_shrblk_ptr_t *rhead)
{
	mlk_shrblk_ptr_t	cur, left = NULL, right = NULL;

	cur = head;
	while (TRUE)
	{
		if (NULL == left)
			*lhead = cur;
		else
		{
			A2R(left->rsib, cur);
			A2R(cur->lsib, left);
		}
		left = cur;
		cur = (mlk_shrblk_ptr_t)R2A(cur->rsib);
		if (cur == head)
		{
			assert(NULL != right);
			break;
		}
		if (NULL == right)
			*rhead = cur;
		else
		{
			A2R(right->rsib, cur);
			A2R(cur->lsib, right);
		}
		right = cur;
		cur = (mlk_shrblk_ptr_t)R2A(cur->rsib);
		if (cur == head)
			break;
	}
	A2R((*lhead)->lsib, left);
	A2R((*rhead)->lsib, right);
	A2R(left->rsib, *lhead);
	A2R(right->rsib, *rhead);
}

mlk_shrblk_ptr_t mlk_shrblk_merge(mlk_shrblk_ptr_t lhead, mlk_shrblk_ptr_t rhead)
{
	mlk_shrblk_ptr_t	left, right, ltail, rtail, head = NULL, tail;
	mlk_shrsub_ptr_t	lsub, rsub;

	ltail = (mlk_shrblk_ptr_t)R2A(lhead->lsib);
	rtail = (mlk_shrblk_ptr_t)R2A(rhead->lsib);
	left = lhead;
	lsub = (mlk_shrsub_ptr_t)R2A(left->value);
	right = rhead;
	rsub = (mlk_shrsub_ptr_t)R2A(right->value);
	while (TRUE)
	{
		if (0 > memvcmp(lsub->data, lsub->length, rsub->data, rsub->length))
		{
			if (NULL == head)
				head = left;
			else
			{
				A2R(tail->rsib, left);
				A2R(left->lsib, tail);
			}
			tail = left;
			left = (mlk_shrblk_ptr_t)R2A(left->rsib);
			if (left == lhead)
			{
				A2R(tail->rsib, right);
				A2R(right->lsib, tail);
				A2R(head->lsib, rtail);
				A2R(rtail->rsib, head);
				break;
			}
			lsub = (mlk_shrsub_ptr_t)R2A(left->value);
		}
		else
		{
			if (NULL == head)
				head = right;
			else
			{
				A2R(tail->rsib, right);
				A2R(right->lsib, tail);
			}
			tail = right;
			right = (mlk_shrblk_ptr_t)R2A(right->rsib);
			if (right == rhead)
			{
				A2R(tail->rsib, left);
				A2R(left->lsib, tail);
				A2R(head->lsib, ltail);
				A2R(ltail->rsib, head);
				break;
			}
			rsub = (mlk_shrsub_ptr_t)R2A(right->value);
		}
	}
	return head;
}
