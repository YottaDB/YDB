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

#ifndef _TREE_H
#define _TREE_H

#include <stdarg.h>

/* The "tree"     typedef defines an AVL tree. One such structure exists for each subscript level (starting from 0).
 * The "treeNode" typedef defines the layout of the tree node. One structure exists for each tree node in an AVL tree.
 * e.g. if "a" is a local variable that has the following nodes defined
 *	a(1)=1
 *	a(1,"a")=2
 *	a(1,"b")=3
 *	a(2,"c")=4
 * There are THREE treeStruct structures (i.e. AVL trees) underneath the base local variable "a".
 *	1) One for the base variable "a". This contains the keys/subscripts 1 and 2.
 *	2) One for the subscripted variable "a(1)". This contains the keys/subscripts "a" and "b".
 *	3) One for the subscripted variable "a(2)". This contains the key/subscript "c".
 * There are FIVE treeNode structures underneath the base local variable "a".
 *	1) One for subscript 1.
 *	2) One for subscript 2.
 *	3) One for subscript "a".
 *	4) One for subscript "b".
 *	5) One for subscript "c".
 *
 */

#define	LV_LOOKUP_DIRECTION_PREV	-1
#define	LV_LOOKUP_DIRECTION_NEXT	+1

typedef mval	lvTreeNodeVal;
typedef mval	treeKeySubscr;

/* A numeric key value stored in the AVL tree is represented in a treeNodeFlt structure.
 * Additionally, a string type key can also be stored in the same AVL tree. Instead of using an "mstr" type directly, we explicitly
 * define the necessary fields from the mstr (no char_len field) so we can save 8 bytes in the treeNode struct on 64-bit platforms).
 * This way both the numeric and string treeNode structures require 12 bytes thus have 8-byte alignment for the treeNode* typedefs
 * (on 64-bit platforms) without any wasted padding space. This means that any code that interprets the key in a treeNode structure
 * needs to examine the "key_mvtype" field and accordingly use a treeNodeFlt or treeNodeStr structure to get at the key.
 */
typedef struct treeNodeFltStruct
{
	lvTreeNodeVal		v;		/* If string, will point to stringpool location */
	struct treeStruct	*sbs_child;	/* pointer to treeStruct storing next level subscripts under this node */
	struct treeStruct	*tree_parent;	/* pointer to treeStruct under whose avl tree this node belongs to */
	unsigned short		key_mvtype;	/* will be <MV_NM> (if non-integer) or <MV_INT | MV_NM> (if integer) */
	signed char		balance;	/* height(left) - height(right). Can be -1, 0, or 1 */
	unsigned char		descent_dir;	/* direction of descent (LEFT = 0, RIGHT = 1) */
	/* the value of the numeric key is stored in the key_* members below */
	union
	{
		struct
		{
			unsigned char	key_sgne;	/* Contains sgn & e fields - fewer instructions to move this one byte
							 * rather than two bit fields when copying to/from an mval */
		} key_bytes;
		struct
		{
#			ifdef BIGENDIAN
			unsigned int	key_sgn : 1;	/* same as mval layout */
			unsigned int	key_e   : 7;	/* same as mval layout */
#			else
			unsigned int	key_e   : 7;	/* same as mval layout */
			unsigned int	key_sgn : 1;	/* same as mval layout */
#			endif
			unsigned int	key_iconv : 1;	/* 1 if (key_mvtype & MV_INT is TRUE) && (key_m0 field stores the
							 * mval->m[1] value of the integer before it was converted into the
							 * float representation.
							 */
			unsigned int	filler  :23;
		} key_bits;
	} key_flags;
	int4			key_m0;
	int4			key_m1;
	struct treeNodeStruct	*avl_left;	/* left AVL subtree at this subscript level */
	struct treeNodeStruct	*avl_right;	/* right AVL subtree at this subscript level */
	struct treeNodeStruct	*avl_parent;	/* parent node within the current subscript level AVL tree */
} treeNodeFlt;

typedef struct treeNodeStruct
{
	lvTreeNodeVal		v;		/* If string, will point to stringpool location */
	struct treeStruct	*sbs_child;	/* pointer to treeStruct storing next level subscripts under this node;
						 * overloaded as the free list pointer if this "treeNode" is in the free list.
						 */
	struct treeStruct	*tree_parent;	/* pointer to treeStruct under whose avl tree this node belongs to */
	unsigned short		key_mvtype;	/* will have MV_STR bit set */
	signed char		balance;	/* height(left) - height(right). Can be -1, 0, or 1 */
	unsigned char		descent_dir;	/* direction of descent (LEFT = 0, RIGHT = 1) */
	uint4			key_len;	/* byte length of string */
	char			*key_addr;	/* pointer to string */
	NON_GTM64_ONLY(uint4	filler_8;)	/* needed to ensure treeNodeFlt & treeNode structures have
						 * 	same size & layout for 32-bit platforms also */
	struct treeNodeStruct	*avl_left;	/* left AVL subtree at this subscript level */
	struct treeNodeStruct	*avl_right;	/* right AVL subtree at this subscript level */
	struct treeNodeStruct	*avl_parent;	/* parent node within the current subscript level AVL tree */
} treeNode;

/* Given a pointer to a "treeNode" structure, the mvtype field will tell us whether it is a treeNodeStr or treeNodeFlt type.
 * key_mvtype will have the MV_STR bit set in case of treeNodeStr and otherwise if treeNodeFlt.
 */
typedef	treeNode	treeNodeStr;

/* last lookup clue structure */
typedef struct
{
	treeNode	*lastNodeLookedUp;	/* pointer to the node that was last looked up using lvAvlTreeLookup function */
	treeNode	*lastNodeMin;		/* minimum value of key that can be underneath the last looked up node's subtree */
	treeNode	*lastNodeMax;		/* maximum value of key that can be underneath the last looked up node's subtree */
} treeSrchStatus;

typedef struct treeStruct
{
	unsigned short		ident;	    /* 2-byte field (same size as mvtype) set to the value MV_LV_TREE */
	unsigned short		sbs_depth;  /* == "n" => all nodes in current avl tree represent lvns with "n" subscripts */
	uint4			avl_height; /* Height of the AVL tree rooted at "avl_root" */
	struct lv_val_struct	*base_lv;   /* back pointer to base var lv_val (subscript depth 0) */
	treeNode		*avl_root;  /* pointer to the root of the AVL tree corresponding to this subscript level;
					     * overloaded as the free list pointer if this "tree" structure is in the free list.
					     */
	treeNode		*sbs_parent;/* pointer to parent (points to lv_val if sbs_depth==1; and treeNode if sbs_depth>1) */
	treeSrchStatus		lastLookup; /* clue to last node lookup in the AVL tree rooted at "avl_root" */
} tree;

/* This section defines macros for the AVL tree implementation */

#define	TREE_DESCEND_LEFT	0
#define	TREE_DESCEND_RIGHT	1

#define TREE_LEFT_HEAVY		0	/* should be same as TREE_DESCEND_LEFT (an implementation efficiency) */
#define TREE_RIGHT_HEAVY 	1	/* should be same as TREE_DESCEND_RIGHT (an implementation efficiency) */
#define TREE_BALANCED		2

#define TREE_IS_LEFT_HEAVY(bal)		(bal) == TREE_LEFT_HEAVY
#define TREE_IS_RIGHT_HEAVY(bal)	(bal) == TREE_RIGHT_HEAVY
#define TREE_IS_BALANCED(bal)		((bal) == TREE_BALANCED)
#define TREE_IS_NOT_BALANCED(bal)	(!(TREE_IS_BALANCED(bal)))

#define TREE_INVERT_BALANCE_FACTOR(bal) (tree_balance_invert[bal])

#define TREE_KEY_SUBSCR_IS_CANONICAL(A_MVTYPE)		(A_MVTYPE & MV_CANONICAL)
#define TREE_KEY_SUBSCR_SET_MV_CANONICAL_BIT(A_MVAL)	(A_MVAL)->mvtype |= MV_CANONICAL
#define TREE_KEY_SUBSCR_RESET_MV_CANONICAL_BIT(A_MVAL)	(A_MVAL)->mvtype &= MV_CANONICAL_OFF

#define MAX_BALANCE	1 /* the maximum difference in height of the left and right subtrees of a tree,
			   * balance > MAX_BALANCE will trigger a re-balance */

#define TREE_NO_SUCH_KEY	(treeNode *)NULL
#define TREE_SEARCH_IN_AVL_TREE	(treeNode *)-1

#define TREE_KEY_CREATE_SUCCESS	0
#define TREE_KEY_CREATE_ERROR	-1

#define	DEMOTE_IF_NEEDED_FALSE	FALSE
#define	DEMOTE_IF_NEEDED_TRUE	TRUE

#define	LV_FLT_NODE_GET_KEY(NODE, KEY)								\
{												\
	treeNodeFlt	*lcl_flt_node;								\
	int		lcl_mvtype;								\
												\
	lcl_flt_node = (treeNodeFlt *)NODE;							\
	lcl_mvtype = lcl_flt_node->key_mvtype;							\
	assert(MVTYPE_IS_NUMERIC(lcl_mvtype));							\
	if (MV_INT & lcl_mvtype)								\
		(KEY)->m[1] = lcl_flt_node->key_m0;						\
	else											\
	{											\
		assert(lcl_flt_node->key_flags.key_bits.key_e || !lcl_flt_node->key_m1);	\
		((mval_b *)(KEY))->sgne = lcl_flt_node->key_flags.key_bytes.key_sgne;		\
		(KEY)->m[0] = lcl_flt_node->key_m0;						\
		(KEY)->m[1] = lcl_flt_node->key_m1;						\
	}											\
	(KEY)->mvtype = lcl_mvtype;								\
}

#define	LV_STR_NODE_GET_KEY(NODE, KEY)			\
{							\
	assert(MVTYPE_IS_STRING(NODE->key_mvtype));	\
	(KEY)->mvtype = NODE->key_mvtype;		\
	(KEY)->str.len = NODE->key_len;			\
	(KEY)->str.addr = NODE->key_addr;		\
}

/* If NODE contains a numeric key, before returning the key's mval to non-lv code make sure we
 * reset any bit(s) that are set only in lv code. The only one at this point in time is MV_CANONICAL.
 */
#define	LV_NODE_GET_KEY(NODE, KEY_MVAL)				\
{								\
	if (TREE_KEY_SUBSCR_IS_CANONICAL(NODE->key_mvtype))	\
	{	/* "NODE" is of type "treeNodeFlt *" */		\
		LV_FLT_NODE_GET_KEY(NODE, KEY_MVAL);		\
		(KEY_MVAL)->mvtype &= MV_CANONICAL_OFF;		\
	} else	/* "NODE" is of type "treeNode *" */		\
		LV_STR_NODE_GET_KEY(NODE, KEY_MVAL);		\
}

/* Macro to return if a given "treeNode *" pointer is a null subscript string.
 * Input "node" could actually be any one of "treeNodeFlt *" or "treeNode *".
 * A null subscript string is of actual type "treeNode *". To minimize the checks, we check for the MV_STR bit in the mvtype.
 * This is asserted below. For "treeNodeFlt *", the MV_STR bit is guaranteed not to be set. That leaves us with "treeNode *" only.
 */
#define	LV_NODE_KEY_IS_STRING(NODE)	(DBG_ASSERT(NULL != NODE)					\
					MVTYPE_IS_STRING(NODE->key_mvtype))

#define	LV_NODE_KEY_IS_NULL_SUBS(NODE)	(LV_NODE_KEY_IS_STRING(NODE) && (0 == NODE->key_len))

typedef void (*treeNodeProcess)(treeNode *node);	/* function to process a tree node */

#ifdef DEBUG
int 		lvAvlTreeKeySubscrCmp(treeKeySubscr *aSubscr, treeNode *bNode);
int 		lvAvlTreeNodeSubscrCmp(treeNode *aNode, treeNode *bNode);
#endif
treeNode	*lvAvlTreeFirst(tree *lvt);
treeNode	*lvAvlTreeLast(tree *lvt);
treeNode 	*lvAvlTreePrev(treeNode *node);
treeNode 	*lvAvlTreeKeyPrev(tree *lvt, treeKeySubscr *key);
treeNode 	*lvAvlTreeNext(treeNode *node);
treeNode 	*lvAvlTreeKeyNext(tree *lvt, treeKeySubscr *key);
treeNode	*lvAvlTreeLookupInt(tree *lvt, treeKeySubscr *key, treeNode **lookupNode);
treeNode	*lvAvlTreeLookupNum(tree *lvt, treeKeySubscr *key, treeNode **lookupNode);
treeNode	*lvAvlTreeLookupStr(tree *lvt, treeKeySubscr *key, treeNode **lookupNode);
treeNode	*lvAvlTreeLookup(tree *lvt, treeKeySubscr *key, treeNode **lookupNode);

tree		*lvTreeCreate(treeNode *sbs_parent, int sbs_depth, struct lv_val_struct *base_lv);
treeNode	*lvTreeNodeInsert(tree *lvt, treeKeySubscr *key, treeNode *parent);
void		lvTreeNodeDelete(tree *lvt, treeNode *node);
treeNode	*lvTreeKeyCollatedNext(tree *lvt, treeKeySubscr *key);
treeNode	*lvTreeNodeCollatedNext(treeNode *node);
tree		*lvTreeClone(tree *lvt, treeNode *sbs_parent, struct lv_val_struct *base_lv);
void		lvTreeWalkPreOrder(tree *lvt, treeNodeProcess process);
void		lvTreeWalkPostOrder(tree *lvt, treeNodeProcess process);
boolean_t	lvTreeIsWellFormed(tree *lvt);

#define	LV_AVL_TREE_FIRST(LVT, NODE)		NODE = lvAvlTreeFirst(LVT)

#define	LV_AVL_TREE_LAST(LVT, NODE)						\
{										\
	treeNode *lcl_avl_root;							\
										\
	lcl_avl_root = (LVT)->avl_root;						\
	NODE = (NULL != lcl_avl_root) ? lvAvlTreeLast(lcl_avl_root) : NULL;	\
}

#	 ifdef TREE_DEBUG
#		 define print				printf
#		 define flushOutput			fflush(stdout)

#		 define TREE_DEBUG1(p)			{print(p); flushOutput;}
#		 define TREE_DEBUG2(p, q)		{print(p, q); flushOutput;}
#		 define TREE_DEBUG3(p, q, r)		{print(p, q, r); flushOutput;}
#		 define TREE_DEBUG4(p, q, r, s)		{print(p, q, r, s); flushOutput;}
#		 define TREE_DEBUG5(p, q, r, s, t)	{print(p, q, r, s, t); flushOutput;}

#		 define	TREE_DEBUG_ONLY(X)			X
#	 else

#		 define TREE_DEBUG1(p)
#		 define TREE_DEBUG2(p, q)
#		 define TREE_DEBUG3(p, q, r)
#		 define TREE_DEBUG4(p, q, r, s)
#		 define TREE_DEBUG5(p, q, r, s, t)

#		 define	TREE_DEBUG_ONLY(X)
#	 endif

#endif
