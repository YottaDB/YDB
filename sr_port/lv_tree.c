/****************************************************************
 *								*
 *	Copyright 2011, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <stddef.h> /* for OFFSETOF macro used in the IS_OFFSET_AND_SIZE_MATCH macro */

#include "collseq.h"
#include "subscript.h"
#include "lv_tree.h"
#include "gtm_stdio.h"
#include "min_max.h"
#include "lv_val.h"
#include "numcmp.h"
#include "arit.h"
#include "promodemo.h"	/* for "promote" & "demote" prototype */
#include "gtmio.h"
#include "have_crit.h"

#define	LV_TREE_INIT_ALLOC	 	 4
#define	LV_TREENODE_INIT_ALLOC		16
#define	LV_TREESUBSCR_INIT_ALLOC	16

/* the below assumes TREE_LEFT_HEAVY is 0, TREE_RIGHT_HEAVY is 1 and TREE_BALANCED is 2 */
LITDEF	int		tree_balance_invert[] = {TREE_RIGHT_HEAVY, TREE_LEFT_HEAVY, TREE_BALANCED};

LITREF	mval		literal_null;
LITREF	int4		ten_pwr[NUM_DEC_DG_1L+1];

STATICFNDCL void lvAvlTreeNodeFltConv(lvTreeNodeNum *fltNode);
STATICFNDCL lvTreeNode *lvAvlTreeSingleRotation(lvTreeNode *rebalanceNode, lvTreeNode *anchorNode, int4 balanceFactor);
STATICFNDCL lvTreeNode *lvAvlTreeDoubleRotation(lvTreeNode *rebalanceNode, lvTreeNode *anchorNode, int4 balanceFactor);
STATICFNDCL boolean_t lvAvlTreeLookupKeyCheck(treeKeySubscr *key);
STATICFNDCL int lvAvlTreeNodeHeight(lvTreeNode *node);
STATICFNDCL boolean_t lvAvlTreeNodeIsWellFormed(lvTree *lvt, lvTreeNode *node);

STATICFNDEF void lvAvlTreeNodeFltConv(lvTreeNodeNum *fltNode)
{
	int		int_m1, int_exp;
	const int4	*pwr;
	DEBUG_ONLY(int	key_mvtype;)

	DEBUG_ONLY(key_mvtype = fltNode->key_mvtype;)
	assert(MV_INT & key_mvtype);
	assert(!fltNode->key_flags.key_bits.key_iconv);
	int_m1 = fltNode->key_m0;
	if (int_m1)
	{
		/* The following logic is very similar to that in promodemo.c (promote function).
		 * That operates on mvals whereas this does not hence the duplication inspite of similar logic.
		 */
		if (0 < int_m1)
			fltNode->key_flags.key_bits.key_sgn = 0;
		else
		{
			fltNode->key_flags.key_bits.key_sgn = 1;
			int_m1 = -int_m1;
		}
		int_exp = 0;
		pwr = &ten_pwr[0];
		while (int_m1 >= *pwr)
		{
			int_exp++;
			pwr++;
			assert(pwr < ARRAYTOP(ten_pwr));
		}
		fltNode->key_m1 = int_m1 * ten_pwr[NUM_DEC_DG_1L - int_exp] ;
		int_exp += EXP_INT_UNDERF;
		fltNode->key_flags.key_bits.key_e = int_exp;
	} else
		fltNode->key_flags.key_bits.key_e = 0;	/* integer 0 will have exponent 0 even after float promotion */
	fltNode->key_flags.key_bits.key_iconv = TRUE;
	return;
}

/* All the below comparison macros & functions return
 *	= 0 if equal;
 *	< 0 if key of aSUBSCR < key of bNODE;
 *	> 0 if key of aSUBSCR > key of bNODE
 * The 3 LV_AVL_TREE_*KEY_CMP macros below have very similar (yet not completely identical) code.
 * They are kept separate because this is frequently used code and we want to avoid if checks as much as possible.
 * Having separate macros for each keytype lets us reduce the # of if checks we need inside the macro.
 */

#define	LV_AVL_TREE_INTKEY_CMP(aSUBSCR, aSUBSCR_M1, bNODE, retVAL)							\
{															\
	int		a_m1, b_mvtype, a_sgn, b_sgn, exp_diff, m1_diff, m0_diff, b_m0;					\
	lvTreeNodeNum	*bNodeFlt;											\
	treeKeySubscr	fltSubscr, *tmpASubscr;										\
															\
	/* aSUBSCR is a number */											\
	assert(TREE_KEY_SUBSCR_IS_CANONICAL((aSUBSCR)->mvtype));							\
	assert(MVTYPE_IS_INT((aSUBSCR)->mvtype));									\
	b_mvtype = (bNODE)->key_mvtype;											\
	assert(!TREE_KEY_SUBSCR_IS_CANONICAL(b_mvtype)									\
		|| (MVTYPE_IS_NUMERIC(b_mvtype) && !MVTYPE_IS_NUM_APPROX(b_mvtype)));					\
	assert(MVTYPE_IS_NUMERIC((aSUBSCR)->mvtype) && !MVTYPE_IS_NUM_APPROX((aSUBSCR)->mvtype));			\
	assert((aSUBSCR_M1) == (aSUBSCR)->m[1]);									\
	if (MV_INT & b_mvtype)												\
	{														\
		bNodeFlt = (lvTreeNodeNum *)(bNODE);									\
		(retVAL) = (aSUBSCR_M1) - bNodeFlt->key_m0;								\
	} else if (TREE_KEY_SUBSCR_IS_CANONICAL(b_mvtype))								\
	{	/* Both aSUBSCR and bNODE are numbers */								\
		b_mvtype &= MV_INT;											\
		/* aSUBSCR is MV_INT but bNODE is not. Promote aSubscr to float representation before comparing		\
		 * it with bNODE. Cannot touch input mval since non-lv code expects MV_INT value to be stored in	\
		 * integer format only (and not float format). So use a temporary mval for comparison.			\
		 */													\
		fltSubscr.m[1] = (aSUBSCR_M1);										\
		tmpASubscr = &fltSubscr;										\
		promote(tmpASubscr);	/* Note this modifies tmpASubscr->m[1] so no longer can use aSUBSCR_M1 below */	\
		assert(tmpASubscr->e || !tmpASubscr->m[1]);								\
		bNodeFlt = (lvTreeNodeNum *)(bNODE);									\
		if (b_mvtype && !bNodeFlt->key_flags.key_bits.key_iconv)						\
		{													\
			lvAvlTreeNodeFltConv(bNodeFlt);									\
			assert(bNodeFlt->key_flags.key_bits.key_iconv);							\
		}													\
		a_sgn = (0 == tmpASubscr->sgn) ? 1 : -1; /* 1 if positive, -1 if negative */				\
		b_sgn = (0 == bNodeFlt->key_flags.key_bits.key_sgn) ? 1 : -1;						\
		/* Check sign. If different we are done right there */							\
		if (a_sgn != b_sgn)											\
			(retVAL) = a_sgn;										\
		else													\
		{	/* Signs equal; compare exponents for magnitude and adjust sense depending on sign. */		\
			exp_diff = tmpASubscr->e - bNodeFlt->key_flags.key_bits.key_e;					\
			if (exp_diff)											\
				(retVAL) = (exp_diff * a_sgn);								\
			else												\
			{	/* Signs and exponents equal; compare magnitudes.  */					\
				/* First, compare high-order 9 digits of magnitude.  */					\
				a_m1 = tmpASubscr->m[1];								\
				m1_diff = a_m1 - bNodeFlt->key_m1;							\
				if (m1_diff)										\
					(retVAL) = (m1_diff * a_sgn);							\
				else											\
				{	/* High-order 9 digits equal; if not zero, compare low-order 9 digits.  */	\
					if (0 == a_m1)	/* zero special case */						\
						(retVAL) = 0;								\
					else										\
					{										\
						b_m0 = (b_mvtype ? 0 : bNodeFlt->key_m0);				\
						m0_diff = tmpASubscr->m[0] - b_m0;					\
						if (m0_diff)								\
							(retVAL) = (m0_diff * a_sgn);					\
						else									\
						{	/* Signs, exponents, high-order & low-order magnitudes equal */	\
							(retVAL) = 0;							\
						}									\
					}										\
				}											\
			}												\
		}													\
	} else														\
		(retVAL) = -1; /* aSubscr is a number, but bNODE is a string */						\
}

#define	LV_AVL_TREE_NUMKEY_CMP(aSUBSCR, bNODE, retVAL)									\
{															\
	int		b_mvtype, a_sgn, b_sgn, exp_diff, m1_diff, m0_diff, a_m1, b_m0;					\
	lvTreeNodeNum	*bNodeFlt;											\
															\
	/* aSUBSCR is a number */											\
	assert(TREE_KEY_SUBSCR_IS_CANONICAL((aSUBSCR)->mvtype));							\
	assert(!MVTYPE_IS_INT((aSUBSCR)->mvtype));									\
	b_mvtype = (bNODE)->key_mvtype;											\
	assert(!TREE_KEY_SUBSCR_IS_CANONICAL(b_mvtype)									\
		|| (MVTYPE_IS_NUMERIC(b_mvtype) && !MVTYPE_IS_NUM_APPROX(b_mvtype)));					\
	assert(MVTYPE_IS_NUMERIC((aSUBSCR)->mvtype) && !MVTYPE_IS_NUM_APPROX((aSUBSCR)->mvtype));			\
	if (TREE_KEY_SUBSCR_IS_CANONICAL(b_mvtype))									\
	{	/* Both aSUBSCR and bNODE are numbers */								\
		b_mvtype &= MV_INT;											\
		bNodeFlt = (lvTreeNodeNum *)(bNODE);									\
		if (b_mvtype && !bNodeFlt->key_flags.key_bits.key_iconv)						\
		{													\
			lvAvlTreeNodeFltConv(bNodeFlt);									\
			assert(bNodeFlt->key_flags.key_bits.key_iconv);							\
		}													\
		a_sgn = (0 == (aSUBSCR)->sgn) ? 1 : -1; /* 1 if positive, -1 if negative */				\
		b_sgn = (0 == bNodeFlt->key_flags.key_bits.key_sgn) ? 1 : -1;						\
		/* Check sign. If different we are done right there */							\
		if (a_sgn != b_sgn)											\
			(retVAL) = a_sgn;										\
		else													\
		{	/* Signs equal; compare exponents for magnitude and adjust sense depending on sign. */		\
			exp_diff = (aSUBSCR)->e - bNodeFlt->key_flags.key_bits.key_e;					\
			if (exp_diff)											\
				(retVAL) = (exp_diff * a_sgn);								\
			else												\
			{	/* Signs and exponents equal; compare magnitudes.  */					\
				/* First, compare high-order 9 digits of magnitude.  */					\
				a_m1 = (aSUBSCR)->m[1];									\
				m1_diff = a_m1 - bNodeFlt->key_m1;							\
				if (m1_diff)										\
					(retVAL) = (m1_diff * a_sgn);							\
				else											\
				{	/* High-order 9 digits equal; if not zero, compare low-order 9 digits.  */	\
					if (0 == a_m1)	/* zero special case */						\
						(retVAL) = 0;								\
					else										\
					{										\
						b_m0 = (b_mvtype ? 0 : bNodeFlt->key_m0);				\
						m0_diff = (aSUBSCR)->m[0] - b_m0;					\
						if (m0_diff)								\
							(retVAL) = (m0_diff * a_sgn);					\
						else									\
						{	/* Signs, exponents, high-order & low-order magnitudes equal */	\
							(retVAL) = 0;							\
						}									\
					}										\
				}											\
			}												\
		}													\
	} else														\
		(retVAL) = -1; /* aSUBSCR is a number, but bNODE is a string */						\
}

#define	LV_AVL_TREE_STRKEY_CMP(aSUBSCR, aSUBSCR_ADDR, aSUBSCR_LEN, bNODE, retVAL)			\
{													\
	/* aSUBSCR is a string */									\
	assert(!TREE_KEY_SUBSCR_IS_CANONICAL((aSUBSCR)->mvtype));					\
	assert(MVTYPE_IS_STRING((aSUBSCR)->mvtype));							\
	if (TREE_KEY_SUBSCR_IS_CANONICAL((bNODE)->key_mvtype))						\
		(retVAL) = 1;	/* aSUBSCR is a string, but bNODE is a number */			\
	else /* aSUBSCR and bNODE are both strings */							\
		MEMVCMP(aSUBSCR_ADDR, aSUBSCR_LEN, (bNODE)->key_addr, (bNODE)->key_len, (retVAL));	\
}

#ifdef DEBUG
/* This function is currently DEBUG-only because no one is supposed to be calling it.
 * Callers are usually performance sensitive and should therefore use the LV_AVL_TREE_NUMKEY_CMP
 * or LV_AVL_TREE_STRKEY_CMP macros that this function in turn invokes.
 */
int lvAvlTreeKeySubscrCmp(treeKeySubscr *aSubscr, lvTreeNode *bNode)
{
	int		retVal, a_mvtype;

	a_mvtype = aSubscr->mvtype;
	if (TREE_KEY_SUBSCR_IS_CANONICAL(a_mvtype))
	{
		if (MVTYPE_IS_INT(a_mvtype))
		{
			LV_AVL_TREE_INTKEY_CMP(aSubscr, aSubscr->m[1], bNode, retVal);
		} else
			LV_AVL_TREE_NUMKEY_CMP(aSubscr, bNode, retVal);
	} else
		LV_AVL_TREE_STRKEY_CMP(aSubscr, aSubscr->str.addr, aSubscr->str.len, bNode, retVal);
	return retVal;
}

/* This function is currently coded not as efficiently as it needs to be because it is only invoked by dbg code.
 * Hence the definition inside a #ifdef DEBUG. If this change and pro code needs it, the below code needs to be revisited.
 */
int lvAvlTreeNodeSubscrCmp(lvTreeNode *aNode, lvTreeNode *bNode)
{
	int		a_mvtype, b_mvtype, retVal;
	int		a_sgn, b_sgn, exp_diff, m1_diff, m0_diff, a_m1, b_m1, a_m0, b_m0;
	lvTreeNodeNum	*aNodeFlt, *bNodeFlt;

	a_mvtype = aNode->key_mvtype;
	b_mvtype = bNode->key_mvtype;
	if (TREE_KEY_SUBSCR_IS_CANONICAL(a_mvtype))
	{	/* aSubscr is a number */
		assert(!MVTYPE_IS_STRING(a_mvtype));
		if (TREE_KEY_SUBSCR_IS_CANONICAL(b_mvtype))
		{	/* Both aNode and bNode are numbers */
			assert(!MVTYPE_IS_STRING(b_mvtype));
			aNodeFlt = (lvTreeNodeNum *)aNode;
			bNodeFlt = (lvTreeNodeNum *)bNode;
			a_mvtype = a_mvtype & MV_INT;
			if (a_mvtype & b_mvtype)
			{	/* Both aNode & bNode are integers */
				a_m1 = aNodeFlt->key_m0;
				b_m1 = bNodeFlt->key_m0;
				return (a_m1 - b_m1);
			}
			if (a_mvtype)
			{
				if (!aNodeFlt->key_flags.key_bits.key_iconv)
					lvAvlTreeNodeFltConv(aNodeFlt);
				assert(aNodeFlt->key_flags.key_bits.key_iconv);
			} else if (b_mvtype = (MV_INT & b_mvtype))
			{
				if (!bNodeFlt->key_flags.key_bits.key_iconv)
					lvAvlTreeNodeFltConv(bNodeFlt);
				assert(bNodeFlt->key_flags.key_bits.key_iconv);
			}
			a_sgn = (0 == aNodeFlt->key_flags.key_bits.key_sgn) ? 1 : -1; /* 1 if positive, -1 if negative */
			b_sgn = (0 == bNodeFlt->key_flags.key_bits.key_sgn) ? 1 : -1;
			/* Check sign. If different we are done right there */
			if (a_sgn != b_sgn)
				return a_sgn;
			/* Signs equal; compare exponents for magnitude and adjust sense depending on sign. */
			exp_diff = aNodeFlt->key_flags.key_bits.key_e - bNodeFlt->key_flags.key_bits.key_e;
			if (exp_diff)
				return (exp_diff * a_sgn);
			/* Signs and exponents equal; compare magnitudes.  */
			/* First, compare high-order 9 digits of magnitude.  */
			a_m1 = aNodeFlt->key_m1;
			m1_diff = a_m1 - bNodeFlt->key_m1;
			if (m1_diff)
				return (m1_diff * a_sgn);
			/* High-order 9 digits equal; if not zero, compare low-order 9 digits.  */
			if (0 == a_m1)	/* zero special case */
				return 0;
			/* If key_iconv is TRUE, key_m0 is not lower-order 9 digits but instead is MV_INT representation */
			a_m0 = (a_mvtype && aNodeFlt->key_flags.key_bits.key_iconv) ? 0 : aNodeFlt->key_m0;
			b_m0 = (b_mvtype && bNodeFlt->key_flags.key_bits.key_iconv) ? 0 : bNodeFlt->key_m0;
			m0_diff = a_m0 - b_m0;
			if (m0_diff)
				return (m0_diff * a_sgn);
			/* Signs, exponents, high-order magnitudes, and low-order magnitudes equal.  */
			return 0;
		}
		/* aNode is a number, but bNode is a string */
		return(-1);
	}
	/* aNode is a string */
	assert(MVTYPE_IS_STRING(a_mvtype));
	if (TREE_KEY_SUBSCR_IS_CANONICAL(b_mvtype))
	{
		assert(!MVTYPE_IS_STRING(b_mvtype));
		return(1);	/* aNode is a string, but bNode is a number */
	}
	/* aNode and bNode are both strings */
	assert(MVTYPE_IS_STRING(b_mvtype));
	MEMVCMP(aNode->key_addr, aNode->key_len, bNode->key_addr, bNode->key_len, retVal);
	return retVal;
}
#endif

/* Return first in-order traversal node (also smallest collating key) in avl tree. Returns NULL if NO key in tree */
lvTreeNode *lvAvlTreeFirst(lvTree *lvt)
{
	lvTreeNode	*avl_first, *next;

	next = lvt->avl_root;
	if (NULL == next)
		return NULL;
	do
	{
		avl_first = next;
		next = next->avl_left;
	} while (NULL != next);
	return avl_first;
}

/* Return last (highest collating) key in avl tree. Returns NULL if NO key in tree */
lvTreeNode *lvAvlTreeLast(lvTree *lvt)
{
	lvTreeNode	*avl_last, *next;

	next = lvt->avl_root;
	if (NULL == next)
		return NULL;
	do
	{
		avl_last = next;
		next = next->avl_right;
	} while (NULL != next);
	return avl_last;
}

/* Returns the in-order predecessor of the input "node".
 * Assumes input is in the avl tree  and operates within the avl tree only.
 */
lvTreeNode *lvAvlTreePrev(lvTreeNode *node)
{
	lvTreeNode	*prev, *tmp, *parent;

	assert(NULL != node);
	prev = node->avl_left;
	if (NULL != prev)
	{	/* in-order predecessor is BELOW "node" */
		tmp = prev->avl_right;
		while (NULL != tmp)
		{
			prev = tmp;
			tmp  = prev->avl_right;
		}
	} else
	{	/* in-order predecessor is an ancestor of "node" or could be "" (because "node" is the BIGGEST key in tree) */
		parent = node->avl_parent;
		tmp = node;
		while (NULL != parent)
		{
			if (parent->avl_right == tmp)
			{
				prev = parent;
				break;
			}
			assert(parent->avl_left == tmp);
			tmp = parent;
			parent = tmp->avl_parent;
		}
	}
	return prev;
}

/* Return "node" in AVL tree IMMEDIATELY BEFORE input "key" */
lvTreeNode *lvAvlTreeKeyPrev(lvTree *lvt, treeKeySubscr *key)
{
	lvTreeNode	*node, *tmp, *prev, *parent;

	node = lvAvlTreeLookup(lvt, key, &parent);
	if (NULL != node)	/* most common case */
	{
		prev = lvAvlTreePrev(node);
		assert((NULL == prev) || (0 < lvAvlTreeKeySubscrCmp(key, prev)));
	} else
	{
		assert(NULL != parent);	/* lvAvlTreeLookup should have initialized this */
		if (TREE_DESCEND_LEFT == parent->descent_dir)
		{
			assert(NULL == parent->avl_left);
			assert(0 > lvAvlTreeKeySubscrCmp(key, parent));
			prev = lvAvlTreePrev(parent);
			assert((NULL == prev) || (0 < lvAvlTreeKeySubscrCmp(key, prev)));
		} else
		{
			assert(NULL == parent->avl_right);
			assert(0 < lvAvlTreeKeySubscrCmp(key, parent));
			DEBUG_ONLY(tmp = lvAvlTreeNext(parent);)
			assert((NULL == tmp) || (0 > lvAvlTreeKeySubscrCmp(key, tmp)));
			prev = parent;
		}
	}
	return prev;
}

/* Returns the in-order successor of the input "node".
 * Assumes input is in the avl tree and operates within the avl tree.
 */
lvTreeNode *lvAvlTreeNext(lvTreeNode *node)
{
	lvTreeNode	*next, *tmp, *parent;
	DEBUG_ONLY(lvTree	*lvt;)

	assert(NULL != node);
	next = node->avl_right;
	if (NULL != next)
	{	/* in-order successor is BELOW "node" */
		tmp = next->avl_left;
		while (NULL != tmp)
		{
			next = tmp;
			tmp  = next->avl_left;
		}
	} else
	{	/* in-order successor is an ancestor of "node" or could be "" (because "node" is the BIGGEST key in tree) */
		parent = node->avl_parent;
		tmp = node;
		while (NULL != parent)
		{
			if (parent->avl_left == tmp)
			{
				next = parent;
				break;
			}
			assert(parent->avl_right == tmp);
			tmp = parent;
			parent = tmp->avl_parent;
		}
	}
	assert((NULL == next) || (node == lvAvlTreePrev(next)));
	return next;
}

/* Return "node" in AVL tree IMMEDIATELY AFTER input "key" */
lvTreeNode *lvAvlTreeKeyNext(lvTree *lvt, treeKeySubscr *key)
{
	lvTreeNode	*node, *tmp, *next, *parent;

	node = lvAvlTreeLookup(lvt, key, &parent);
	if (NULL != node)	/* most common case */
	{
		next = lvAvlTreeNext(node);
		assert((NULL == next) || (0 > lvAvlTreeKeySubscrCmp(key, next)));
	} else
	{
		assert(NULL != parent);	/* lvAvlTreeLookup should have initialized this */
		if (TREE_DESCEND_RIGHT == parent->descent_dir)
		{
			assert(NULL == parent->avl_right);
			assert(0 < lvAvlTreeKeySubscrCmp(key, parent));
			next = lvAvlTreeNext(parent);
			assert((NULL == next) || (0 > lvAvlTreeKeySubscrCmp(key, next)));
		} else
		{
			assert(NULL == parent->avl_left);
			assert(0 > lvAvlTreeKeySubscrCmp(key, parent));
			DEBUG_ONLY(tmp = lvAvlTreePrev(parent);)
			assert((NULL == tmp) || (0 < lvAvlTreeKeySubscrCmp(key, tmp)));
			next = parent;
		}
	}
	return next;
}

/* Return first post-order traversal node in avl tree. Returns NULL if NO key in tree */
lvTreeNode *lvAvlTreeFirstPostOrder(lvTree *lvt)
{
	lvTreeNode	*first, *tmp;

	first = lvt->avl_root;
	if (NULL == first)
		return NULL;
	/* Look for first post-order traversal node under left subtree if any. If not look under right sub-tree.
	 * If neither is present, then this node is it.
	 */
	do
	{
		if ((NULL != (tmp = first->avl_left)) || (NULL != (tmp = first->avl_right)))
			first = tmp;	/* need to go further down the tree to find post-order successor */
		else
			break;	/* found leaf-level post-order successor */
	} while (TRUE);
	return first;
}

/* Returns the post-order successor of the input "node". Assumes input is in the avl tree and operates within the avl tree. */
lvTreeNode *lvAvlTreeNextPostOrder(lvTreeNode *node)
{
	lvTreeNode		*next, *tmp, *parent;

	assert(NULL != node);
	parent = node->avl_parent;
	if (NULL == parent)	/* Reached root node of the AVL tree in the post order traversal */
		return NULL;	/* Done with post-order traversal */
	if (node == parent->avl_right)	/* done with "node" and its subtree so post-order successor is "parent" */
		return parent;
	assert(node == parent->avl_left);
	/* Post-order successor will be in the right subtree of "parent" */
	next = parent->avl_right;
	if (NULL == next)
		return parent; /* No right subtree so "parent" it is */
	/* Find post-order successor under the right subtree. Use logic similar to "lvAvlTreeFirstPostOrder" */
	do
	{
		if ((NULL != (tmp = next->avl_left)) || (NULL != (tmp = next->avl_right)))
			next = tmp;	/* need to go further down the tree to find post-order successor */
		else
			break;	/* found leaf-level post-order successor */
	} while (TRUE);
	return next;
}

/* Returns the collated successor of the input "key" taking into account the currently effective null collation scheme */
lvTreeNode *lvAvlTreeKeyCollatedNext(lvTree *lvt, treeKeySubscr *key)
{
	lvTreeNode	*node;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(local_collseq_stdnull))
	{
		if (MV_IS_STRING(key) && (0 == key->str.len))
		{	/* want the subscript AFTER the null subscript. That is the FIRST node in the tree
			 * unless the first one happens to also be the null subscript. In that case, we need
			 * to hop over that and get the next node in the tree.
			 */
			node = lvAvlTreeFirst(lvt);
		} else
		{	/* want the subscript AFTER a numeric or string subscript. It could end up resulting
			 * in the null subscript in case "key" is the highest numeric subscript in the tree.
			 * In that case, hop over the null subscript as that comes first in the collation order
			 * even though it comes in between numbers and strings in the tree node storage order.
			 */
			node = lvAvlTreeKeyNext(lvt, key);
		}
		/* If "node" holds the NULL subscript, then hop over to the next one.  */
		if ((NULL != node) && MVTYPE_IS_STRING(node->key_mvtype) && (0 == node->key_len))
			node = lvAvlTreeNext(node);	/* Need to hop over the null subscript */
	} else
		node = lvAvlTreeKeyNext(lvt, key);
	return node;
}

/* Returns the collated successor of the input "node" taking into account the currently effective null collation scheme */
lvTreeNode *lvAvlTreeNodeCollatedNext(lvTreeNode *node)
{
	boolean_t	get_next;
	lvTree		*lvt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(local_collseq_stdnull))
	{	/* If standard null collation, then null subscript needs special handling.
		 *
		 * If current subscript is a null subscript, the next subscript in collation order could
		 * very well be the first numeric (canonical) subscript in the avl tree (if one exists).
		 * If not the next subscript would be the first non-null string subscript in the tree.
		 *
		 * If current subscript is NOT a null subscript, it is possible we encounter the null
		 * subscript as the next subscript in collation order. In that case, we need to hop over
		 * it as that collates before the current numeric subscript.
		 */
		if ((NULL != node) && MVTYPE_IS_STRING(node->key_mvtype) && (0 == node->key_len))
		{
			lvt = LV_GET_PARENT_TREE(node);
			node = lvAvlTreeFirst(lvt);
			assert(NULL != node);
		} else
			node = lvAvlTreeNext(node);
		get_next = ((NULL != node) && MVTYPE_IS_STRING(node->key_mvtype) && (0 == node->key_len));
	} else
		get_next = TRUE;
	if (get_next)
		node = lvAvlTreeNext(node);
	return node;
}

/* Function to clone an avl tree (used by the LV_TREE_CLONE macro). Uses recursion to descend the tree. */
lvTreeNode *lvAvlTreeCloneSubTree(lvTreeNode *node, lvTree *lvt, lvTreeNode *avl_parent)
{
        lvTreeNodeVal	*dupVal;
        lvTreeNode        *cloneNode, *left, *right;
        lvTreeNode        *leftSubTree, *rightSubTree;
	lvTree		*lvt_child;
	lv_val		*base_lv;

	assert(NULL != node);
	cloneNode = lvtreenode_getslot(LVT_GET_SYMVAL(lvt));
	/* The following is optimized to do the initialization of just the needed structure members. For that it assumes a
	 * particular "lvTreeNode" structure layout. The assumed layout is asserted so any changes to the layout will
	 * automatically show an issue here and cause the below initialization to be accordingly reworked.
	 */
	assert(0 == OFFSETOF(lvTreeNode, v));
	assert(OFFSETOF(lvTreeNode, v) + SIZEOF(cloneNode->v) == OFFSETOF(lvTreeNode, sbs_child));
	assert(OFFSETOF(lvTreeNode, sbs_child) + SIZEOF(cloneNode->sbs_child) == OFFSETOF(lvTreeNode, tree_parent));
	assert(OFFSETOF(lvTreeNode, tree_parent) + SIZEOF(cloneNode->tree_parent) == OFFSETOF(lvTreeNode, key_mvtype));
	assert(OFFSETOF(lvTreeNode, key_mvtype) + SIZEOF(cloneNode->key_mvtype) == OFFSETOF(lvTreeNode, balance));
	assert(OFFSETOF(lvTreeNode, balance) + SIZEOF(cloneNode->balance) == OFFSETOF(lvTreeNode, descent_dir));
	assert(OFFSETOF(lvTreeNode, descent_dir) + SIZEOF(cloneNode->descent_dir) == OFFSETOF(lvTreeNode, key_len));
	assert(OFFSETOF(lvTreeNode, key_len) + SIZEOF(cloneNode->key_len) == OFFSETOF(lvTreeNode, key_addr));
	assert(OFFSETOF(lvTreeNode, key_mvtype) + 8 == OFFSETOF(lvTreeNode, key_addr));
	GTM64_ONLY(assert(OFFSETOF(lvTreeNode, key_addr) + SIZEOF(cloneNode->key_addr) == OFFSETOF(lvTreeNode, avl_left));)
	NON_GTM64_ONLY(
		assert(OFFSETOF(lvTreeNode, key_addr) + SIZEOF(cloneNode->key_addr) == OFFSETOF(lvTreeNode, filler_8byte));
		assert(OFFSETOF(lvTreeNode, filler_8byte) + SIZEOF(cloneNode->filler_8byte) == OFFSETOF(lvTreeNode, avl_left));
	)
	assert(OFFSETOF(lvTreeNode, avl_left) + SIZEOF(cloneNode->avl_left) == OFFSETOF(lvTreeNode, avl_right));
	assert(OFFSETOF(lvTreeNode, avl_right) + SIZEOF(cloneNode->avl_right) == OFFSETOF(lvTreeNode, avl_parent));
	assert(OFFSETOF(lvTreeNode, avl_parent) + SIZEOF(cloneNode->avl_parent) == SIZEOF(lvTreeNode));
	cloneNode->v = node->v;
	/* "cloneNode->sbs_child" initialized later */
	cloneNode->tree_parent = lvt;
	/* cloneNode->key_mvtype/balance/descent_dir/key_len all initialized in one shot */
	memcpy(&cloneNode->key_mvtype, &node->key_mvtype, 8);	/* Asserts above keep the 8 byte length secure */
	cloneNode->key_addr = node->key_addr;
	NON_GTM64_ONLY(
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNode, filler_8byte, lvTreeNodeNum, key_m1));
		((lvTreeNodeNum *)cloneNode)->key_m1 = ((lvTreeNodeNum *)node)->key_m1;
	)
	cloneNode->avl_parent = avl_parent;
	lvt_child = node->sbs_child;
	base_lv = lvt->base_lv;
	if (NULL != lvt_child)
	{
		LV_TREE_CLONE(lvt_child, cloneNode, base_lv);	/* initializes "cloneNode->sbs_child" */
	} else
		cloneNode->sbs_child = NULL;
	left = node->avl_left;
	leftSubTree = (NULL != left) ? lvAvlTreeCloneSubTree(left, lvt, cloneNode) : NULL;
	cloneNode->avl_left = leftSubTree;
	right = node->avl_right;
	rightSubTree = (NULL != right) ? lvAvlTreeCloneSubTree(right, lvt, cloneNode) : NULL;
	cloneNode->avl_right = rightSubTree;
	return cloneNode;
}

#ifdef DEBUG
/* Function to check integrity of lv key subscript. Currently tests the MV_CANONICAL bit is in sync with the key type */
STATICFNDEF boolean_t lvAvlTreeLookupKeyCheck(treeKeySubscr *key)
{
	mval		dummy_mval;
	boolean_t	is_canonical;

	/* Ensure "key->mvtype" comes in with the MV_CANONICAL bit set if applicable */
	dummy_mval = *key;
	is_canonical = MV_IS_CANONICAL(&dummy_mval);
	if (is_canonical)
		TREE_KEY_SUBSCR_SET_MV_CANONICAL_BIT(&dummy_mval);
	else
		TREE_KEY_SUBSCR_RESET_MV_CANONICAL_BIT(&dummy_mval);
	assert(TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype) == TREE_KEY_SUBSCR_IS_CANONICAL(dummy_mval.mvtype));
	assert(TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype) || MV_IS_STRING(key));
	assert(!TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype) || MV_IS_NUMERIC(key));
	return TRUE;
}

boolean_t lvTreeIsWellFormed(lvTree *lvt)
{
	lvTreeNode	*avl_root, *tmpNode, *minNode, *maxNode, *sbs_parent, *avl_node, *node;
	treeSrchStatus	*lastLookup;
	lvTree		*tmplvt;
	int		nm_node_cnt, sbs_depth;
	lv_val		*base_lv;

	if (NULL != lvt)
	{
		/* Check lvt->ident */
		assert(MV_LV_TREE == lvt->ident);
		/* Check lvt->sbs_parent */
		sbs_parent = LVT_PARENT(lvt);
		assert(NULL != sbs_parent);
		assert(sbs_parent->sbs_child == lvt);
		/* Check lvt->sbs_depth */
		sbs_depth = 1;
		while (!LV_IS_BASE_VAR(sbs_parent))
		{
			tmplvt = LV_GET_PARENT_TREE(sbs_parent);
			sbs_parent = tmplvt->sbs_parent;
			sbs_depth++;
		}
		assert(sbs_depth == lvt->sbs_depth);
		/* Check lvt->base_lv */
		assert((lv_val *)sbs_parent == lvt->base_lv);
		/* Note: lvt->avl_height is checked in lvAvlTreeNodeIsWellFormed */
		/* Check lvt->avl_root */
		if (NULL != (avl_root = lvt->avl_root))
		{
			/* Check lvt->lastLookup clue fields */
			lastLookup = &lvt->lastLookup;
			if (NULL != (tmpNode = lastLookup->lastNodeLookedUp))
			{
				minNode = lastLookup->lastNodeMin;
				if (NULL != minNode)
					assert(0 > lvAvlTreeNodeSubscrCmp(minNode, tmpNode));
				maxNode = lastLookup->lastNodeMax;
				if (NULL != maxNode)
					assert(0 < lvAvlTreeNodeSubscrCmp(maxNode, tmpNode));
			}
			lvAvlTreeNodeIsWellFormed(lvt, avl_root);
		}
	}
	return TRUE;
}

STATICFNDEF boolean_t lvAvlTreeNodeIsWellFormed(lvTree *lvt, lvTreeNode *node)
{
	int 		leftSubTreeHeight, rightSubTreeHeight;
	boolean_t	leftWellFormed, rightWellFormed;
	lvTreeNode	*left, *right;
	lvTree		*sbs_child;

	if (NULL == node)
		return TRUE;
	assert(node->tree_parent == lvt);
	left = node->avl_left;
	assert((NULL == left) || (left->avl_parent == node));
	right = node->avl_right;
	assert((NULL == right) || (right->avl_parent == node));
	leftWellFormed = ((NULL == left) || (0 > lvAvlTreeNodeSubscrCmp(left, node)));
	assert(leftWellFormed);
	rightWellFormed = ((NULL == right) || (0 < lvAvlTreeNodeSubscrCmp(right, node)));
	assert(rightWellFormed);
	leftSubTreeHeight = lvAvlTreeNodeHeight(left);
	rightSubTreeHeight = lvAvlTreeNodeHeight(right);
	if (rightSubTreeHeight > leftSubTreeHeight)
		assert(node->balance == TREE_RIGHT_HEAVY);
	else if (rightSubTreeHeight == leftSubTreeHeight)
		assert(node->balance == TREE_BALANCED);
	else
		assert(node->balance == TREE_LEFT_HEAVY);
	if (node == node->tree_parent->avl_root)
		assert(node->tree_parent->avl_height == MAX(leftSubTreeHeight, rightSubTreeHeight) + 1);
	assert(lvAvlTreeNodeIsWellFormed(lvt, left));
	assert(lvAvlTreeNodeIsWellFormed(lvt, right));
	sbs_child = node->sbs_child;
	if (NULL != sbs_child)
		assert(lvTreeIsWellFormed(sbs_child));
	return TRUE;
}

STATICFNDEF int lvAvlTreeNodeHeight(lvTreeNode *node)
{
	int leftSubTreeHeight, rightSubTreeHeight;

	if (NULL != node)
	{
		leftSubTreeHeight = lvAvlTreeNodeHeight(node->avl_left);
		rightSubTreeHeight = lvAvlTreeNodeHeight(node->avl_right);

		return (MAX(leftSubTreeHeight, rightSubTreeHeight) + 1);
	}
	return 0;
}

void	assert_tree_member_offsets(void)
{
	STATICDEF boolean_t	first_tree_create = TRUE;

	if (first_tree_create)
	{
		/* This is the FIRST "lvTree" that this process is creating. Do a few per-process one-time assertions first. */
		assert(0 == TREE_LEFT_HEAVY);	/* tree_balance_invert array definition depends on this */
		assert(1 == TREE_RIGHT_HEAVY);	/* tree_balance_invert array definition depends on this */
		assert(2 == TREE_BALANCED);	/* tree_balance_invert array definition depends on this */
		assert(TREE_DESCEND_LEFT == TREE_LEFT_HEAVY);	/* they are interchangeably used for performance reasons */
		assert(TREE_DESCEND_RIGHT == TREE_RIGHT_HEAVY); /* they are interchangeably used for performance reasons */

		/* A lot of the lv functions are passed in an (lv_val *) which could be a (lvTreeNode *) as well.
		 * For example, in op_kill.c, if "kill a" is done, an "lv_val *" (corresponding to the base local variable "a")
		 * is passed whereas if "kill a(1,2)" is done, a "lvTreeNode *" (corresponding to the subscripted node "a(1,2)")
		 * is passed. From the input pointer, we need to determine in op_kill.c if it is a "lv_val *" or "lvTreeNode *".
		 * Assuming "curr_lv" is the input pointer of type "lv_val *", to find out if it is a pointer to an lv_val or
		 * lvTreeNode, one checks the ident of the parent. If it is MV_SYM, it means the pointer is to an lv_val (base
		 * variable) and if not it is a lvTreeNode pointer. The code will look like the below.
		 *
		 *	if (MV_SYM == curr_lv->ptrs.val_ent.parent.sym->ident)
		 *	{	// curr_lv is a base var pointing to an "lv_val" structure
		 *		...
		 *	} else
		 *	{	// curr_lv points to a lvTreeNode structure whose parent.sym field points to a "lvTree" structure
		 *		assert(MV_LV_TREE == curr_lv->ptrs.val_ent.parent.sym->ident);
		 *		...
		 *	}
		 *
		 * Note : The MV_SYM == check above has now been folded into a IS_LV_VAL_PTR macro in lv_val.h
		 *
		 * In order for the above check to work, one needs to ensure that that an lv_val.ptrs.val_ent.parent.sym
		 * is at the same offset and size as a lvTreeNode.tree_parent and that a symval.ident is at the same offset and
		 * size as a lvTree.ident.
		 *
		 * Similarly, in op_fndata.c we could be passed in a base lv_val * or a subscripted lv_val (i.e. lvTreeNode *)
		 * and want to find out if this lv_val has any children. To do this we would normally need to check
		 * if the input is an lv_val or a lvTreeNode pointer and access curr_lv->ptrs.val_ent.children or
		 * curr_lv->sbs_child respectively. To avoid this additional if checks, we ensure that both are at the
		 * same offset and have the same size.
		 *
		 * We therefore ensure that all fields below in the same line should be at same structure offset
		 * AND have same size as each other. Note: all of this might not be necessary in current code but
		 * might be useful in the future. Since it is easily possible for us to ensure this, we do so right now.
		 *
		 *	                  lvTreeNode.tree_parent == lv_val.ptrs.val_ent.parent.sym
		 *	                  lvTreeNode.sbs_child   == lv_val.ptrs.val_ent.children
		 *    lvTree.ident     == symval.ident           == lv_val.v.mvtype
		 *    lvTree.sbs_depth == symval.sbs_depth
		 *	                  lvTreeNode.v           == lv_val.v
		 */
		/*	                  lvTreeNode.tree_parent == lv_val.ptrs.val_ent.parent.sym */
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNode, tree_parent, lv_val, ptrs.val_ent.parent.sym));
		/*	                  lvTreeNode.sbs_child   == lv_val.ptrs.val_ent.children */
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNode, sbs_child, lv_val, ptrs.val_ent.children));
		/*    lvTree.ident     == symval.ident           == lv_val.v.mvtype */
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTree, ident, symval, ident));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTree, ident, lv_val, v.mvtype));
		/* In order to get OFFSETOF & SIZEOF to work on v.mvtype, the mval field "mvtype" was changed from
		 * being a 16-bit "unsigned int" type bitfield to a "unsigned short". While this should not affect the
		 * size of the "mvtype" fields, we fear it might affect the size of the immediately following fields
		 * "sgn" (1-bit), "e" (7-bit) and "fnpc_index" (8-bit). While all of them together occupy 16-bits, they
		 * have an "unsigned int" as the type specifier. In order to ensure the compiler does not allocate 4-bytes
		 * (because of the int specification) to those 3 bitfields (and actually use only 2-bytes of those) and
		 * create a 2-byte filler space, we assert that the offset of the immediately following non-bitfield (which
		 * is "m[2]" in Unix & "str" in VMS) in the mval is 4-bytes. If the compiler had allocated 4-bytes, then this
		 * offset would have been 8-bytes instead and the assert will fail alerting us of the unnecessary mval size bloat.
		 */
		UNIX_ONLY(assert(4 == OFFSETOF(mval, m[0]));)
		VMS_ONLY(assert(4 == OFFSETOF(mval, str));)
		/*    lvTree.sbs_depth == symval.sbs_depth */
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTree, sbs_depth, symval, sbs_depth));
		/*	                  lvTreeNode.v           == lv_val.v */
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNode, v, lv_val, v));
		/* Assert that lvTreeNodeNum and lvTreeNodeStr structures have ALMOST the same layout.
		 * This makes them interchangeably usable with care on the non-intersecting fields.
		 */
		assert(SIZEOF(lvTreeNodeNum) == SIZEOF(lvTreeNodeStr));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeNum, v, lvTreeNodeStr, v));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeNum, sbs_child, lvTreeNodeStr, sbs_child));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeNum, tree_parent, lvTreeNodeStr, tree_parent));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeNum, key_mvtype, lvTreeNodeStr, key_mvtype));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeNum, balance, lvTreeNodeStr, balance));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeNum, descent_dir, lvTreeNodeStr, descent_dir));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeNum, avl_left, lvTreeNodeStr, avl_left));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeNum, avl_right, lvTreeNodeStr, avl_right));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeNum, avl_parent, lvTreeNodeStr, avl_parent));
		/* Assert that lvTreeNodeStr and lvTreeNode structures have EXACTLY the same layout.
		 * This makes them interchangeably usable.
		 */
		assert(SIZEOF(lvTreeNodeStr) == SIZEOF(lvTreeNode));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeStr, v, lvTreeNode, v));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeStr, sbs_child, lvTreeNode, sbs_child));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeStr, tree_parent, lvTreeNode, tree_parent));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeStr, key_mvtype, lvTreeNode, key_mvtype));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeStr, balance, lvTreeNode, balance));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeStr, descent_dir, lvTreeNode, descent_dir));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeStr, key_len, lvTreeNode, key_len));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeStr, key_addr, lvTreeNode, key_addr));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeStr, avl_left, lvTreeNode, avl_left));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeStr, avl_right, lvTreeNode, avl_right));
		assert(IS_OFFSET_AND_SIZE_MATCH(lvTreeNodeStr, avl_parent, lvTreeNode, avl_parent));
		first_tree_create = FALSE;
	}
}
#endif

/* In order to lookup a key in the AVL tree, there are THREE functions. One for each different keytype (integer, number, string).
 * 	lvAvlTreeLookupInt (look up an integer subscript)
 * 	lvAvlTreeLookupNum (look up a  non-integer numeric subscript)
 * 	lvAvlTreeLookupStr (look up a  string subscript)
 * The functions share a lot of code but have slightly different codepaths. Since these functions are frequently invoked,
 * we want to keep their cost to a minimum and having separate functions for each keytype lets us minimize the # of if checks
 * in them which is why we do it this way even if it means a lot of duplication.
 *
 * The lookup first attempts to use the clue (if possible) and avoid a full tree traversal.
 * If not easily possible, we do the full tree traversal. As part of that, we update the clue (to help with future lookups).
 * As this is an AVL tree, the full tree traversal will take O(log(n)) (i.e. logarithmic) time.
 */
lvTreeNode *lvAvlTreeLookupInt(lvTree *lvt, treeKeySubscr *key, lvTreeNode **lookupParent)
{
	int		cmp;
	int4		key_m1;
	lvTreeNode	*node, *nextNode, *lastNodeMin, *lastNodeMax, *minNode, *maxNode, **nodePtr;
	lvTreeNode	*parent, *tmpNode;
	treeSrchStatus	*lastLookup;

	assert(lvAvlTreeLookupKeyCheck(key));
	assert(TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
	assert(MVTYPE_IS_INT(key->mvtype));
	assert(NULL != lookupParent);
	TREE_DEBUG_ONLY(assert(lvTreeIsWellFormed(lvt));)
	key_m1 = key->m[1];
	/* First see if node can be looked up easily from the lastLookup clue (without a tree traversal) */
	lastLookup = &lvt->lastLookup;
	if (NULL != (tmpNode = lastLookup->lastNodeLookedUp))
	{
		assert(NULL != lvt->avl_root);	/* there better be an AVL tree if we have a non-zero clue */
		LV_AVL_TREE_INTKEY_CMP(key, key_m1, tmpNode, cmp);
		if (0 == cmp)
		{	/* Input key matches last used clue. Return right away with a match. */
			node = tmpNode;
			/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
			return node;
		} else if (0 < cmp)
		{	/* Input key is GREATER than last used clue. Check RIGHT subtree of clue node to see if input key
			 * could possibly be found there.
			 */
			if (NULL == tmpNode->avl_right)
			{	/* No subtree to the right of the "clue" node.
				 * If input key is LESSER than the maximum key that can be found under the "clue" node,
				 *	then we know for sure input key can not be found anywhere else in the tree.
				 * If input key is GREATER than the maximum key then it definitely is not under "clue"
				 *	but could be somewhere else in the tree. We choose to do a fresh traversal from the top.
				 * If input key is EQUAL to the maximum key then return right away with a match.
				 */
				maxNode = lastLookup->lastNodeMax;
				if (NULL != maxNode)
				{
					assert(0 < lvAvlTreeNodeSubscrCmp(maxNode, tmpNode));
					LV_AVL_TREE_INTKEY_CMP(key, key_m1, maxNode, cmp);
					if (0 == cmp)
					{	/* input key is EQUAL to the maximum possible key under "clue" node */
						node = maxNode;
						/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
						return node;
					} else if (0 > cmp)
					{	/* input key is LESSER than the maximum possible key under "clue" node */
						parent = tmpNode;
						parent->descent_dir = TREE_DESCEND_RIGHT;
						*lookupParent = parent;
						return NULL;
					}
				} else
				{	/* Note: maxNode == NULL implies, max key is +INFINITY so treat this case the same way
					 * as if input key value is LESSER than the maximum possible key under the "clue" node.
					 */
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_RIGHT;
					*lookupParent = parent;
					return NULL;
				}
			}
		} else if (NULL == tmpNode->avl_left)
		{	/* Input key is LESSER than last used clue AND no subtree to the left of the "clue" node.
			 * If input key is GREATER than the minimum key that can be found under the "clue" node,
			 *	then we know for sure input key can not be found anywhere else in the tree.
			 * If input key is LESSER than the minimum key then it definitely is not under "clue"
			 *	but could be somewhere else in the tree. We choose to do a fresh traversal from the top.
			 * If input key is EQUAL to the minimum key then return right away with a match.
			 */
			minNode = lastLookup->lastNodeMin;
			if (NULL != minNode)
			{
				assert(0 > lvAvlTreeNodeSubscrCmp(minNode, tmpNode));
				LV_AVL_TREE_INTKEY_CMP(key, key_m1, minNode, cmp);
				if (0 == cmp)
				{	/* input key is EQUAL to the minimum possible key under "clue" node */
					node = minNode;
					/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
					return node;
				} else if (0 < cmp)
				{	/* input key is GREATER than the minimum possible key under "clue" node */
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_LEFT;
					*lookupParent = parent;
					return NULL;
				}
			} else
			{	/* Note: minNode == NULL implies, min key is -INFINITY so treat this case the same way
				 * as if input key value is GREATER than the minimum possible key under the "clue" node.
				 */
				parent = tmpNode;
				parent->descent_dir = TREE_DESCEND_LEFT;
				*lookupParent = parent;
				return NULL;
			}
		}
	}
	/* Now that we know "clue" did not help, do a fresh traversal looking for the input key. Maintain clue at the same time */
	parent = lvt->avl_root;
	if (NULL == parent)
	{
		*lookupParent = NULL;
		return NULL;
	}
	node = parent;
	lastNodeMin = (lvTreeNode *)NULL;	/* the minimum possible key under "clue" is initially -INFINITY */
	lastNodeMax = (lvTreeNode *)NULL;	/* the maximum possible key under "clue" is initially +INFINITY */
	if (NULL != node)
	{
		while (TRUE)
		{
			LV_AVL_TREE_INTKEY_CMP(key, key_m1, node, cmp);
			if (0 == cmp)
			{
				lvt->lastLookup.lastNodeLookedUp = node;
				break;
			} else if (cmp < 0)
			{
				node->descent_dir = TREE_DESCEND_LEFT;
				/* if we descend left, we know for sure all-subtree-keys are < node key so update the max key */
				nodePtr = &lastNodeMax;
				nextNode = node->avl_left;
			} else /* (cmp > 0) */
			{
				node->descent_dir = TREE_DESCEND_RIGHT;
				/* if we descend right, we know for sure all-subtree-keys are > node-key so update the min key */
				nodePtr = &lastNodeMin;
				nextNode = node->avl_right;
			}
			parent = node;
			node = nextNode;
			if (NULL == node)
			{
				lvt->lastLookup.lastNodeLookedUp = parent;
				break;
			}
			*nodePtr = parent;	/* the actual max-key or min-key update happens here */
		}
	} else
		lvt->lastLookup.lastNodeLookedUp = NULL;
	*lookupParent = parent;
	lvt->lastLookup.lastNodeMin = lastNodeMin;	/* now that clue has been set, also set max-key and min-key */
	lvt->lastLookup.lastNodeMax = lastNodeMax;
	return node;
}

/* All comments for "lvAvlTreeLookupInt" function apply here as well */
lvTreeNode *lvAvlTreeLookupNum(lvTree *lvt, treeKeySubscr *key, lvTreeNode **lookupParent)
{
	int		cmp;
	lvTreeNode	*node, *nextNode, *lastNodeMin, *lastNodeMax, *minNode, *maxNode, **nodePtr;
	lvTreeNode	*parent, *tmpNode;
	treeSrchStatus	*lastLookup;

	assert(lvAvlTreeLookupKeyCheck(key));
	assert(TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
	assert(!MVTYPE_IS_INT(key->mvtype));
	assert(NULL != lookupParent);
	TREE_DEBUG_ONLY(assert(lvTreeIsWellFormed(lvt));)
	/* First see if node can be looked up easily from the lastLookup clue (without a tree traversal) */
	lastLookup = &lvt->lastLookup;
	if (NULL != (tmpNode = lastLookup->lastNodeLookedUp))
	{
		assert(NULL != lvt->avl_root);	/* there better be an AVL tree if we have a non-zero clue */
		LV_AVL_TREE_NUMKEY_CMP(key, tmpNode, cmp);
		if (0 == cmp)
		{	/* Input key matches last used clue. Return right away with a match. */
			node = tmpNode;
			/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
			return node;
		} else if (0 < cmp)
		{	/* Input key is GREATER than last used clue. Check RIGHT subtree of clue node to see if input key
			 * could possibly be found there.
			 */
			if (NULL == tmpNode->avl_right)
			{	/* No subtree to the right of the "clue" node.
				 * If input key is LESSER than the maximum key that can be found under the "clue" node,
				 *	then we know for sure input key can not be found anywhere else in the tree.
				 * If input key is GREATER than the maximum key then it definitely is not under "clue"
				 *	but could be somewhere else in the tree. We choose to do a fresh traversal from the top.
				 * If input key is EQUAL to the maximum key then return right away with a match.
				 */
				maxNode = lastLookup->lastNodeMax;
				if (NULL != maxNode)
				{
					assert(0 < lvAvlTreeNodeSubscrCmp(maxNode, tmpNode));
					LV_AVL_TREE_NUMKEY_CMP(key, maxNode, cmp);
					if (0 == cmp)
					{	/* input key is EQUAL to the maximum possible key under "clue" node */
						node = maxNode;
						/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
						return node;
					} else if (0 > cmp)
					{	/* input key is LESSER than the maximum possible key under "clue" node */
						parent = tmpNode;
						parent->descent_dir = TREE_DESCEND_RIGHT;
						*lookupParent = parent;
						return NULL;
					}
				} else
				{	/* Note: maxNode == NULL implies, max key is +INFINITY so treat this case the same way
					 * as if input key value is LESSER than the maximum possible key under the "clue" node.
					 */
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_RIGHT;
					*lookupParent = parent;
					return NULL;
				}
			}
		} else if (NULL == tmpNode->avl_left)
		{	/* Input key is LESSER than last used clue AND no subtree to the left of the "clue" node.
			 * If input key is GREATER than the minimum key that can be found under the "clue" node,
			 *	then we know for sure input key can not be found anywhere else in the tree.
			 * If input key is LESSER than the minimum key then it definitely is not under "clue"
			 *	but could be somewhere else in the tree. We choose to do a fresh traversal from the top.
			 * If input key is EQUAL to the minimum key then return right away with a match.
			 */
			minNode = lastLookup->lastNodeMin;
			if (NULL != minNode)
			{
				assert(0 > lvAvlTreeNodeSubscrCmp(minNode, tmpNode));
				LV_AVL_TREE_NUMKEY_CMP(key, minNode, cmp);
				if (0 == cmp)
				{	/* input key is EQUAL to the minimum possible key under "clue" node */
					node = minNode;
					/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
					return node;
				} else if (0 < cmp)
				{	/* input key is GREATER than the minimum possible key under "clue" node */
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_LEFT;
					*lookupParent = parent;
					return NULL;
				}
			} else
			{	/* Note: minNode == NULL implies, min key is -INFINITY so treat this case the same way
				 * as if input key value is GREATER than the minimum possible key under the "clue" node.
				 */
				parent = tmpNode;
				parent->descent_dir = TREE_DESCEND_LEFT;
				*lookupParent = parent;
				return NULL;
			}
		}
	}
	/* Now that we know "clue" did not help, do a fresh traversal looking for the input key. Maintain clue at the same time */
	parent = lvt->avl_root;
	if (NULL == parent)
	{
		*lookupParent = NULL;
		return NULL;
	}
	node = parent;
	lastNodeMin = (lvTreeNode *)NULL;	/* the minimum possible key under "clue" is initially -INFINITY */
	lastNodeMax = (lvTreeNode *)NULL;	/* the maximum possible key under "clue" is initially +INFINITY */
	if (NULL != node)
	{
		while (TRUE)
		{
			LV_AVL_TREE_NUMKEY_CMP(key, node, cmp);
			if (0 == cmp)
			{
				lvt->lastLookup.lastNodeLookedUp = node;
				break;
			} else if (cmp < 0)
			{
				node->descent_dir = TREE_DESCEND_LEFT;
				/* if we descend left, we know for sure all-subtree-keys are < node key so update the max key */
				nodePtr = &lastNodeMax;
				nextNode = node->avl_left;
			} else /* (cmp > 0) */
			{
				node->descent_dir = TREE_DESCEND_RIGHT;
				/* if we descend right, we know for sure all-subtree-keys are > node-key so update the min key */
				nodePtr = &lastNodeMin;
				nextNode = node->avl_right;
			}
			parent = node;
			node = nextNode;
			if (NULL == node)
			{
				lvt->lastLookup.lastNodeLookedUp = parent;
				break;
			}
			*nodePtr = parent;	/* the actual max-key or min-key update happens here */
		}
	} else
		lvt->lastLookup.lastNodeLookedUp = NULL;
	*lookupParent = parent;
	lvt->lastLookup.lastNodeMin = lastNodeMin;	/* now that clue has been set, also set max-key and min-key */
	lvt->lastLookup.lastNodeMax = lastNodeMax;
	return node;
}

/* All comments for "lvAvlTreeLookupInt" function apply here as well */
lvTreeNode *lvAvlTreeLookupStr(lvTree *lvt, treeKeySubscr *key, lvTreeNode **lookupParent)
{
	int		cmp, key_len;
	char		*key_addr;
	lvTreeNode	*node, *nextNode, *lastNodeMin, *lastNodeMax, *minNode, *maxNode, **nodePtr;
	lvTreeNode	*parent, *tmpNode;
	treeSrchStatus	*lastLookup;

	assert(lvAvlTreeLookupKeyCheck(key));
	assert(!TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
	assert(MVTYPE_IS_STRING(key->mvtype));
	assert(NULL != lookupParent);
	TREE_DEBUG_ONLY(assert(lvTreeIsWellFormed(lvt));)
	key_addr = key->str.addr;
	key_len = key->str.len;
	/* First see if node can be looked up easily from the lastLookup clue (without a tree traversal) */
	lastLookup = &lvt->lastLookup;
	if (NULL != (tmpNode = lastLookup->lastNodeLookedUp))
	{
		assert(NULL != lvt->avl_root);	/* there better be an AVL tree if we have a non-zero clue */
		LV_AVL_TREE_STRKEY_CMP(key, key_addr, key_len, tmpNode, cmp);
		if (0 == cmp)
		{	/* Input key matches last used clue. Return right away with a match. */
			node = tmpNode;
			/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
			return node;
		} else if (0 < cmp)
		{	/* Input key is GREATER than last used clue. Check RIGHT subtree of clue node to see if input key
			 * could possibly be found there.
			 */
			if (NULL == tmpNode->avl_right)
			{	/* No subtree to the right of the "clue" node.
				 * If input key is LESSER than the maximum key that can be found under the "clue" node,
				 *	then we know for sure input key can not be found anywhere else in the tree.
				 * If input key is GREATER than the maximum key then it definitely is not under "clue"
				 *	but could be somewhere else in the tree. We choose to do a fresh traversal from the top.
				 * If input key is EQUAL to the maximum key then return right away with a match.
				 */
				maxNode = lastLookup->lastNodeMax;
				if (NULL != maxNode)
				{
					assert(0 < lvAvlTreeNodeSubscrCmp(maxNode, tmpNode));
					LV_AVL_TREE_STRKEY_CMP(key, key_addr, key_len, maxNode, cmp);
					if (0 == cmp)
					{	/* input key is EQUAL to the maximum possible key under "clue" node */
						node = maxNode;
						/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
						return node;
					} else if (0 > cmp)
					{	/* input key is LESSER than the maximum possible key under "clue" node */
						parent = tmpNode;
						parent->descent_dir = TREE_DESCEND_RIGHT;
						*lookupParent = parent;
						return NULL;
					}
				} else
				{	/* Note: maxNode == NULL implies, max key is +INFINITY so treat this case the same way
					 * as if input key value is LESSER than the maximum possible key under the "clue" node.
					 */
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_RIGHT;
					*lookupParent = parent;
					return NULL;
				}
			}
		} else if (NULL == tmpNode->avl_left)
		{	/* Input key is LESSER than last used clue AND no subtree to the left of the "clue" node.
			 * If input key is GREATER than the minimum key that can be found under the "clue" node,
			 *	then we know for sure input key can not be found anywhere else in the tree.
			 * If input key is LESSER than the minimum key then it definitely is not under "clue"
			 *	but could be somewhere else in the tree. We choose to do a fresh traversal from the top.
			 * If input key is EQUAL to the minimum key then return right away with a match.
			 */
			minNode = lastLookup->lastNodeMin;
			if (NULL != minNode)
			{
				assert(0 > lvAvlTreeNodeSubscrCmp(minNode, tmpNode));
				LV_AVL_TREE_STRKEY_CMP(key, key_addr, key_len, minNode, cmp);
				if (0 == cmp)
				{	/* input key is EQUAL to the minimum possible key under "clue" node */
					node = minNode;
					/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
					return node;
				} else if (0 < cmp)
				{	/* input key is GREATER than the minimum possible key under "clue" node */
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_LEFT;
					*lookupParent = parent;
					return NULL;
				}
			} else
			{	/* Note: minNode == NULL implies, min key is -INFINITY so treat this case the same way
				 * as if input key value is GREATER than the minimum possible key under the "clue" node.
				 */
				parent = tmpNode;
				parent->descent_dir = TREE_DESCEND_LEFT;
				*lookupParent = parent;
				return NULL;
			}
		}
	}
	/* Now that we know "clue" did not help, do a fresh traversal looking for the input key. Maintain clue at the same time */
	parent = lvt->avl_root;
	if (NULL == parent)
	{
		*lookupParent = NULL;
		return NULL;
	}
	node = parent;
	lastNodeMin = (lvTreeNode *)NULL;	/* the minimum possible key under "clue" is initially -INFINITY */
	lastNodeMax = (lvTreeNode *)NULL;	/* the maximum possible key under "clue" is initially +INFINITY */
	if (NULL != node)
	{
		while (TRUE)
		{
			LV_AVL_TREE_STRKEY_CMP(key, key_addr, key_len, node, cmp);
			if (0 == cmp)
			{
				lvt->lastLookup.lastNodeLookedUp = node;
				break;
			} else if (cmp < 0)
			{
				node->descent_dir = TREE_DESCEND_LEFT;
				/* if we descend left, we know for sure all-subtree-keys are < node key so update the max key */
				nodePtr = &lastNodeMax;
				nextNode = node->avl_left;
			} else /* (cmp > 0) */
			{
				node->descent_dir = TREE_DESCEND_RIGHT;
				/* if we descend right, we know for sure all-subtree-keys are > node-key so update the min key */
				nodePtr = &lastNodeMin;
				nextNode = node->avl_right;
			}
			parent = node;
			node = nextNode;
			if (NULL == node)
			{
				lvt->lastLookup.lastNodeLookedUp = parent;
				break;
			}
			*nodePtr = parent;	/* the actual max-key or min-key update happens here */
		}
	} else
		lvt->lastLookup.lastNodeLookedUp = NULL;
	*lookupParent = parent;
	lvt->lastLookup.lastNodeMin = lastNodeMin;	/* now that clue has been set, also set max-key and min-key */
	lvt->lastLookup.lastNodeMax = lastNodeMax;
	return node;
}

/* Function to lookup an input key in the AVL tree. This function works for any input key type.
 * Returns if a given key is found or not. "lookupParent" will be updated ONLY IF "node" is NOT found in tree.
 * Note: It is preferable for performance-sensitive callers to call the lvAvlTreeLookupInt/lvAvlTreeLookupNum/lvAvlTreeLookupStr
 * functions directly (assuming the caller already knows the key type) instead of going through here thereby avoiding an if check.
 */
lvTreeNode *lvAvlTreeLookup(lvTree *lvt, treeKeySubscr *key, lvTreeNode **lookupParent)
{
	int		key_mvtype;

	assert(lvAvlTreeLookupKeyCheck(key));
	key_mvtype = key->mvtype;
	if (TREE_KEY_SUBSCR_IS_CANONICAL(key_mvtype))
	{
		if (MVTYPE_IS_INT(key_mvtype))
			return lvAvlTreeLookupInt(lvt, key, lookupParent);
		else
			return lvAvlTreeLookupNum(lvt, key, lookupParent);
	} else
		return lvAvlTreeLookupStr(lvt, key, lookupParent);
}

/* The below two functions (lvAvlTreeSingleRotation & lvAvlTreeDoubleRotation) do height balancing of AVL tree by tree rotation.
 * They are used by the "lvAvlTreeNodeInsert" and "lvAvlTreeNodeDelete" functions.
 * A node insertion or deletion can cause the AVL tree height balance to be disturbed by taking it to one of 4 states.
 *	a) Left Left
 *	b) Right Right
 *	c) Left Right
 *	d) Right Left
 * Cases (a) and (b) can be taken into a "Balanced" state by just ONE rotation (implemented by lvAvlTreeSingleRotation).
 * Cases (c) and (d) need TWO rotations to take them to a "Balanced" state as shown below (implemented by lvAvlTreeDoubleRotation).
 *
 * In the below illustration (courtesy of http://en.wikipedia.org/wiki/AVL_tree) key points to note are
 *	-> 3,4,5 are nodes with those key values.
 *	-> A,B,C,D are arbitrary subtrees (not necessarily nodes).
 *	-> The double-width link (\\ or //) in each tree is where the rotation happens
 *		(the child in that link becomes the parent) to take it to the next state.
 *
 *      Left Right case                      Left Left case                                  Balanced case
 *      ----------------                     -----------------                               --------------
 *
 *             -------                                 -------                                -------------
 *             |  5  |                                 |  5  |                                |     4     |
 *             -------                                 -------                                -------------
 *             /     \                                //     \                               /             \
 *            /       \                              //       \                             /               \
 *        -------    -------                      -------    -------                    -------            -------
 *        |  3  |    |  D  |                      |  4  |    |  D  |                    |  3  |            |  5  |
 *        -------    -------                      -------    -------                    -------            -------
 *        /     \\                ---->           /     \                ---->          /     \             /     \
 *       /       \\                              /       \                             /       \           /       \
 *   -------    -------                      -------    -------                    -------    -------  -------    -------
 *   |  A  |    |  4  |                      |  3  |    |  C  |                    |  A  |    |  B  |  |  C  |    |  D  |
 *   -------    -------                      -------    -------                    -------    -------  -------    -------
 *              /     \                      /     \
 *             /       \                    /       \
 *         -------    -------           -------    -------
 *         |  B  |    |  C  |           |  A  |    |  B  |
 *         -------    -------           -------    -------
 *
 *      Right Left case                      Right Right case                                  Balanced case
 *      ----------------                     -----------------                                 --------------
 *
 *         -------                               -------                                        -------------
 *         |  3  |                               |  3  |                                        |     4     |
 *         -------                               -------                                        -------------
 *         /     \                               /     \\                                      /             \
 *        /       \                             /       \\                                    /               \
 *    -------    -------                    -------    -------                            -------            -------
 *    |  A  |    |  5  |                    |  A  |    |  4  |                            |  3  |            |  5  |
 *    -------    -------                    -------    -------                            -------            -------
 *               //   \            ---->               /     \             ---->          /     \             /     \
 *              //     \                              /       \                          /       \           /       \
 *           -------  -------                     -------    -------                 -------    -------  -------    -------
 *           |  4  |  |  D  |                     |  B  |    |  5  |                 |  A  |    |  B  |  |  C  |    |  D  |
 *           -------  -------                     -------    -------                 -------    -------  -------    -------
 *           /     \                                         /     \
 *          /       \                                       /       \
 *      -------    -------                              -------    -------
 *      |  B  |    |  C  |                              |  C  |    |  D  |
 *      -------    -------                              -------    -------
 *
 */

STATICFNDEF lvTreeNode *lvAvlTreeSingleRotation(lvTreeNode *rebalanceNode, lvTreeNode *anchorNode, int4 balanceFactor)
{
	lvTreeNode	*newRoot, *node;

	TREE_DEBUG1("DOING SINGLE ROTATION\n");

	assert((TREE_LEFT_HEAVY == balanceFactor) || (TREE_RIGHT_HEAVY ==  balanceFactor));
	if (TREE_LEFT_HEAVY == balanceFactor)
	{	/* Left-Left path rotate. In above illustration, 5 is rebalanceNode, 4 is anchorNode */
		assert(rebalanceNode->avl_left == anchorNode);
		node = anchorNode->avl_right;
		rebalanceNode->avl_left = node;
		anchorNode->avl_right = rebalanceNode;
	} else
	{	/* Right-Right path rotate. In above illustration, 3 is rebalanceNode, 4 is anchorNode */
		assert(rebalanceNode->avl_right == anchorNode);
		node = anchorNode->avl_left;
		rebalanceNode->avl_right = node;
		anchorNode->avl_left = rebalanceNode;
	}
	if (node)
		node->avl_parent = rebalanceNode;
	rebalanceNode->avl_parent = anchorNode;
	if (TREE_IS_NOT_BALANCED(anchorNode->balance))
	{
		rebalanceNode->balance = TREE_BALANCED;
		anchorNode->balance = TREE_BALANCED;
	} else
		anchorNode->balance = TREE_INVERT_BALANCE_FACTOR(balanceFactor);
	newRoot = anchorNode;
	return newRoot;
}

/* Comments before "lvAvlTreeSingleRotation" function definition describe what this function does */
STATICFNDEF lvTreeNode *lvAvlTreeDoubleRotation(lvTreeNode *rebalanceNode, lvTreeNode *anchorNode, int4 balanceFactor)
{
	lvTreeNode	*newRoot, *node, *rebalanceChild, *anchorChild;
	int		balance;

	TREE_DEBUG1("DOING DOUBLE ROTATION\n");

	assert((TREE_LEFT_HEAVY == balanceFactor) || (TREE_RIGHT_HEAVY ==  balanceFactor));
	if (TREE_LEFT_HEAVY == balanceFactor)
	{	/* Left-Right path rotate. In above illustration, 5 is rebalanceNode, 3 is anchorNode */
		assert(rebalanceNode->avl_left == anchorNode);
		newRoot = anchorNode->avl_right;
		rebalanceChild = newRoot->avl_right;
		rebalanceNode->avl_left = rebalanceChild;
		anchorChild = newRoot->avl_left;
		anchorNode->avl_right = anchorChild;
		newRoot->avl_left = anchorNode;
		newRoot->avl_right = rebalanceNode;
	} else
	{	/* Right-Left path rotate. In above illustration, 3 is rebalanceNode, 5 is anchorNode */
		assert(rebalanceNode->avl_right == anchorNode);
		newRoot = anchorNode->avl_left;
		rebalanceChild = newRoot->avl_left;
		rebalanceNode->avl_right = rebalanceChild;
		anchorChild = newRoot->avl_right;
		anchorNode->avl_left = anchorChild;
		newRoot->avl_left = rebalanceNode;
		newRoot->avl_right = anchorNode;
	}
	if (NULL != rebalanceChild)
		rebalanceChild->avl_parent = rebalanceNode;
	if (NULL != anchorChild)
		anchorChild->avl_parent = anchorNode;
	anchorNode->avl_parent = newRoot;
	rebalanceNode->avl_parent = newRoot;
	balance = newRoot->balance;
	if (balance == balanceFactor)
	{
		rebalanceNode->balance = TREE_INVERT_BALANCE_FACTOR(balanceFactor);
		anchorNode->balance = TREE_BALANCED;
	} else if (TREE_IS_BALANCED(balance))
	{
		rebalanceNode->balance = TREE_BALANCED;
		anchorNode->balance = TREE_BALANCED;
	} else
	{	/* balance & balanceFactor are opposites */
		rebalanceNode->balance = TREE_BALANCED;
		anchorNode->balance = balanceFactor;
	}
	newRoot->balance = TREE_BALANCED;
	return(newRoot);
}

/* Function to INSERT a node into the AVL tree.
 *
 * At a high level, insertion proceeds by doing a key lookup first and inserting the key wherever the lookup ends.
 * After inserting a node, it is necessary to check each of the node's ancestors for consistency with the rules of AVL.
 * For each node checked, if the height difference between the left and right subtrees is atmost 1 then no rotations are necessary.
 * If not, then the subtree rooted at this node is unbalanced. It is possible more than one node in the ancestor path is
 * unbalanced due to the insertion but at most ONE tree rotation is needed (at the last unbalanced node down the tree path)
 * to restore the entire tree to the rules of AVL.
 *
 * Key points to note for AVL tree insertion are (courtesy of http://neil.brown.name/blog/20041124101820)
 *
 * When we step down from a node to the longest subtree, that node's tree will not get any larger. If the subtree
 * we descend into grows, a rotation will be needed, but this will result in this tree being the same height as it
 * was before. The only difference at this level is that it will be balanced whereas it wasn't before. This means
 * that when we step down a longer path, we can forget about all the nodes above the one we step down from. They
 * cannot be affected as neither of their subtrees will grow. This observation tells us where the rotation has to happen.
 *
 * We only need to rotate if as part of the lookup/insert, we step down to a longer subtree, and then all
 * subsequent nodes are balanced, without longer or shorter subtrees. The actual rotation that happens
 * (if needed) will depend on what happens immediately after stepping down to a longer subtree. If the
 * next step is in the same direction as the step into the larger subtree, then a SINGLE rotation will be
 * needed. If the next step is in the other direction, then a DOUBLE rotation will be needed.
 *
 * The important data that we need is that part of the path from the last unbalanced node to the insertion
 * point. All nodes in this list other than the first will be balanced, including the newly inserted
 * node. All of these nodes will either take part in a rotation, or will need to have their balance value
 * changed (though the new node won't need it's balance changed, it could take part in a rotation though).
 *
 * This path may not actually start with an unbalanced node. If every node in the path from the root is
 * balanced, then that whole path needs to be recorded, and every node will have it's balance changed.
 *
 * Time for insert = O(log n) for lookup + O(1) for at most one single or double rotation = O(log n)
 */
lvTreeNode *lvAvlTreeNodeInsert(lvTree *lvt, treeKeySubscr *key, lvTreeNode *parent)
{
	lvTreeNode 	*node, *t_root, *tmp_parent, *anchorNode, *tmpNode, **nodePtr;
	lvTreeNode	*rebalanceNode, *rebalanceNodeParent;
	lvTreeNodeNum	*fltNode;
	int		balanceFactor, curBalance, cmp, key_mvtype;
	treeSrchStatus	*lastLookup;
	DEBUG_ONLY(lvTreeNode	*dbg_node);

	assert(MV_DEFINED(key));
	/* At time of node insertion into tree, ensure that if we have MV_CANONICAL bit set, then MV_STR bit is not set.
	 * This makes it clear that the node key is a number and does not have the string representation constructed.
	 */
	assert(!TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype) || MV_IS_NUMERIC(key) && !MV_IS_STRING(key));
	assert(NULL == lvAvlTreeLookup(lvt, key, &tmp_parent));
	/* create a node in the avl tree and initialize it */
	node = lvtreenode_getslot(LVT_GET_SYMVAL(lvt));
	assert(NULL != node);
	/* node->v must be initialized by caller */
	node->sbs_child = NULL;
	node->tree_parent = lvt;
	node->avl_left = node->avl_right = NULL;
	node->avl_parent = parent;
	key_mvtype = key->mvtype;
	if (TREE_KEY_SUBSCR_IS_CANONICAL(key_mvtype))
	{	/* Initialize lvTreeNodeNum structure */
		fltNode = (lvTreeNodeNum *)node;
		fltNode->key_mvtype = (key_mvtype & MV_EXT_NUM_MASK);
		if (MV_INT & key_mvtype)
		{
			fltNode->key_m0 = key->m[1];
			fltNode->key_flags.key_bits.key_iconv = FALSE;
		} else
		{
			fltNode->key_flags.key_bytes.key_sgne = ((mval_b *)key)->sgne;
			fltNode->key_m0 = key->m[0];
			fltNode->key_m1 = key->m[1];
		}
	} else
	{	/* Initialize lvTreeNode structure */
		node->key_mvtype = MV_STR;	/* do not use input mvtype as it might have MV_NM or MV_NUM_APPROX bits set */
		node->key_len = key->str.len;
		node->key_addr = key->str.addr;
	}
	node->balance = TREE_BALANCED;
	/* node->descent_dir is initialized later when this node is part of a lvAvlTreeLookup operation.
	 * this field is not used by anyone until then so ok not to initialize it here.
	 * Update lastLookup clue to reflect the newly inserted node. Note that this clue will continue
	 * to be valid inspite of any single or double rotations that happen as part of the insert below.
	 */
	lastLookup = &lvt->lastLookup;
	lastLookup->lastNodeLookedUp = node;	/* first part of lastLookup clue update */
	/* Do height-balancing tree rotations as needed to maintain avl property */
	if (NULL != parent)
	{
		if (TREE_DESCEND_LEFT == parent->descent_dir)
		{
			parent->avl_left = node;
			nodePtr = &lastLookup->lastNodeMax;
		} else
		{
			parent->avl_right = node;
			nodePtr = &lastLookup->lastNodeMin;
		}
		*nodePtr = parent;	/* second (and last) part of lastLookup clue update */
		t_root = lvt->avl_root;
		/* Do balance factor adjustment & rotation as needed */
		tmpNode = parent;
		while (t_root != tmpNode)
		{
			balanceFactor = tmpNode->balance;
			if (TREE_IS_NOT_BALANCED(balanceFactor))
				break;
			tmpNode->balance = tmpNode->descent_dir;
			tmpNode = tmpNode->avl_parent;
		}
		rebalanceNode = tmpNode;
		if (TREE_DESCEND_LEFT == rebalanceNode->descent_dir)
		{
			anchorNode = rebalanceNode->avl_left;
			balanceFactor = TREE_LEFT_HEAVY;
		} else
		{
			anchorNode = rebalanceNode->avl_right;
			balanceFactor = TREE_RIGHT_HEAVY;
		}
		curBalance = rebalanceNode->balance;
		if (TREE_IS_BALANCED(curBalance)) /* tree has grown taller */
		{
			rebalanceNode->balance = balanceFactor;
			TREE_DEBUG3("Tree height increased from %d to %d\n", lvt->avl_height, (lvt->avl_height + 1));
			lvt->avl_height++;
			assert(1 < lvt->avl_height);
		} else if (curBalance == TREE_INVERT_BALANCE_FACTOR(balanceFactor))
		{	/* the tree has gotten more balanced */
			rebalanceNode->balance = TREE_BALANCED;
		} else
		{	/* The tree has gotten out of balance so need a rotation */
			rebalanceNodeParent = rebalanceNode->avl_parent;
			assert(anchorNode->balance == anchorNode->descent_dir);
			assert(balanceFactor == rebalanceNode->descent_dir);
			if (anchorNode->balance == balanceFactor)
				tmpNode = lvAvlTreeSingleRotation(rebalanceNode, anchorNode, balanceFactor);
			else
				tmpNode = lvAvlTreeDoubleRotation(rebalanceNode, anchorNode, balanceFactor);
			tmpNode->avl_parent = rebalanceNodeParent;
			if (NULL != rebalanceNodeParent)
			{	/* root of tree has NOT changed */
				assert(rebalanceNodeParent->descent_dir
					== ((rebalanceNodeParent->avl_left == rebalanceNode)
						? TREE_DESCEND_LEFT : TREE_DESCEND_RIGHT));
				if (TREE_DESCEND_LEFT == rebalanceNodeParent->descent_dir)
					rebalanceNodeParent->avl_left = tmpNode;
				else
					rebalanceNodeParent->avl_right = tmpNode;
			} else
				lvt->avl_root = tmpNode;	/* new root although tree height is still the same */
		}
	} else
	{	/* we just inserted the real root of the tree */
		assert(NULL == lvt->avl_root);
		lvt->avl_root = node;
		assert(NULL == node->avl_right);
		assert(NULL == node->avl_left);
		assert(TREE_BALANCED == node->balance);
		TREE_DEBUG3("Tree height increased from %d to %d\n", lvt->avl_height, (lvt->avl_height + 1));
		assert(0 == lvt->avl_height);
		lvt->avl_height = 1;
		lastLookup->lastNodeMin = NULL;	/* ensure lastLookup clue is uptodate */
		lastLookup->lastNodeMax = NULL;	/* ensure lastLookup clue is uptodate */
	}
	TREE_DEBUG_ONLY(assert(lvTreeIsWellFormed(lvt));)
	return node;
}

/* Function to DELETE a node from the AVL tree.
 *
 * At a high level, deletion proceeds by doing a key lookup first and deleting the node wherever the lookup ends.
 * After deleting a node, it is necessary to check each of the node's ancestors for consistency with the rules of AVL.
 * For each node checked, if the height difference between the left and right subtrees is atmost 1 then no rotations are necessary.
 * If not, then the subtree rooted at this node is unbalanced. It is possible more than one node in the ancestor path is
 * unbalanced due to the deletion. Unlike insertion, MANY (as many as the height of the tree) tree rotations are needed
 * to restore the entire tree to the rules of AVL.
 *
 * Key points to note for AVL tree deletion are (courtesy of http://neil.brown.name/blog/20041124141849)
 *
 * It is easy to remove a node when it only has one child, as that child can replace the deleted node as the child of
 * the grandparent. If the target node has two children, the easiest approach is to swap the node with a node that does
 * have at most one child. This can work providing we can find such a node that is adjacent to the target node in the sort-order.
 * A few moments consideration will confirm that if a node has two children, then there must be a node both immediately above
 * and below it in the sort order, and that both of these must be further down the tree than the target node, and that neither
 * of these can have two children (as if they did, then one of the children would lie between that node and the target, which
 * contradicts the selection of that node). In this implementation we choose the immediately-above node (in-order successor) only.
 *
 * While insert only needed one rotation, delete may need rotation at every level.
 * If the shrinkage of a subtree (i.e. height reduction due to a tree rotation) causes the node's tree to shrink, the change
 * must propagate up. If it does not, then all higher nodes in the tree will not be affected.
 *
 * Time for delete = O(log n) for lookup + at most O(log n) rotations = O(log n)
 */
void lvAvlTreeNodeDelete(lvTree *lvt, lvTreeNode *node)
{
	boolean_t	treeBalanced;
	int4		balanceFactor, curNodeBalance;
	lvTreeNode	*balanceThisNode, *nextBalanceThisNode, *anchorNode, *newRoot;
	lvTreeNode	*next, *nextLeft, *nextRight, *nextParent, *left;
	lvTreeNode	*nodeLeft, *nodeRight, *nodeParent;
	lvTreeNode	*rebalanceNode, *rebalanceNodeParent;

	assert(IS_LVAVLTREENODE(((lv_val *)node)));	/* ensure input "node" is of type "lvTreeNode *" or "lvTreeNodeNum *"
							 * and not "lv_val *" */
	assert(lvt == node->tree_parent);
	nodeLeft = node->avl_left;
	nodeRight = node->avl_right;
	nodeParent = node->avl_parent;
	if (NULL == nodeRight)
	{	/* No right subtree. Move the left subtree (if it exists) over to the current node. */
		balanceThisNode = nodeParent;
		if (NULL != balanceThisNode)
		{
			assert(balanceThisNode->descent_dir
				== ((balanceThisNode->avl_left == node) ? TREE_DESCEND_LEFT : TREE_DESCEND_RIGHT));
			if (TREE_DESCEND_LEFT == balanceThisNode->descent_dir)
				balanceThisNode->avl_left = nodeLeft;
			else
				balanceThisNode->avl_right = nodeLeft;
		} else
		{	/* No parent and the fact that this is an AVL tree implies only two possibilities
			 * 	a) deleting only node of tree.
			 *	b) deleting root of a height=2 tree that has only one child-node on its left and no other nodes.
			 * In either case, the tree height will reduce by 1 due to the deletion.
			 * Set tree root here. Tree height will be reduced later in the rebalancing for loop.
			 */
			lvt->avl_root = nodeLeft;
		}
		if (NULL != nodeLeft)
			nodeLeft->avl_parent = balanceThisNode;
	} else
	{	/* Replace node with in-order successor, and rearrange */
		next = nodeRight;
		while (TRUE)
		{
			left = next->avl_left;
			if (NULL != left)
			{
				next->descent_dir = TREE_DESCEND_LEFT;
				next = left;
			} else
				break;
		}
		/* at this point, "next" is the in-order successor of "node" */
		assert(NULL == next->avl_left);
		nextRight = next->avl_right;
		nextParent = next->avl_parent;
		if (nextParent != node)
		{
			balanceThisNode = nextParent;
			assert(nextParent->descent_dir
				== ((nextParent->avl_left == next) ? TREE_DESCEND_LEFT : TREE_DESCEND_RIGHT));
			if (TREE_DESCEND_LEFT == nextParent->descent_dir)
				nextParent->avl_left = nextRight;
			else
				nextParent->avl_right = nextRight;
			if (NULL != nextRight)
				nextRight->avl_parent = nextParent;
			next->avl_right = nodeRight;
			nodeRight->avl_parent = next;
		} else
		{
			assert(nextParent->avl_right == next);
			balanceThisNode = next;
		}
		assert(NULL != balanceThisNode);
		next->avl_left = nextLeft = nodeLeft;
		if (NULL != nextLeft)
			nextLeft->avl_parent = next;
		next->avl_parent = nodeParent;
		next->balance = node->balance;
		next->descent_dir = TREE_DESCEND_RIGHT;
		if (NULL != nodeParent)
		{
			assert(nodeParent->descent_dir
				== ((nodeParent->avl_left == node) ? TREE_DESCEND_LEFT : TREE_DESCEND_RIGHT));
			if (TREE_DESCEND_LEFT == nodeParent->descent_dir)
				nodeParent->avl_left = next;
			else
				nodeParent->avl_right = next;
		} else
			lvt->avl_root = next;	/* root of avl tree changing from "node" to "next" */
	}
	/* At this point "balanceThisNode" points to the first unbalanced node (from the bottom of the tree path)
	 * which might also need a rotation (single or double). The rotations need to bubble up the tree hence the for loop.
	 */
	for ( ; ; balanceThisNode = nextBalanceThisNode)
	{	/* Balancing act */
		if (NULL == balanceThisNode)
		{	/* Went past root of tree as part of balance factor adjustments. Tree height has reduced. */
			TREE_DEBUG3("Tree height reduced from %d to %d\n", lvt->avl_height, (lvt->avl_height - 1));
			assert(lvt->avl_height > 0);
			lvt->avl_height--;
			break;
		}
		balanceFactor = balanceThisNode->descent_dir;
		curNodeBalance = balanceThisNode->balance;
		if (TREE_IS_BALANCED(curNodeBalance))
		{	/* Found a balanced node in the tree path. Fix its balance to account for the decrease in subtree height.
			 * No more tree rotations needed so break out of the for loop.
			 */
			balanceThisNode->balance = TREE_INVERT_BALANCE_FACTOR(balanceFactor);
			break;
		}
		nextBalanceThisNode = balanceThisNode->avl_parent;
		assert((NULL == nextBalanceThisNode)
			|| nextBalanceThisNode->descent_dir
				== ((nextBalanceThisNode->avl_left == balanceThisNode) ? TREE_LEFT_HEAVY : TREE_RIGHT_HEAVY));
		if (curNodeBalance == balanceFactor)
		{	/* The longer subtree is the one which had its height reduce so this node now becomes balanced.
			 * So no rotation needed for this node, but the height of this node has now reduced so we still
			 * need to trace back up the tree to see if this causes any more height imbalances.
			 */
			balanceThisNode->balance = TREE_BALANCED;
			continue;
		}
		/* balanceThisNode->balance == inverse of balanceFactor.
		 *
		 * The subtree at balanceThisNode is already heavy on one side and a node delete has caused
		 * the other side to reduce one more level increasing the height difference between the two
		 * subtrees to be 2 and therefore requires a rebalance (using tree rotation).
		 */
		rebalanceNode = balanceThisNode;
		if (TREE_LEFT_HEAVY == balanceFactor)
		{
			balanceFactor = TREE_RIGHT_HEAVY;
			anchorNode = rebalanceNode->avl_right;
		} else
		{
			balanceFactor = TREE_LEFT_HEAVY;
			anchorNode = rebalanceNode->avl_left;
		}
		treeBalanced = TREE_IS_BALANCED(anchorNode->balance);
		if (treeBalanced || (anchorNode->balance == balanceFactor))
		{
			newRoot = lvAvlTreeSingleRotation(rebalanceNode, anchorNode, balanceFactor);
			assert((treeBalanced && TREE_IS_NOT_BALANCED(rebalanceNode->balance)
							&& TREE_IS_NOT_BALANCED(anchorNode->balance))
				|| (TREE_IS_BALANCED(rebalanceNode->balance) && TREE_IS_BALANCED(anchorNode->balance)));
		} else
			newRoot = lvAvlTreeDoubleRotation(rebalanceNode, anchorNode, balanceFactor);
		rebalanceNodeParent = nextBalanceThisNode;
		newRoot->avl_parent = rebalanceNodeParent;
		if (NULL != rebalanceNodeParent)
		{	/* Point parent to new root of the rotated subtree */
			assert(rebalanceNodeParent->descent_dir
				== ((rebalanceNodeParent->avl_left == rebalanceNode) ? TREE_DESCEND_LEFT : TREE_DESCEND_RIGHT));
			if (TREE_DESCEND_LEFT == rebalanceNodeParent->descent_dir)
				rebalanceNodeParent->avl_left = newRoot;
			else
				rebalanceNodeParent->avl_right = newRoot;
		} else
			lvt->avl_root = newRoot;
		if (treeBalanced)
		{	/* If tree is balanced at "anchorNode" before the rotation, the post-rotated tree height does NOT change.
			 * This means no more need to propagate the rebalancing up the tree. Break out of the loop now.
			 */
			 break;
		}
	}
	lvt->lastLookup.lastNodeLookedUp = NULL;	/* reset lastLookup clue since all bets are off after the delete */
}
