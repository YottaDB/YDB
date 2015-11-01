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
#include "gtm_stdio.h"
#include "mprof.h"

struct mprof_tree *new_node(struct trace_entry arg)
{
	struct mprof_tree *tree;

        tree = (struct mprof_tree *)pcalloc(sizeof(struct mprof_tree));
	tree->e = arg;
	tree->e.count = tree->e.usr_time = tree->e.sys_time = 0;
	tree->e.loop_level = tree->e.cur_loop_level = 0;
	tree->link[0] = tree->link[1] = tree->loop_link = (struct mprof_tree *)NULL;
	tree->bal = 0;
	tree->cache = 0;

	return tree;
}

void mprof_tree_print(struct mprof_tree *tree,int tabs,int longl)
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
#define PRINTTABS       if (longl) for (x=0;x<tabs;x++) printf("\t")
#define PRINTNL         if (longl) printf("\n")
#define PRINTLB		PRINTNL; PRINTTABS; printf("(");
#define PRINTRB		PRINTNL; PRINTTABS; printf(")");
        struct mprof_tree *an[MAX_MPROF_TREE_HEIGHT];   /* Stack A: nodes */
        struct mprof_tree **ap = an;                    /* Stack A: stack pointer */
        struct mprof_tree *p = tree;
        struct mprof_tree *tmp_te;
        struct mprof_tree *tmp_tmp;
        char str[100];
        int x = 0 ;


        {
                if (p != NULL)
                {
                        printf("[%s,%s,%s,",p->e.rout_name,p->e.label_name,p->e.line_num);
			printf("%d,%d,%d",p->e.count,p->e.loop_level,p->e.cur_loop_level);
                        printf("[%d]]",p->e.for_count);
                        printf("{%d,%d}",p->e.usr_time,p->e.sys_time);/**/
                        tmp_tmp = p->loop_link;
                        tmp_te = p->loop_link;
                        while (tmp_te != '\0')
                        {
                                printf("one more");
                                printf("<%d,%d,",tmp_te->e.count,tmp_te->e.loop_level);
				printf("%d,[%d]>",tmp_te->e.cur_loop_level,tmp_te->e.for_count);
                                tmp_te=tmp_te->loop_link;
                        }
			if (0 > longl)
			{
				printf("\n");
				return;
			}
			PRINTLB;
                        mprof_tree_print(p->link[0],tabs+1,longl);
                        PRINTRB;
                        PRINTLB;
                        mprof_tree_print(p->link[1],tabs+1,longl);
                        PRINTRB;
                }
                else
                {
                printf(".");
                }
        }
}

void mprof_tree_walk(struct mprof_tree *tree)
{
	struct mprof_tree *an[MAX_MPROF_TREE_HEIGHT]; 	/* Stack A: nodes */
	struct mprof_tree **ap = an;			/* Stack A: stack pointer */
	struct mprof_tree *p = tree->link[0];
	struct mprof_tree *tmp_p;

	assert(tree);

	for ( ; ; )
	{
		while (p != NULL)
		{
			*ap++ = p;
			p = p->link[0];
		}
		if (ap == an)
			return;
		p = *--ap;
		crt_gbl(p, 0);
		tmp_p =  p->loop_link;
		if (0 != p->e.cur_loop_level)
			while (NULL != tmp_p)
			{
				tmp_p->e.loop_level +=  p->e.cur_loop_level;
				tmp_p =  tmp_p->loop_link;
			}

		tmp_p =  p->loop_link;
		if ((NULL != tmp_p) && (0 != tmp_p->e.loop_level))
		{
			crt_gbl(tmp_p, 1);
			tmp_p =  tmp_p->loop_link;
		}

		while ((NULL != tmp_p) && (0 != tmp_p->e.loop_level))
		{
			crt_gbl(tmp_p, 2);
			tmp_p = tmp_p->loop_link;
		}
		p = p->link[1];
	}
}

void *mprof_tree_find_node(struct mprof_tree *tree, struct trace_entry arg)
{
	struct mprof_tree 	*t, *s, *p, *q, *r;
	int			diff;

	t = tree;
	s = p = t->link[0];

	if (s)
	{
		for ( ; ; )
		{
			diff = memcmp((p)->e.rout_name, arg.rout_name, strlen((char *)arg.rout_name)+1);
			if (diff == 0)
			{
				diff = memcmp((p)->e.label_name, arg.label_name, strlen((char *)arg.label_name)+1);
				if (diff == 0)
					diff = memcmp((p)->e.line_num, arg.line_num, strlen((char *)arg.line_num)+1);
			}

			if (diff < 0)
			{
				p->cache = 0;
				q = p->link[0];
				if (!q)
				{
					p->link[0] = q = new_node(arg);
					break;
				}
			}
			else if (diff > 0)
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
			if (q->bal != 0)
				t = p, s = q;
			p = q;
		}
		r = p = s->link[s->cache];
		while (p != q)
		{
			p->bal = (p->cache*2) - 1;
			p = p->link[p->cache];
		}
		if (s->cache == 0)
		{
			if (s->bal == 0)
			{
				s->bal = -1;
				return q;
			}
			else if (s->bal == 1)
			{
				s->bal = 0;
				return q;
			}
			assert (s->bal == -1);
			if (r->bal == -1)
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
				if (p->bal == -1)
					s->bal = 1, r->bal = 0;
				else if (p->bal == 0)
					s->bal = r->bal = 0;
				else
				{
					assert(p->bal == 1);
					s->bal = 0, r->bal = -1;
				}
				p->bal = 0;
			}
		}
		else
		{
      			if (s->bal == 0)
        		{
          			s->bal = 1;
          			return q;
        		}
      			else if (s->bal == -1)
        		{
          			s->bal = 0;
          			return q;
        		}

      			assert (s->bal == 1);
      			if (r->bal == 1)
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
          			if (p->bal == +1)
            				s->bal = -1, r->bal = 0;
          			else if (p->bal == 0)
            				s->bal = r->bal = 0;
          			else
            			{
              				assert (p->bal == -1);
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

struct mprof_tree  *mprof_tree_insert(struct mprof_tree *tree, struct trace_entry arg)
{
	struct mprof_tree *p;

	return (p = mprof_tree_find_node(tree, arg));
}
