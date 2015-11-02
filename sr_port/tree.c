/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
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
#include "tree.h"
#include "gtm_stdio.h"
#include "min_max.h"
#include "lv_val.h"
#include "numcmp.h"
#include "arit.h"
#include "promodemo.h"	/* for "promote" & "demote" prototype */

#define	LV_TREE_INIT_ALLOC	 	 4
#define	LV_TREENODE_INIT_ALLOC		16
#define	LV_TREESUBSCR_INIT_ALLOC	16

/* the below assumes TREE_LEFT_HEAVY is 0, TREE_RIGHT_HEAVY is 1 and TREE_BALANCED is 2 */
LITDEF	int		tree_balance_invert[] = {TREE_RIGHT_HEAVY, TREE_LEFT_HEAVY, TREE_BALANCED};

LITREF	mval		literal_null;
LITREF	int4		ten_pwr[NUM_DEC_DG_1L+1];

static void lvAvlTreeNodeFltConv(treeNodeFlt *fltNode)
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
 *	0 if equal;
 *	< 0 if key of aSubscr < key of bNode;
 *	> 0 if key of aSubscr > key of bNode
 */

#define	LV_AVL_TREE_INTKEY_CMP(aSUBSCR, aSUBSCR_M1, bNODE, retVAL)							\
{															\
	int		a_m1, b_mvtype, a_sgn, b_sgn, exp_diff, m1_diff, m0_diff, b_m0;					\
	treeNodeFlt	*bNodeFlt;											\
	treeKeySubscr	fltSubscr, *tmpASubscr;										\
															\
	/* aSUBSCR is a number */											\
	assert(TREE_KEY_SUBSCR_IS_CANONICAL(aSUBSCR->mvtype));								\
	assert(MVTYPE_IS_INT(aSUBSCR->mvtype));										\
	b_mvtype = bNODE->key_mvtype;											\
	assert(!TREE_KEY_SUBSCR_IS_CANONICAL(b_mvtype)									\
		|| (MVTYPE_IS_NUMERIC(b_mvtype) && !MVTYPE_IS_NUM_APPROX(b_mvtype)));					\
	assert(MVTYPE_IS_NUMERIC(aSUBSCR->mvtype) && !MVTYPE_IS_NUM_APPROX(aSUBSCR->mvtype));				\
	assert(aSUBSCR_M1 == aSUBSCR->m[1]);										\
	if (MV_INT & b_mvtype)												\
	{														\
		bNodeFlt = (treeNodeFlt *)bNODE;									\
		retVAL = aSUBSCR_M1 - bNodeFlt->key_m0;									\
	} else if (TREE_KEY_SUBSCR_IS_CANONICAL(b_mvtype))								\
	{	/* Both aSUBSCR and bNODE are numbers */								\
		b_mvtype &= MV_INT;											\
		/* aSUBSCR is MV_INT but bNODE is not. Promote aSubscr to float representation before comparing		\
		 * it with bNODE. Cannot touch input mval since non-lv code expects MV_INT value to be stored in	\
		 * integer format only (and not float format). So use a temporary mval for comparison.			\
		 */													\
		fltSubscr.m[1] = aSUBSCR_M1;										\
		tmpASubscr = &fltSubscr;										\
		promote(tmpASubscr);	/* Note this modifies tmpASubscr->m[1] so no longer can use aSUBSCR_M1 below */	\
		assert(tmpASubscr->e || !tmpASubscr->m[1]);								\
		bNodeFlt = (treeNodeFlt *)bNODE;									\
		if (b_mvtype && !bNodeFlt->key_flags.key_bits.key_iconv)						\
		{													\
			lvAvlTreeNodeFltConv(bNodeFlt);									\
			assert(bNodeFlt->key_flags.key_bits.key_iconv);							\
		}													\
		a_sgn = (0 == tmpASubscr->sgn) ? 1 : -1; /* 1 if positive, -1 if negative */				\
		b_sgn = (0 == bNodeFlt->key_flags.key_bits.key_sgn) ? 1 : -1;						\
		/* Check sign. If different we are done right there */							\
		if (a_sgn != b_sgn)											\
			retVAL = a_sgn;											\
		else													\
		{	/* Signs equal; compare exponents for magnitude and adjust sense depending on sign. */		\
			exp_diff = tmpASubscr->e - bNodeFlt->key_flags.key_bits.key_e;					\
			if (exp_diff)											\
				retVAL = (exp_diff * a_sgn);								\
			else												\
			{	/* Signs and exponents equal; compare magnitudes.  */					\
				/* First, compare high-order 9 digits of magnitude.  */					\
				a_m1 = tmpASubscr->m[1];								\
				m1_diff = a_m1 - bNodeFlt->key_m1;							\
				if (m1_diff)										\
					retVAL = (m1_diff * a_sgn);							\
				else											\
				{	/* High-order 9 digits equal; if not zero, compare low-order 9 digits.  */	\
					if (0 == a_m1)	/* zero special case */						\
						retVAL = 0;								\
					else										\
					{										\
						b_m0 = (b_mvtype ? 0 : bNodeFlt->key_m0);				\
						m0_diff = tmpASubscr->m[0] - b_m0;					\
						if (m0_diff)								\
							retVAL = (m0_diff * a_sgn);					\
						else									\
						{	/* Signs, exponents, high-order & low-order magnitudes equal */	\
							retVAL = 0;							\
						}									\
					}										\
				}											\
			}												\
		}													\
	} else														\
		retVAL = -1; /* aSubscr is a number, but bNODE is a string */						\
}

#define	LV_AVL_TREE_NUMKEY_CMP(aSUBSCR, bNODE, retVAL)									\
{															\
	int		b_mvtype, a_sgn, b_sgn, exp_diff, m1_diff, m0_diff, a_m1, b_m0;					\
	treeNodeFlt	*bNodeFlt;											\
															\
	/* aSUBSCR is a number */											\
	assert(TREE_KEY_SUBSCR_IS_CANONICAL(aSUBSCR->mvtype));								\
	assert(!MVTYPE_IS_INT(aSUBSCR->mvtype));									\
	b_mvtype = bNODE->key_mvtype;											\
	assert(!TREE_KEY_SUBSCR_IS_CANONICAL(b_mvtype)									\
		|| (MVTYPE_IS_NUMERIC(b_mvtype) && !MVTYPE_IS_NUM_APPROX(b_mvtype)));					\
	assert(MVTYPE_IS_NUMERIC(aSUBSCR->mvtype) && !MVTYPE_IS_NUM_APPROX(aSUBSCR->mvtype));				\
	if (TREE_KEY_SUBSCR_IS_CANONICAL(b_mvtype))									\
	{	/* Both aSUBSCR and bNODE are numbers */								\
		b_mvtype &= MV_INT;											\
		bNodeFlt = (treeNodeFlt *)bNODE;									\
		if (b_mvtype && !bNodeFlt->key_flags.key_bits.key_iconv)						\
		{													\
			lvAvlTreeNodeFltConv(bNodeFlt);									\
			assert(bNodeFlt->key_flags.key_bits.key_iconv);							\
		}													\
		a_sgn = (0 == aSUBSCR->sgn) ? 1 : -1; /* 1 if positive, -1 if negative */				\
		b_sgn = (0 == bNodeFlt->key_flags.key_bits.key_sgn) ? 1 : -1;						\
		/* Check sign. If different we are done right there */							\
		if (a_sgn != b_sgn)											\
			retVAL = a_sgn;											\
		else													\
		{	/* Signs equal; compare exponents for magnitude and adjust sense depending on sign. */		\
			exp_diff = aSUBSCR->e - bNodeFlt->key_flags.key_bits.key_e;					\
			if (exp_diff)											\
				retVAL = (exp_diff * a_sgn);								\
			else												\
			{	/* Signs and exponents equal; compare magnitudes.  */					\
				/* First, compare high-order 9 digits of magnitude.  */					\
				a_m1 = aSUBSCR->m[1];									\
				m1_diff = a_m1 - bNodeFlt->key_m1;							\
				if (m1_diff)										\
					retVAL = (m1_diff * a_sgn);							\
				else											\
				{	/* High-order 9 digits equal; if not zero, compare low-order 9 digits.  */	\
					if (0 == a_m1)	/* zero special case */						\
						retVAL = 0;								\
					else										\
					{										\
						b_m0 = (b_mvtype ? 0 : bNodeFlt->key_m0);				\
						m0_diff = aSUBSCR->m[0] - b_m0;						\
						if (m0_diff)								\
							retVAL = (m0_diff * a_sgn);					\
						else									\
						{	/* Signs, exponents, high-order & low-order magnitudes equal */	\
							retVAL = 0;							\
						}									\
					}										\
				}											\
			}												\
		}													\
	} else														\
		retVAL = -1; /* aSUBSCR is a number, but bNODE is a string */						\
}

#define	LV_AVL_TREE_STRKEY_CMP(aSUBSCR, aSUBSCR_ADDR, aSUBSCR_LEN, bNODE, retVAL)		\
{												\
	/* aSUBSCR is a string */								\
	assert(!TREE_KEY_SUBSCR_IS_CANONICAL(aSUBSCR->mvtype));					\
	assert(MVTYPE_IS_STRING(aSUBSCR->mvtype));						\
	if (TREE_KEY_SUBSCR_IS_CANONICAL(bNODE->key_mvtype))					\
		retVAL = 1;	/* aSUBSCR is a string, but bNODE is a number */		\
	else /* aSUBSCR and bNODE are both strings */						\
		MEMVCMP(aSUBSCR_ADDR, aSUBSCR_LEN, bNODE->key_addr, bNODE->key_len, retVAL);	\
}

#ifdef DEBUG
/* This function is currently DEBUG-only because no one is supposed to be calling it.
 * Callers are usually performance sensitive and should therefore use the LV_AVL_TREE_NUMKEY_CMP
 * or LV_AVL_TREE_STRKEY_CMP macros that this function in turn invokes.
 */
int lvAvlTreeKeySubscrCmp(treeKeySubscr *aSubscr, treeNode *bNode)
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
int lvAvlTreeNodeSubscrCmp(treeNode *aNode, treeNode *bNode)
{
	int		a_mvtype, b_mvtype, retVal;
	int		a_sgn, b_sgn, exp_diff, m1_diff, m0_diff, a_m1, b_m1, a_m0, b_m0;
	treeNodeFlt	*aNodeFlt, *bNodeFlt;

	a_mvtype = aNode->key_mvtype;
	b_mvtype = bNode->key_mvtype;
	if (TREE_KEY_SUBSCR_IS_CANONICAL(a_mvtype))
	{	/* aSubscr is a number */
		assert(!MVTYPE_IS_STRING(a_mvtype));
		if (TREE_KEY_SUBSCR_IS_CANONICAL(b_mvtype))
		{	/* Both aNode and bNode are numbers */
			assert(!MVTYPE_IS_STRING(b_mvtype));
			aNodeFlt = (treeNodeFlt *)aNode;
			bNodeFlt = (treeNodeFlt *)bNode;
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

/* Return first (smallest collating) key in avl tree. Returns NULL if NO key in tree */
treeNode *lvAvlTreeFirst(tree *lvt)
{
	treeNode	*avl_first, *next;

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
treeNode *lvAvlTreeLast(tree *lvt)
{
	treeNode	*avl_last, *next;

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
treeNode *lvAvlTreePrev(treeNode *node)
{
	treeNode	*prev, *tmp, *parent;

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
treeNode *lvAvlTreeKeyPrev(tree *lvt, treeKeySubscr *key)
{
	treeNode	*node, *tmp, *prev, *parent;

	node = lvAvlTreeLookup(lvt, key, &parent);
	if (TREE_NO_SUCH_KEY != node)	/* most common case */
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
treeNode *lvAvlTreeNext(treeNode *node)
{
	treeNode	*next, *tmp, *parent;
	DEBUG_ONLY(tree	*lvt;)

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
treeNode *lvAvlTreeKeyNext(tree *lvt, treeKeySubscr *key)
{
	treeNode	*node, *tmp, *next, *parent;

	node = lvAvlTreeLookup(lvt, key, &parent);
	if (TREE_NO_SUCH_KEY != node)	/* most common case */
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

static treeNode *lvAvlTreeSingleRotation(treeNode *rebalanceNode, treeNode *anchorNode, int4 balanceFactor)
{
	treeNode	*newRoot, *node;

	TREE_DEBUG1("DOING SINGLE ROTATION\n");

	assert((TREE_LEFT_HEAVY == balanceFactor) || (TREE_RIGHT_HEAVY ==  balanceFactor));
	if (TREE_LEFT_HEAVY == balanceFactor)
	{	/* Left-Left path rotate */
		assert(rebalanceNode->avl_left == anchorNode);
		node = anchorNode->avl_right;
		rebalanceNode->avl_left = node;
		anchorNode->avl_right = rebalanceNode;
	} else
	{	/* Right-Right path rotate */
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

static treeNode *lvAvlTreeDoubleRotation(treeNode *rebalanceNode, treeNode *anchorNode, int4 balanceFactor)
{
	treeNode	*newRoot, *node, *rebalanceChild, *anchorChild;
	int		balance;

	TREE_DEBUG1("DOING DOUBLE ROTATION\n");

	assert((TREE_LEFT_HEAVY == balanceFactor) || (TREE_RIGHT_HEAVY ==  balanceFactor));
	if (TREE_LEFT_HEAVY == balanceFactor)
	{	/* Left-Right path rotate */
		assert(rebalanceNode->avl_left == anchorNode);
		newRoot = anchorNode->avl_right;
		rebalanceChild = newRoot->avl_right;
		rebalanceNode->avl_left = rebalanceChild;
		anchorChild = newRoot->avl_left;
		anchorNode->avl_right = anchorChild;
		newRoot->avl_left = anchorNode;
		newRoot->avl_right = rebalanceNode;
	} else
	{	/* Right-Left path rotate */
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

#ifdef DEBUG
static boolean_t lvAvlTreeLookupKeyCheck(treeKeySubscr *key)
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
#endif

treeNode *lvAvlTreeLookupInt(tree *lvt, treeKeySubscr *key, treeNode **lookupParent)
{
	int		cmp;
	int4		key_m1;
	treeNode	*node, *nextNode, *lastNodeMin, *lastNodeMax, *minNode, *maxNode, **nodePtr;
	treeNode	*parent, *tmpNode;
	treeSrchStatus	*lastLookup;

	assert(lvAvlTreeLookupKeyCheck(key));
	assert(TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
	assert(MVTYPE_IS_INT(key->mvtype));
	assert(NULL != lookupParent);
	TREE_DEBUG_ONLY(assert(lvTreeIsWellFormed(lvt));)
	key_m1 = key->m[1];
	/* See if node can be looked up easily from the lastLookup clue (without a tree traversal). If not do one. */
	lastLookup = &lvt->lastLookup;
	if (NULL != (tmpNode = lastLookup->lastNodeLookedUp))
	{
		assert(NULL != lvt->avl_root);	/* there better be an AVL tree if we have a non-zero clue */
		LV_AVL_TREE_INTKEY_CMP(key, key_m1, tmpNode, cmp);
		if (0 == cmp)
		{
			node = tmpNode;
			/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
			return node;
		} else if (0 < cmp)
		{
			if (NULL == tmpNode->avl_right)
			{
				maxNode = lastLookup->lastNodeMax;
				if (NULL != maxNode)
				{
					assert(0 < lvAvlTreeNodeSubscrCmp(maxNode, tmpNode));
					LV_AVL_TREE_INTKEY_CMP(key, key_m1, maxNode, cmp);
					if (0 == cmp)
					{
						node = maxNode;
						/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
						return node;
					} else if (0 > cmp)
					{
						parent = tmpNode;
						parent->descent_dir = TREE_DESCEND_RIGHT;
						*lookupParent = parent;
						return TREE_NO_SUCH_KEY;
					}
				} else
				{
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_RIGHT;
					*lookupParent = parent;
					return TREE_NO_SUCH_KEY;
				}
			}
		} else if (NULL == tmpNode->avl_left)
		{
			minNode = lastLookup->lastNodeMin;
			if (NULL != minNode)
			{
				assert(0 > lvAvlTreeNodeSubscrCmp(minNode, tmpNode));
				LV_AVL_TREE_INTKEY_CMP(key, key_m1, minNode, cmp);
				if (0 == cmp)
				{
					node = minNode;
					/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
					return node;
				} else if (0 < cmp)
				{
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_LEFT;
					*lookupParent = parent;
					return TREE_NO_SUCH_KEY;
				}
			} else
			{
				parent = tmpNode;
				parent->descent_dir = TREE_DESCEND_LEFT;
				*lookupParent = parent;
				return TREE_NO_SUCH_KEY;
			}
		}
	}
	parent = lvt->avl_root;
	if (NULL == parent)
	{
		*lookupParent = NULL;
		return TREE_NO_SUCH_KEY;
	}
	node = parent;
	lastNodeMin = (treeNode *)NULL;
	lastNodeMax = (treeNode *)NULL;
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
				nodePtr = &lastNodeMax;
				nextNode = node->avl_left;
			} else /* (cmp > 0) */
			{
				node->descent_dir = TREE_DESCEND_RIGHT;
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
			*nodePtr = parent;
		}
	} else
		lvt->lastLookup.lastNodeLookedUp = NULL;
	*lookupParent = parent;
	assert(TREE_NO_SUCH_KEY == (treeNode *)NULL);	/* ensure that if "node" is NULL, we are returning TREE_NO_SUCH_KEY */
	lvt->lastLookup.lastNodeMin = lastNodeMin;
	lvt->lastLookup.lastNodeMax = lastNodeMax;
	return node;
}

treeNode *lvAvlTreeLookupNum(tree *lvt, treeKeySubscr *key, treeNode **lookupParent)
{
	int		cmp;
	treeNode	*node, *nextNode, *lastNodeMin, *lastNodeMax, *minNode, *maxNode, **nodePtr;
	treeNode	*parent, *tmpNode;
	treeSrchStatus	*lastLookup;

	assert(lvAvlTreeLookupKeyCheck(key));
	assert(TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
	assert(!MVTYPE_IS_INT(key->mvtype));
	assert(NULL != lookupParent);
	TREE_DEBUG_ONLY(assert(lvTreeIsWellFormed(lvt));)
	/* See if node can be looked up easily from the lastLookup clue (without a tree traversal). If not do one. */
	lastLookup = &lvt->lastLookup;
	if (NULL != (tmpNode = lastLookup->lastNodeLookedUp))
	{
		assert(NULL != lvt->avl_root);	/* there better be an AVL tree if we have a non-zero clue */
		LV_AVL_TREE_NUMKEY_CMP(key, tmpNode, cmp);
		if (0 == cmp)
		{
			node = tmpNode;
			/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
			return node;
		} else if (0 < cmp)
		{
			if (NULL == tmpNode->avl_right)
			{
				maxNode = lastLookup->lastNodeMax;
				if (NULL != maxNode)
				{
					assert(0 < lvAvlTreeNodeSubscrCmp(maxNode, tmpNode));
					LV_AVL_TREE_NUMKEY_CMP(key, maxNode, cmp);
					if (0 == cmp)
					{
						node = maxNode;
						/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
						return node;
					} else if (0 > cmp)
					{
						parent = tmpNode;
						parent->descent_dir = TREE_DESCEND_RIGHT;
						*lookupParent = parent;
						return TREE_NO_SUCH_KEY;
					}
				} else
				{
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_RIGHT;
					*lookupParent = parent;
					return TREE_NO_SUCH_KEY;
				}
			}
		} else if (NULL == tmpNode->avl_left)
		{
			minNode = lastLookup->lastNodeMin;
			if (NULL != minNode)
			{
				assert(0 > lvAvlTreeNodeSubscrCmp(minNode, tmpNode));
				LV_AVL_TREE_NUMKEY_CMP(key, minNode, cmp);
				if (0 == cmp)
				{
					node = minNode;
					/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
					return node;
				} else if (0 < cmp)
				{
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_LEFT;
					*lookupParent = parent;
					return TREE_NO_SUCH_KEY;
				}
			} else
			{
				parent = tmpNode;
				parent->descent_dir = TREE_DESCEND_LEFT;
				*lookupParent = parent;
				return TREE_NO_SUCH_KEY;
			}
		}
	}
	parent = lvt->avl_root;
	if (NULL == parent)
	{
		*lookupParent = NULL;
		return TREE_NO_SUCH_KEY;
	}
	node = parent;
	lastNodeMin = (treeNode *)NULL;
	lastNodeMax = (treeNode *)NULL;
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
				nodePtr = &lastNodeMax;
				nextNode = node->avl_left;
			} else /* (cmp > 0) */
			{
				node->descent_dir = TREE_DESCEND_RIGHT;
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
			*nodePtr = parent;
		}
	} else
		lvt->lastLookup.lastNodeLookedUp = NULL;
	*lookupParent = parent;
	assert(TREE_NO_SUCH_KEY == (treeNode *)NULL);	/* ensure that if "node" is NULL, we are returning TREE_NO_SUCH_KEY */
	lvt->lastLookup.lastNodeMin = lastNodeMin;
	lvt->lastLookup.lastNodeMax = lastNodeMax;
	return node;
}

treeNode *lvAvlTreeLookupStr(tree *lvt, treeKeySubscr *key, treeNode **lookupParent)
{
	int		cmp, key_len;
	char		*key_addr;
	treeNode	*node, *nextNode, *lastNodeMin, *lastNodeMax, *minNode, *maxNode, **nodePtr;
	treeNode	*parent, *tmpNode;
	treeSrchStatus	*lastLookup;

	assert(lvAvlTreeLookupKeyCheck(key));
	assert(!TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype));
	assert(MVTYPE_IS_STRING(key->mvtype));
	assert(NULL != lookupParent);
	TREE_DEBUG_ONLY(assert(lvTreeIsWellFormed(lvt));)
	key_addr = key->str.addr;
	key_len = key->str.len;
	/* See if node can be looked up easily from the lastLookup clue (without a tree traversal). If not do one. */
	lastLookup = &lvt->lastLookup;
	if (NULL != (tmpNode = lastLookup->lastNodeLookedUp))
	{
		assert(NULL != lvt->avl_root);	/* there better be an AVL tree if we have a non-zero clue */
		LV_AVL_TREE_STRKEY_CMP(key, key_addr, key_len, tmpNode, cmp);
		if (0 == cmp)
		{
			node = tmpNode;
			/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
			return node;
		} else if (0 < cmp)
		{
			if (NULL == tmpNode->avl_right)
			{
				maxNode = lastLookup->lastNodeMax;
				if (NULL != maxNode)
				{
					assert(0 < lvAvlTreeNodeSubscrCmp(maxNode, tmpNode));
					LV_AVL_TREE_STRKEY_CMP(key, key_addr, key_len, maxNode, cmp);
					if (0 == cmp)
					{
						node = maxNode;
						/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
						return node;
					} else if (0 > cmp)
					{
						parent = tmpNode;
						parent->descent_dir = TREE_DESCEND_RIGHT;
						*lookupParent = parent;
						return TREE_NO_SUCH_KEY;
					}
				} else
				{
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_RIGHT;
					*lookupParent = parent;
					return TREE_NO_SUCH_KEY;
				}
			}
		} else if (NULL == tmpNode->avl_left)
		{
			minNode = lastLookup->lastNodeMin;
			if (NULL != minNode)
			{
				assert(0 > lvAvlTreeNodeSubscrCmp(minNode, tmpNode));
				LV_AVL_TREE_STRKEY_CMP(key, key_addr, key_len, minNode, cmp);
				if (0 == cmp)
				{
					node = minNode;
					/* "parent" or "parent->descent_dir" need not be set since node is non-NULL */
					return node;
				} else if (0 < cmp)
				{
					parent = tmpNode;
					parent->descent_dir = TREE_DESCEND_LEFT;
					*lookupParent = parent;
					return TREE_NO_SUCH_KEY;
				}
			} else
			{
				parent = tmpNode;
				parent->descent_dir = TREE_DESCEND_LEFT;
				*lookupParent = parent;
				return TREE_NO_SUCH_KEY;
			}
		}
	}
	parent = lvt->avl_root;
	if (NULL == parent)
	{
		*lookupParent = NULL;
		return TREE_NO_SUCH_KEY;
	}
	node = parent;
	lastNodeMin = (treeNode *)NULL;
	lastNodeMax = (treeNode *)NULL;
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
				nodePtr = &lastNodeMax;
				nextNode = node->avl_left;
			} else /* (cmp > 0) */
			{
				node->descent_dir = TREE_DESCEND_RIGHT;
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
			*nodePtr = parent;
		}
	} else
		lvt->lastLookup.lastNodeLookedUp = NULL;
	*lookupParent = parent;
	assert(TREE_NO_SUCH_KEY == (treeNode *)NULL);	/* ensure that if "node" is NULL, we are returning TREE_NO_SUCH_KEY */
	lvt->lastLookup.lastNodeMin = lastNodeMin;
	lvt->lastLookup.lastNodeMax = lastNodeMax;
	return node;
}

/* Return if a given key is found or not. "lookupParent" will be updated ONLY IF "node" is NOT found in tree */
treeNode *lvAvlTreeLookup(tree *lvt, treeKeySubscr *key, treeNode **lookupParent)
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

static void lvAvlTreeNodeDelete(tree *lvt, treeNode *node)
{
	boolean_t	treeBalanced;
	int4		balanceFactor, curNodeBalance;
	treeNode	*balanceThisNode, *nextBalanceThisNode, *anchorNode, *newRoot;
	treeNode	*next, *nextLeft, *nextRight, *nextParent, *left;
	treeNode	*nodeLeft, *nodeRight, *nodeParent;
	treeNode	*rebalanceNode, *rebalanceNodeParent;

	assert(IS_LVAVLTREENODE(((lv_val *)node)));	/* ensure input "node" is of type "treeNode *" or "treeNodeFlt *"
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
		{
			balanceThisNode->balance = TREE_INVERT_BALANCE_FACTOR(balanceFactor);
			break;
		}
		nextBalanceThisNode = balanceThisNode->avl_parent;
		assert((NULL == nextBalanceThisNode)
			|| nextBalanceThisNode->descent_dir
				== ((nextBalanceThisNode->avl_left == balanceThisNode) ? TREE_LEFT_HEAVY : TREE_RIGHT_HEAVY));
		if (curNodeBalance == balanceFactor)
		{
			balanceThisNode->balance = TREE_BALANCED;
			continue;
		}
		/* balanceThisNode->balance == inverse of balanceFactor.
		 *
		 * The subtree at balanceThisNode is already heavy on one side and a node delete has caused
		 * the other side to reduce one more level increasing the height difference between the two
		 * subtrees to be 2 and therefore requires a rebalance.
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

static treeNode *lvAvlTreeCloneSubTree(treeNode *node, tree *lvt, treeNode *avl_parent)
{
        lvTreeNodeVal	*dupVal;
        treeNode        *cloneNode, *left, *right;
        treeNode        *leftSubTree, *rightSubTree;
	tree		*lvt_child;
	lv_val		*base_lv;

	assert(NULL != node);
	cloneNode = lvtreenode_getslot(LVT_GET_SYMVAL(lvt));
	/* The following is optimized to do the initialization of just the needed structure members. For that it assumes a
	 * particular "treeNode" structure layout. The assumed layout is asserted so any changes to the layout will
	 * automatically show an issue here and cause the below initialization to be accordingly reworked.
	 */
	assert(0 == OFFSETOF(treeNode, v));
	assert(OFFSETOF(treeNode, v) + SIZEOF(cloneNode->v) == OFFSETOF(treeNode, sbs_child));
	assert(OFFSETOF(treeNode, sbs_child) + SIZEOF(cloneNode->sbs_child) == OFFSETOF(treeNode, tree_parent));
	assert(OFFSETOF(treeNode, tree_parent) + SIZEOF(cloneNode->tree_parent) == OFFSETOF(treeNode, key_mvtype));
	assert(OFFSETOF(treeNode, key_mvtype) + SIZEOF(cloneNode->key_mvtype) == OFFSETOF(treeNode, balance));
	assert(OFFSETOF(treeNode, balance) + SIZEOF(cloneNode->balance) == OFFSETOF(treeNode, descent_dir));
	assert(OFFSETOF(treeNode, descent_dir) + SIZEOF(cloneNode->descent_dir) == OFFSETOF(treeNode, key_len));
	assert(OFFSETOF(treeNode, key_len) + SIZEOF(cloneNode->key_len) == OFFSETOF(treeNode, key_addr));
	assert(OFFSETOF(treeNode, key_mvtype) + 8 == OFFSETOF(treeNode, key_addr));
	GTM64_ONLY(assert(OFFSETOF(treeNode, key_addr) + SIZEOF(cloneNode->key_addr) == OFFSETOF(treeNode, avl_left));)
	NON_GTM64_ONLY(
		assert(OFFSETOF(treeNode, key_addr) + SIZEOF(cloneNode->key_addr) == OFFSETOF(treeNode, filler_8));
		assert(OFFSETOF(treeNode, filler_8) + SIZEOF(cloneNode->filler_8) == OFFSETOF(treeNode, avl_left));
	)
	assert(OFFSETOF(treeNode, avl_left) + SIZEOF(cloneNode->avl_left) == OFFSETOF(treeNode, avl_right));
	assert(OFFSETOF(treeNode, avl_right) + SIZEOF(cloneNode->avl_right) == OFFSETOF(treeNode, avl_parent));
	assert(OFFSETOF(treeNode, avl_parent) + SIZEOF(cloneNode->avl_parent) == SIZEOF(treeNode));
	cloneNode->v = node->v;
	/* "cloneNode->sbs_child" initialized later */
	cloneNode->tree_parent = lvt;
	/* cloneNode->key_mvtype/balance/descent_dir/key_len all initialized in one shot.
	 * Note: We use "qw_num *" instead of "uint8 *" below because the former works on 32-bit platforms too.
	 */
	GTM64_ONLY(assert(IS_PTR_8BYTE_ALIGNED(&cloneNode->key_mvtype));)
	NON_GTM64_ONLY(assert(IS_PTR_4BYTE_ALIGNED(&cloneNode->key_mvtype));)
	GTM64_ONLY(assert(IS_PTR_8BYTE_ALIGNED(&node->key_mvtype));)
	NON_GTM64_ONLY(assert(IS_PTR_4BYTE_ALIGNED(&node->key_mvtype));)
	*(RECAST(qw_num *)&cloneNode->key_mvtype) = *(RECAST(qw_num *)&node->key_mvtype);
	cloneNode->key_addr = node->key_addr;
	NON_GTM64_ONLY(
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNode, filler_8, treeNodeFlt, key_m1));
		((treeNodeFlt *)cloneNode)->key_m1 = ((treeNodeFlt *)node)->key_m1;
	)
	cloneNode->avl_parent = avl_parent;
	lvt_child = node->sbs_child;
	base_lv = lvt->base_lv;
	if (NULL != lvt_child)
		lvTreeClone(lvt_child, cloneNode, base_lv);	/* initializes "cloneNode->sbs_child" */
	else
		cloneNode->sbs_child = NULL;
	left = node->avl_left;
	leftSubTree = (NULL != left) ? lvAvlTreeCloneSubTree(left, lvt, cloneNode) : NULL;
	cloneNode->avl_left = leftSubTree;
	right = node->avl_right;
	rightSubTree = (NULL != right) ? lvAvlTreeCloneSubTree(right, lvt, cloneNode) : NULL;
	cloneNode->avl_right = rightSubTree;
	return cloneNode;
}

static void lvAvlTreeWalkPreOrder(treeNode *node, treeNodeProcess process)
{
	treeNode	*left, *right;

	assert(NULL != node);
	process(node);
	left = node->avl_left;
	if (NULL != left)
		lvAvlTreeWalkPreOrder(left, process);
	right = node->avl_right;
	if (NULL != right)
		lvAvlTreeWalkPreOrder(right, process);
	return;
}

static void lvAvlTreeWalkPostOrder(treeNode *node, treeNodeProcess process)
{
	treeNode	*left, *right;

	assert(NULL != node);
	left = node->avl_left;
	if (NULL != left)
		lvAvlTreeWalkPostOrder(left, process);
	right = node->avl_right;
	if (NULL != right)
		lvAvlTreeWalkPostOrder(right, process);
	process(node);
	return;
}

static int lvAvlTreeNodeHeight(treeNode *node)
{
	int leftSubTreeHeight, rightSubTreeHeight;

	if (NULL != node)
	{
		leftSubTreeHeight = lvAvlTreeNodeHeight(node->avl_left);
		rightSubTreeHeight = lvAvlTreeNodeHeight(node->avl_right);

		return(MAX(leftSubTreeHeight, rightSubTreeHeight) + 1);
	}
	return 0;
}

#ifdef DEBUG
static boolean_t lvAvlTreeNodeIsWellFormed(tree *lvt, treeNode *node)
{
	int 		leftSubTreeHeight, rightSubTreeHeight;
	boolean_t	leftWellFormed, rightWellFormed;
	treeNode	*left, *right;
	tree		*sbs_child;

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
#endif

tree *lvTreeCreate(treeNode *sbs_parent, int sbs_depth, lv_val *base_lv)
{
	tree		*newTree;

#	ifdef DEBUG
	static boolean_t	first_tree_create = TRUE;

	if (first_tree_create)
	{
		/* This is the FIRST tree that this process is creating. Do a few per-process one-time assertions first. */
		assert(0 == TREE_LEFT_HEAVY);	/* tree_balance_invert array definition depends on this */
		assert(1 == TREE_RIGHT_HEAVY);	/* tree_balance_invert array definition depends on this */
		assert(2 == TREE_BALANCED);	/* tree_balance_invert array definition depends on this */
		assert(TREE_DESCEND_LEFT == TREE_LEFT_HEAVY);	/* they are interchangeably used for performance reasons */
		assert(TREE_DESCEND_RIGHT == TREE_RIGHT_HEAVY); /* they are interchangeably used for performance reasons */

		/* A lot of the lv functions are passed in an (lv_val *) which could be a (treeNode *) as well.
		 * For example, in op_kill.c, if "kill a" is done, an "lv_val *" (corresponding to the base local variable "a")
		 * is passed whereas if "kill a(1,2)" is done, a "treeNode *" (corresponding to the subscripted node "a(1,2)")
		 * is passed. From the input pointer, we need to determine in op_kill.c whether it is a "lv_val *" or "treeNode *".
		 * Assuming "curr_lv" is the input pointer of type "lv_val *", to find out if it is a pointer to an lv_val or
		 * treeNode, one checks the ident of the parent. If it is MV_SYM, it means the pointer is to an lv_val (base
		 * variable) and if not it is a treeNode pointer. The code will look like the below.
		 *
		 *	if (MV_SYM == curr_lv->ptrs.val_ent.parent.sym->ident)
		 *	{	// curr_lv is a base var pointing to an "lv_val" structure
		 *		...
		 *	} else
		 *	{	// curr_lv is a pointer to a treeNode structure whose parent.sym field points to a "tree" structure
		 *		assert(MV_LV_TREE == curr_lv->ptrs.val_ent.parent.sym->ident);
		 *		...
		 *	}
		 *
		 * Note : The MV_SYM == check above has now been folded into a IS_LV_VAL_PTR macro in lv_val.h
		 *
		 * In order for the above check to work, one needs to ensure that that an lv_val.ptrs.val_ent.parent.sym
		 * is at the same offset and size as a treeNode.tree_parent and that a symval.ident is at the same offset and
		 * size as a tree.ident.
		 *
		 * Similarly, in op_fndata.c we could be passed in a base lv_val * or a subscripted lv_val (i.e. treeNode *)
		 * and want to find out if this lv_val has any children. To do this we would normally need to check
		 * if the input is an lv_val or a treeNode pointer and access curr_lv->ptrs.val_ent.children or
		 * curr_lv->sbs_child respectively. To avoid this additional if checks, we ensure that both are at the
		 * same offset and have the same size.
		 *
		 * We therefore ensure that all fields below in the same line should be at same structure offset
		 * AND have same size as each other. Note: all of this might not be necessary in current code but
		 * might be useful in the future. Since it is easily possible for us to ensure this, we do so right now.
		 *
		 *	                  treeNode.tree_parent == lv_val.ptrs.val_ent.parent.sym
		 *	                  treeNode.sbs_child   == lv_val.ptrs.val_ent.children
		 *	tree.ident     == symval.ident         == lv_val.v.mvtype
		 *	tree.sbs_depth == symval.sbs_depth
		 *	                  treeNode.v           == lv_val.v
		 */
		/*	                  treeNode.tree_parent == lv_val.ptrs.val_ent.parent.sym */
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNode, tree_parent, lv_val, ptrs.val_ent.parent.sym));
		/*	                  treeNode.sbs_child   == lv_val.ptrs.val_ent.children */
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNode, sbs_child, lv_val, ptrs.val_ent.children));
		/*	tree.ident     == symval.ident         == lv_val.v.mvtype */
		assert(IS_OFFSET_AND_SIZE_MATCH(tree, ident, symval, ident));
		assert(IS_OFFSET_AND_SIZE_MATCH(tree, ident, lv_val, v.mvtype));
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
		/*	tree.sbs_depth == symval.sbs_depth */
		assert(IS_OFFSET_AND_SIZE_MATCH(tree, sbs_depth, symval, sbs_depth));
		/*	                  treeNode.v           == lv_val.v */
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNode, v, lv_val, v));
		/* Assert that treeNodeFlt and treeNodeStr structures have ALMOST the same layout.
		 * This makes them interchangeably usable with care on the non-intersecting fields.
		 */
		assert(SIZEOF(treeNodeFlt) == SIZEOF(treeNodeStr));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeFlt, v, treeNodeStr, v));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeFlt, sbs_child, treeNodeStr, sbs_child));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeFlt, tree_parent, treeNodeStr, tree_parent));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeFlt, key_mvtype, treeNodeStr, key_mvtype));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeFlt, balance, treeNodeStr, balance));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeFlt, descent_dir, treeNodeStr, descent_dir));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeFlt, avl_left, treeNodeStr, avl_left));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeFlt, avl_right, treeNodeStr, avl_right));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeFlt, avl_parent, treeNodeStr, avl_parent));
		/* Assert that treeNodeStr and treeNode structures have EXACTLY the same layout.
		 * This makes them interchangeably usable.
		 */
		assert(SIZEOF(treeNodeStr) == SIZEOF(treeNode));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeStr, v, treeNode, v));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeStr, sbs_child, treeNode, sbs_child));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeStr, tree_parent, treeNode, tree_parent));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeStr, key_mvtype, treeNode, key_mvtype));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeStr, balance, treeNode, balance));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeStr, descent_dir, treeNode, descent_dir));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeStr, key_len, treeNode, key_len));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeStr, key_addr, treeNode, key_addr));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeStr, avl_left, treeNode, avl_left));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeStr, avl_right, treeNode, avl_right));
		assert(IS_OFFSET_AND_SIZE_MATCH(treeNodeStr, avl_parent, treeNode, avl_parent));
		first_tree_create = FALSE;
	}
#	endif
	newTree = lvtree_getslot(LV_GET_SYMVAL(base_lv));
	newTree->ident = MV_LV_TREE;
	assert(0 < sbs_depth);
	newTree->sbs_depth = sbs_depth;
	newTree->avl_height = 0;
	newTree->base_lv = base_lv;
	newTree->avl_root = NULL;
	newTree->sbs_parent = sbs_parent;
	sbs_parent->sbs_child = newTree;
	newTree->lastLookup.lastNodeLookedUp = NULL;
	return(newTree);
}

/* Inserts a node into the LV AVL tree.
 *
 * Key points to note for AVL tree insertion (from http://neil.brown.name/blog/20041124101820) are
 *
 * We only need to rotate if as part of the lookup/insert, we step down to a longer subtree, and then all
 * subsequent nodes are balanced, without longer or shorter subtrees. The actual rotation that happens
 * (if needed) will depend on what happens immediately after stepping down to a longer subtree. If the
 * next step is in the same direction as the step into a larger subtree, then a single rotation will be
 * needed. If the next step is in the other direction, then a double rotation will be needed.
 *
 * The important data that we need is that part of the path from the last unbalanced node to the insertion
 * point. All nodes in this list other than the first will be balanced, including the newly inserted
 * node. All of these nodes will either take part in a rotation, or will need to have their balance value
 * changed (though the new node won't need it's balance changed, it could take part in a rotation though).
 *
 * This path may not actually start with an unbalanced node. If every node in the path from the root is
 * balanced, then that whole path needs to be recorded, and every node will have it's balance changed.
 */
treeNode *lvTreeNodeInsert(tree *lvt, treeKeySubscr *key, treeNode *parent)
{
	treeNode 	*node, *t_root, *tmp_parent, *anchorNode, *tmpNode, **nodePtr;
	treeNode	*rebalanceNode, *rebalanceNodeParent;
	treeNodeFlt	*fltNode;
	int		balanceFactor, curBalance, cmp, key_mvtype;
	treeSrchStatus	*lastLookup;
	DEBUG_ONLY(treeNode	*dbg_node);

	assert(MV_DEFINED(key));
	/* At time of node insertion into tree, ensure that if we have MV_CANONICAL bit set, then MV_STR bit is not set.
	 * This makes it clear that the node key is a number and does not have the string representation constructed.
	 */
	assert(!TREE_KEY_SUBSCR_IS_CANONICAL(key->mvtype) || MV_IS_NUMERIC(key) && !MV_IS_STRING(key));
	assert(TREE_NO_SUCH_KEY == lvAvlTreeLookup(lvt, key, &tmp_parent));
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
	{	/* Initialize treeNodeFlt structure */
		fltNode = (treeNodeFlt *)node;
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
	{	/* Initialize treeNode structure */
		node->key_mvtype = MV_STR;	/* do not use input mvtype as it might have MV_NM or MV_NUM_APPROX bits set */
		node->key_len = key->str.len;
		node->key_addr = key->str.addr;
	}
	node->balance = TREE_BALANCED;
	/* node->descent_dir is initialized later when this node is part of a lvAvlTreeLookup operation.
	 * this field is not used by anyone until then so ok not to initialize it here.
	 */
	/* Maintain clue, do height-balancing tree rotations as needed to maintain avl property etc. */
	/* Update lastLookup clue to reflect the newly inserted node. Note that this clue will continue
	 * to be valid inspite of any single or double rotations that happen as part of the insert below.
	 */
	lastLookup = &lvt->lastLookup;
	lastLookup->lastNodeLookedUp = node;	/* first part of lastLookup clue update */
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

void	lvTreeNodeDelete(tree *lvt, treeNode *node)
{
	assert(NULL != lvt);
	lvAvlTreeNodeDelete(lvt, node);		/* node in avl tree part of the lv subscript tree */
	/* Now that "node" has been removed from the integer-array or the AVL tree, it is safe to free it */
	LVTREENODE_FREESLOT(node);
	TREE_DEBUG_ONLY(assert(lvTreeIsWellFormed(lvt));)
	return;
}

/* Returns the collated successor of the input "key" taking into account the currently effective null collation scheme */
treeNode *lvTreeKeyCollatedNext(tree *lvt, treeKeySubscr *key)
{
	treeNode	*node;
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
treeNode *lvTreeNodeCollatedNext(treeNode *node)
{
	boolean_t	get_next;
	tree		*lvt;
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

tree *lvTreeClone(tree *lvt, treeNode *sbs_parent, lv_val *base_lv)
{
        tree		*cloneTree;
	treeNode	*avl_root;

	cloneTree = lvtree_getslot(LV_GET_SYMVAL(base_lv));
	/* The following is optimized to do the initialization of just the needed structure members. For that it assumes a
	 * particular "tree" structure layout. The assumed layout is asserted so any changes to the layout will automatically
	 * show an issue here and cause the below initialization to be accordingly reworked.
	 */
	assert(8 == OFFSETOF(tree, base_lv));
	assert(OFFSETOF(tree, base_lv) + SIZEOF(lvt->base_lv) == OFFSETOF(tree, avl_root));
	assert(OFFSETOF(tree, avl_root) + SIZEOF(lvt->avl_root) == OFFSETOF(tree, sbs_parent));
	assert(OFFSETOF(tree, sbs_parent) + SIZEOF(lvt->sbs_parent) == OFFSETOF(tree, lastLookup));
	assert(OFFSETOF(tree, lastLookup) + SIZEOF(lvt->lastLookup) == SIZEOF(tree));
	/* Note: We use "qw_num *" instead of "uint8 *" below because the former works on 32-bit platforms too. */
	GTM64_ONLY(assert(IS_PTR_8BYTE_ALIGNED(cloneTree));)
	NON_GTM64_ONLY(assert(IS_PTR_4BYTE_ALIGNED(cloneTree));)
	GTM64_ONLY(assert(IS_PTR_8BYTE_ALIGNED(lvt));)
	NON_GTM64_ONLY(assert(IS_PTR_4BYTE_ALIGNED(lvt));)
	*(qw_num *)cloneTree = *(qw_num *)lvt;
	cloneTree->base_lv = base_lv;
	cloneTree->sbs_parent = sbs_parent;
	sbs_parent->sbs_child = cloneTree;
	/* reset clue in cloned tree as source tree pointers are no longer relevant in cloned tree */
	cloneTree->lastLookup.lastNodeLookedUp = NULL;
	if (NULL != (avl_root = lvt->avl_root))
        	cloneTree->avl_root = lvAvlTreeCloneSubTree(avl_root, cloneTree, NULL);
	else
        	cloneTree->avl_root = NULL;
        return cloneTree;
}

void lvTreeWalkPreOrder(tree *lvt, treeNodeProcess process)
{
	treeNode	*avl_root;

	assert(NULL != lvt);
	if (NULL != (avl_root = lvt->avl_root))
		lvAvlTreeWalkPreOrder(avl_root, process);
	return;
}

void lvTreeWalkPostOrder(tree *lvt, treeNodeProcess process)
{
	treeNode	*avl_root;

	assert(NULL != lvt);
	if (NULL != (avl_root = lvt->avl_root))
		lvAvlTreeWalkPostOrder(avl_root, process);
	return;
}


#ifdef DEBUG
boolean_t lvTreeIsWellFormed(tree *lvt)
{
	treeNode	*avl_root, *tmpNode, *minNode, *maxNode, *sbs_parent, *avl_node, *node;
	treeSrchStatus	*lastLookup;
	tree		*tmplvt;
	int		nm_node_cnt, sbs_depth;
	lv_val		*base_lv;

	if (NULL != lvt)
	{
		/* Check lvt->ident */
		assert(MV_LV_TREE == lvt->ident);
		/* Check lvt->sbs_parent */
		sbs_parent = lvt->sbs_parent;
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
			/* Check lvt->lastLookup */
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
#endif

