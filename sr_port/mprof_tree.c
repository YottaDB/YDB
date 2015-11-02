/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "error.h"
#include "mprof.h"
#include "min_max.h" /* necessary for MIN which is necessary for MIDENT_CMP */

static mident	*tmp_rout_name, *tmp_label_name;

mprof_tree *new_node(trace_entry *arg)
{
	mprof_tree *tree;

	/* First, we need to pcalloc new space for the label/routine if we have not
	 * found an occurrence that is in the tree already */
	if (NULL == tmp_rout_name)
		tmp_rout_name = arg->rout_name;
	if (NULL != tmp_rout_name && tmp_rout_name == arg->rout_name)
	{
		arg->rout_name = (mident *) pcalloc(sizeof(mident));
		arg->rout_name->len = tmp_rout_name->len;
		arg->rout_name->addr = (char *) pcalloc(tmp_rout_name->len);
		memcpy(arg->rout_name->addr, tmp_rout_name->addr, tmp_rout_name->len);

	}
	if (NULL == tmp_label_name)
		tmp_label_name = arg->label_name;
	if (NULL != tmp_label_name && tmp_label_name == arg->label_name)
	{
		arg->label_name = (mident *) pcalloc(sizeof(mident));
		arg->label_name->len = tmp_label_name->len;
		arg->label_name->addr = (char *) pcalloc(tmp_label_name->len);
		memcpy(arg->label_name->addr, tmp_label_name->addr, tmp_label_name->len);
	}

        tree = (mprof_tree *)pcalloc(sizeof(mprof_tree));
	tree->e = *arg;
	tree->e.count = tree->e.usr_time = tree->e.sys_time = 0;
	tree->e.loop_level = tree->e.cur_loop_level = 0;
	tree->link[0] = tree->link[1] = tree->loop_link = (mprof_tree *)NULL;
	tree->bal = 0;
	tree->cache = 0;

	return tree;
}

#ifdef MPROF_DEBUGGING
#define	MPROF_DEBUGGING 1
#include "util.h"
void mprof_print_entryref(mident *rout, mident *lab, int line)
{
	if (rout->addr) util_out_print("(!AD, ", FALSE, rout->len, rout->addr);
	if (lab->addr) util_out_print("!AD, ", FALSE, lab->len, lab->addr);
	util_out_print("!SL) ", FALSE, line);
	util_out_print("[0x!8XL,0x!8XL]", FALSE, rout,lab);
}

void mprof_tree_print(mprof_tree *tree, int tabs, int longl)
{
	/*
	 * This is a debugging function. It MUST NOT be called for normal behaviour.
	 * It prints the mprof_tree, in a structured way. It does not print every field of the structure.
	 *
	 * longl:
	 * -1 means do not recurse the whole tree
	 * 1  means write the output indented appropriately
	 * 0  means do not indent
	 *
	 * tabs: shows the number of tabs to indent (i.e. level of root)
	 */
#define PRINTTABS       if (longl) for (x=0;x<tabs;x++) util_out_print("\t", FALSE)
#define PRINTNL         if (longl) util_out_print("", TRUE)
#define PRINTLB		PRINTNL; PRINTTABS; util_out_print("(", FALSE);
#define PRINTRB		PRINTNL; PRINTTABS; util_out_print(")", FALSE);
        mprof_tree 	*p = tree;
        mprof_tree 	*tmp_te;
        mprof_tree 	*tmp_tmp;
	int x = 0;


	if (p != NULL)
	{
		util_out_print("[", FALSE);
		mprof_print_entryref(p->e.rout_name, p->e.label_name, p->e.line_num);
		util_out_print("<c: !SL, fc: !SL, ll: !SL, cll: !SL>", FALSE, p->e.count, p->e.for_count,
				p->e.loop_level, p->e.cur_loop_level);
		tmp_tmp = p->loop_link;
		tmp_te = p->loop_link;
		while (tmp_te != '\0')
		{
			util_out_print("<one more: ", FALSE);
			util_out_print("!SL,!SL,!SL,!SL> ", FALSE, tmp_te->e.count, tmp_te->e.for_count,
					tmp_te->e.loop_level, tmp_te->e.cur_loop_level);
			tmp_te=tmp_te->loop_link;
		}
		if (0 > longl)
		{
			util_out_print("", TRUE);
			return;
		}
		PRINTLB;
		mprof_tree_print(p->link[0], tabs+1, longl);
		PRINTRB;
		PRINTLB;
		mprof_tree_print(p->link[1], tabs+1, longl);
		PRINTRB;
	}
	else
		util_out_print(".", FALSE);

}
#endif /* MPROF_DEBUGGING */

static mprof_tree 	*lastp, *topp;	/* for debugging segv chasing pointers */
static int 		mproftreeheight;	/* keep in static for debugging */

void mprof_tree_walk(mprof_tree *tree)
{
	mprof_tree *an[MAX_MPROF_TREE_HEIGHT]; 	/* Stack A: nodes */
	mprof_tree **ap = an;			/* Stack A: stack pointer */
	mprof_tree *p = tree->link[0];
	mprof_tree *tmp_p;

	error_def(ERR_MAXTRACEHEIGHT);

	assert(tree);
	mproftreeheight = 0;

	for ( ; ; )
	{
		while (NULL != p)
		{
			if (&an[MAX_MPROF_TREE_HEIGHT - 1] < ap)
			{
				if (!mproftreeheight)	/* first time */
					rts_error(VARLSTCNT(3) ERR_MAXTRACEHEIGHT, 1, MAX_MPROF_TREE_HEIGHT);
				mproftreeheight++;
				lastp = p;
				assert(FALSE);
				break;
			}

			*ap++ = p;
			p = p->link[0];
		}
		if (ap == an)
			return;
		p = *--ap;
		topp = p;
		crt_gbl(p, 0);
		tmp_p =  p->loop_link;
		if (0 != p->e.cur_loop_level)
			while (NULL != tmp_p)
			{
				lastp = tmp_p;
				tmp_p->e.loop_level +=  p->e.cur_loop_level;
				tmp_p =  tmp_p->loop_link;
			}

		tmp_p =  p->loop_link;
		if ((NULL != tmp_p) && (0 != tmp_p->e.loop_level))
		{
			lastp = tmp_p;
			crt_gbl(tmp_p, 1);
			tmp_p =  tmp_p->loop_link;
		}

		while ((NULL != tmp_p) && (0 != tmp_p->e.loop_level))
		{
			lastp = tmp_p;
			crt_gbl(tmp_p, 2);
			tmp_p = tmp_p->loop_link;
		}
		p = p->link[1];
	}
}

static mprof_tree *mprof_tree_find_node(mprof_tree *tree, trace_entry *arg)
{
	mprof_tree 	*t, *s, *p, *q, *r;
	int		diff;

	t = tree;
	s = p = t->link[0];

	if (s)
	{
		for ( ; ; )
		{
			if (p->e.rout_name == arg->rout_name)
				diff = 0;
			else
			{
				MIDENT_CMP(p->e.rout_name, arg->rout_name, diff);
				if (0 == diff)
				{ /* it's the same routine name, so they need not be different memory locations */
					arg->rout_name = p->e.rout_name;
				}
			}
			if (0 == diff)
			{
				if (p->e.label_name == arg->label_name)
					diff = 0;
				else
				{
					MIDENT_CMP(p->e.label_name, arg->label_name, diff);
					if (0 == diff)
					{ /* it's the same label name, so they need not be different memory locations */
						arg->label_name = p->e.label_name;
					}
				}
				if (0 == diff)
					diff = arg->line_num - p->e.line_num;
			}

			if (0 > diff)
			{
				p->cache = 0;
				q = p->link[0];
				if (!q)
				{
					p->link[0] = q = new_node(arg);
					break;
				}
			}
			else if (0 < diff)
			{
				p->cache = 1;
				q = p->link[1];
				if (!q)
				{
					p->link[1] = q = new_node(arg);
					break;
				}
			}
			else
				return p;
			if (0 != q->bal)
				t = p, s = q;
			p = q;
		}
		r = p = s->link[s->cache];
		while (p != q)
		{
			p->bal = (p->cache*2) - 1;
			p = p->link[p->cache];
		}
		if (0 == s->cache)
		{
			if (0 == s->bal)
			{
				s->bal = -1;
				return q;
			}
			else if (1 == s->bal)
			{
				s->bal = 0;
				return q;
			}
			assert (-1 == s->bal);
			if (-1 == r->bal)
			{
				p = r;
				s->link[0] = r->link[1];
				r->link[1] = s;
				s->bal = r->bal = 0;
			}
			else
			{
				assert(r->bal == 1);
				p = r->link[1];
				r->link[1] = p->link[0];
				p->link[0] = r;
				s->link[0] = p->link[1];
				p->link[1] = s;
				if (-1 == p->bal)
					s->bal = 1, r->bal = 0;
				else if (0 == p->bal)
					s->bal = r->bal = 0;
				else
				{
					assert(1 == p->bal);
					s->bal = 0, r->bal = -1;
				}
				p->bal = 0;
			}
		}
		else
		{
      			if (0 == s->bal)
        		{
          			s->bal = 1;
          			return q;
        		}
      			else if (-1 == s->bal)
        		{
          			s->bal = 0;
          			return q;
        		}

      			assert (1 == s->bal);
      			if (1 == r->bal)
        		{
          			p = r;
          			s->link[1] = r->link[0];
          			r->link[0] = s;
          			s->bal = r->bal = 0;
        			}
      			else
        		{
				assert (r->bal == -1);
          			p = r->link[0];
          			r->link[0] = p->link[1];
          			p->link[1] = r;
          			s->link[1] = p->link[0];
          			p->link[0] = s;
          			if (1 == p->bal)
            				s->bal = -1, r->bal = 0;
          			else if (0 == p->bal)
            				s->bal = r->bal = 0;
          			else
            			{
              				assert (-1 == p->bal);
              				s->bal = 0, r->bal = 1;
            			}
          			p->bal = 0;
			}
		}
		if (t != tree && s == t->link[1])
			t->link[1] = p;
		else
			t->link[0] = p;
	}
	else
	{
		q = t->link[0] = new_node(arg);
		q->link[0] = q->link[1] = NULL;
		q->bal = 0;
	}

	return q;
}

mprof_tree  *mprof_tree_insert(mprof_tree *tree, trace_entry *arg)
{
	mprof_tree *p;

	tmp_rout_name = arg->rout_name;
	tmp_label_name = arg->label_name;
	p = mprof_tree_find_node(tree, arg);

	/* If the rout_name or label_name already exists in the tree, it would have been changed to point to the reused
	 * name in the tree. Otherwise, if it doesn't already exist in the tree, new_node() would have created new copies
	 * and would have changed anyway. */
	assert(tmp_rout_name != arg->rout_name);
	assert(tmp_label_name != arg->label_name);
	tmp_rout_name = tmp_label_name = NULL;
	return p;
}
