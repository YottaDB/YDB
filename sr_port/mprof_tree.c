/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "error.h"
#include "mprof.h"
#include "min_max.h" /* necessary for MIN which is necessary for MIDENT_CMP */

/* macros indicating descend direction */
#define LEFT 	0
#define RIGHT 	1
#define NEITHER	-1
#define BASE	1

/* macros indicating node comparison direction */
#define LESS	-1
#define MORE	1
#define EQUAL	0

/* macros indicating node involvement in insertion */
#define IN_PATH		-1
#define NOT_IN_PATH	1
#define IS_INS_NODE	0

STATICDEF mident *tmp_rout_name, *tmp_label_name;

/* Creates a new generic node in the MPROF tree based on the information passed in arg. */
mprof_tree *new_node(trace_entry *arg)
{
	mprof_tree *tree;

	/* First, we need to pcalloc new space for the label/routine if we have not
	 * found an occurrence that is in the tree already
	 */
	if (NULL == tmp_rout_name)
		tmp_rout_name = arg->rout_name;
	if ((NULL != tmp_rout_name) && (tmp_rout_name == arg->rout_name))
	{
		arg->rout_name = (mident *)pcalloc(SIZEOF(mident));
		arg->rout_name->len = tmp_rout_name->len;
		arg->rout_name->addr = (char *)pcalloc((unsigned int)tmp_rout_name->len);
		memcpy(arg->rout_name->addr, tmp_rout_name->addr, tmp_rout_name->len);
	}
	if (NULL == tmp_label_name)
		tmp_label_name = arg->label_name;
	if ((NULL != tmp_label_name) && (tmp_label_name == arg->label_name))
	{
		arg->label_name = (mident *)pcalloc(SIZEOF(mident));
		arg->label_name->len = tmp_label_name->len;
		arg->label_name->addr = (char *)pcalloc((unsigned int)tmp_label_name->len);
		memcpy(arg->label_name->addr, tmp_label_name->addr, tmp_label_name->len);
	}
	tree = (mprof_tree *)pcalloc(SIZEOF(mprof_tree));
	tree->e.rout_name = arg->rout_name;
	tree->e.label_name = arg->label_name;
	tree->e.line_num = arg->line_num;
	tree->e.count = tree->e.usr_time = tree->e.sys_time = tree->e.elp_time = tree->e.loop_level = 0;
	tree->e.raddr = NULL;
	tree->link[LEFT] = tree->link[RIGHT] = tree->loop_link = NULL;
	tree->desc_dir = NEITHER;
	tree->ins_path_hint = NOT_IN_PATH;
	return tree;
}

/* Creates a FOR-specific node in the MPROF tree based on the information passed in arg and
 * return_address for this loop.
 */
mprof_tree *new_for_node(trace_entry *arg, char *ret_addr)
{
	mprof_tree *node;

	node = (mprof_tree *)new_node(arg);
	node->e.raddr = (char *)ret_addr;
	return node;
}

/* Traverses the tree and saves every value in the global */
void mprof_tree_walk(mprof_tree *node)
{
	mprof_tree *loop_link;

	while (TRUE)
	{
		if (NULL != node->link[LEFT])
			mprof_tree_walk(node->link[LEFT]);
		crt_gbl(node, FALSE);
		loop_link = node->loop_link;
		while (NULL != loop_link)
		{
			crt_gbl(loop_link, TRUE);
			loop_link = loop_link->loop_link;
		}
		if (NULL != node->link[RIGHT])
			node = node->link[RIGHT];
		else
			break;
	}
	return;
}

/* Does a single left or right rotation, depending on the index passed. The diagram
 * of the rotation (for index == RIGHT) is presented below:
 *
 *       B                      D
 *      / \                    / \
 *     A   D      ==>         B   E
 *        / \                / \
 *       C   E              A   C
 */
STATICFNDEF mprof_tree *rotate_2(mprof_tree **path_top, int index)
{
	mprof_tree *B, *C, *D, *E;

	B = *path_top;
	D = B->link[index];
	C = D->link[BASE - index];
	E = D->link[index];
	*path_top = D;
	D->link[BASE - index] = B;
	B->link[index] = C;
	B->desc_dir = NEITHER;
	D->desc_dir = NEITHER;
	return E;
}

/* Does a double left or right rotation, depending on the index passed. The diagram
 * of the rotation (for index == RIGHT) is presented below:
 *
 *       B                            _D_
 *      / \                          /   \
 *     A   F                        B     F
 *        / \      ===>            / \   / \
 *       D   G                    A   C  E  G
 *      / \
 *     C   E
 */
STATICFNDEF mprof_tree *rotate_3(mprof_tree **path_top, int index, int third)
{
	mprof_tree *B, *C, *D, *E, *F;

	B = *path_top;
	F = B->link[index];
	D = F->link[BASE - index];
	/* nodes C and E can be NULL */
	C = D->link[BASE - index];
	E = D->link[index];
	*path_top = D;
	D->link[BASE - index] = B;
	D->link[index] = F;
	B->link[index] = C;
	F->link[BASE - index] = E;
	D->desc_dir = NEITHER;
	/* assume both subtrees are balanced */
	B->desc_dir = F->desc_dir = NEITHER;
	/* tree became balanced */
	if (third == NEITHER)
		return NULL;
	if (third == index)
	{	/* E holds the insertion so B is now unbalanced */
		B->desc_dir = BASE - index;
		return E;
	} else
	{	/* C holds the insertion so F is now unbalanced */
		F->desc_dir = index;
		return C;
	}
}

/* Compares the contents of the specified node with the information stored in arg.
 * Returns 0 on a match, 1 if arg was evaluated as "greater" than node, and -1 otherwise.
 */
STATICFNDEF int mprof_tree_compare(mprof_tree *node, trace_entry *arg)
{
	int diff;

	if (node->e.rout_name == arg->rout_name)
		diff = EQUAL;
	else
	{
		MIDENT_CMP(node->e.rout_name, arg->rout_name, diff);
		/* it's the same routine name, so they need not be different memory locations */
		if (EQUAL == diff)
			arg->rout_name = node->e.rout_name;
	}
	/* the routine names are the same */
	if (EQUAL == diff)
	{	/* pointers to the label name are the same */
		if (node->e.label_name == arg->label_name)
			diff = EQUAL;
		else
		{
			MIDENT_CMP(node->e.label_name, arg->label_name, diff);
			/* it's the same label name, so they need not be different memory locations */
			if (EQUAL == diff)
				arg->label_name = node->e.label_name;
		}
		/* the label names are the same */
		if (EQUAL == diff)
		{
			diff = arg->line_num - node->e.line_num;
			if (EQUAL != diff)
				diff = diff > EQUAL ? MORE : LESS;
		}
	}
	return diff;
}

/* Adjusts the descending directions for nodes in the path in which the node has just been added */
STATICFNDEF void mprof_tree_rebalance_path(mprof_tree *path, trace_entry *arg)
{
	int dir;

	/* Each node in path is currently balanced, so we will descend in the direction
	 * of the newly inserted node, marking every node as unbalanced in that direction
	 * along the way.
	 */
	while (path)
	{
		if (IS_INS_NODE == path->ins_path_hint)
			break;
		if ((NULL == path->link[LEFT]) || (NOT_IN_PATH == path->link[LEFT]->ins_path_hint))
			dir = RIGHT;
		else
			dir = LEFT;
		path->desc_dir = dir;
		path = path->link[dir];
	}
	return;
}

/* Determines at which point (if any) tree became unbalanced, and balances it. */
STATICFNDEF void mprof_tree_rebalance(mprof_tree **path_top, trace_entry *arg)
{
	mprof_tree *path;
	int first, second, third, diff;

	path = *path_top;
	/* entire tree is balanced, so only update desc_dir */
	if (NEITHER == path->desc_dir)
	{
		mprof_tree_rebalance_path(path, arg);
		return;
	}
	/* we still had the room for the new node, so only update desc_dir */
	first = EQUAL < mprof_tree_compare(path, arg) ? RIGHT : LEFT;
	if (path->desc_dir != first)
	{
		path->desc_dir = NEITHER;
		mprof_tree_rebalance_path(path->link[first], arg);
		return;
	}
	/* tree became unbalanced, but it is a simpler case, so we need a single rotation */
	second = EQUAL < mprof_tree_compare(path->link[first], arg) ? RIGHT : LEFT;
	if (first == second)
	{
		path = rotate_2(path_top, first);
		mprof_tree_rebalance_path(path, arg);
		return;
	}
	/* we will need a double rotation */
	path = path->link[first]->link[second];
	diff = mprof_tree_compare(path, arg);
	if (EQUAL == diff)
		third = NEITHER;			/* node we are interested in is the node we just added */
	else
		third = EQUAL < diff ? RIGHT : LEFT;	/* node was added in the sibling spot of the parent node */
	path = rotate_3(path_top, first, third);
	mprof_tree_rebalance_path(path, arg);
	return;
}

/* Attempts to find a node that matches arg's specification. If the node is not found, we
 * create one; in either case the pointer to the desired node is returned.
 */
mprof_tree  *mprof_tree_insert(mprof_tree **treep, trace_entry *arg)
{
	int 		diff, dir;
	mprof_tree 	*tree = *treep;
	mprof_tree 	**path_top = treep;

	tmp_rout_name = arg->rout_name;
	tmp_label_name = arg->label_name;
	while (tree)
	{
		tree->ins_path_hint = IN_PATH; /* this node is within the insertion path */
		diff = mprof_tree_compare(tree, arg);
		if (EQUAL == diff)
			break;
		if (NEITHER != tree->desc_dir)
			path_top = treep;
		dir = EQUAL < diff ? RIGHT : LEFT;
		treep = &tree->link[dir];
		if (NULL != tree->link[BASE - dir])
			tree->link[BASE - dir]->ins_path_hint = NOT_IN_PATH; /* the sibling node is not in the insertion path */
		tree = *treep;
	}
	/* did not find the node; will create one */
	if (NULL == tree)
	{
		tree = new_node(arg);
		tree->desc_dir = NEITHER;
		tree->ins_path_hint = IS_INS_NODE;
		*treep = tree;
		mprof_tree_rebalance(path_top, arg);
	} else
		mprof_reclaim_slots();
	/* If the rout_name or label_name already exists in the tree, it would have been changed to point to the reused
	 * name in the tree. Otherwise, if it doesn't already exist in the tree, new_node() would have created new copies
	 * and would have changed anyway.
	 */
	assert(tmp_rout_name != arg->rout_name);
	assert(tmp_label_name != arg->label_name);
	tmp_rout_name = tmp_label_name = NULL;
	return tree;
}
