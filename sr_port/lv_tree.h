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

#ifndef _TREE_H
#define _TREE_H

#include <stdarg.h>

/* The "lvTree"     typedef defines an AVL tree. One such structure exists for each local variable subscript level (starting from 0)
 * The "lvTreeNode" typedef defines the layout of the tree node. One structure exists for each tree node in an AVL tree.
 * e.g. if "a" is a local variable that has the following nodes defined
 *	a(1)=1
 *	a(1,"a")=2
 *	a(1,"b")=3
 *	a(2,"c")=4
 * There is ONE "lv_val" structure corresponding to the base local variable "a".
 * There are THREE "lvTree" structures (i.e. AVL trees) underneath the base local variable "a".
 *	1) One for the base variable "a". This contains the keys/subscripts 1 and 2.
 *	2) One for the subscripted variable "a(1)". This contains the keys/subscripts "a" and "b".
 *	3) One for the subscripted variable "a(2)". This contains the key/subscript "c".
 * There are FIVE "lvTreeNode" structures underneath the base local variable "a".
 *	1) One for subscript 1.
 *	2) One for subscript 2.
 *	3) One for subscript "a".
 *	4) One for subscript "b".
 *	5) One for subscript "c".
 */

typedef mval	lvTreeNodeVal;
typedef mval	treeKeySubscr;

/* A numeric key value stored in the AVL tree is represented in a lvTreeNodeNum structure. Additionally, a string type key can
 * also be stored in the same AVL tree. Instead of using an "mstr" type directly, we explicitly define the necessary fields
 * from the mstr (no char_len field) so we can save 8 bytes in the lvTreeNode struct on 64-bit platforms). This way both the
 * numeric and string lvTreeNode structures require 12 bytes thus have 8-byte alignment for the lvTreeNode* typedefs (on 64-bit
 * platforms) without any wasted padding space. This means that any code that interprets the key in a lvTreeNode structure
 * needs to examine the "key_mvtype" field and accordingly use a lvTreeNodeNum or lvTreeNodeStr structure to get at the key.
 */
typedef struct lvTreeNodeNumStruct
{
	lvTreeNodeVal		v;		/* If string, will point to stringpool location */
	struct lvTreeStruct	*sbs_child;	/* pointer to lvTreeStruct storing next level subscripts under this node */
	struct lvTreeStruct	*tree_parent;	/* pointer to lvTreeStruct under whose avl tree this node belongs to */
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
} lvTreeNodeNum;

typedef struct treeNodeStruct
{
	lvTreeNodeVal		v;		/* If string, will point to stringpool location */
	struct lvTreeStruct	*sbs_child;	/* pointer to lvTreeStruct storing next level subscripts under this node;
						 * overloaded as the free list pointer if this "lvTreeNode" is in the free list.
						 */
	struct lvTreeStruct	*tree_parent;	/* pointer to lvTreeStruct under whose avl tree this node belongs to */
	unsigned short		key_mvtype;	/* will have MV_STR bit set */
	signed char		balance;	/* height(left) - height(right). Can be -1, 0, or 1 */
	unsigned char		descent_dir;	/* direction of descent (LEFT = 0, RIGHT = 1) */
	uint4			key_len;	/* byte length of string */
	char			*key_addr;	/* pointer to string */
	NON_GTM64_ONLY(uint4	filler_8byte;)	/* needed to ensure lvTreeNodeNum & lvTreeNode structures have
						 * 	same size & layout for 32-bit platforms also */
	struct treeNodeStruct	*avl_left;	/* left AVL subtree at this subscript level */
	struct treeNodeStruct	*avl_right;	/* right AVL subtree at this subscript level */
	struct treeNodeStruct	*avl_parent;	/* parent node within the current subscript level AVL tree */
} lvTreeNode;

/* Given a pointer to a "lvTreeNode" structure, the mvtype field will tell us whether it is a lvTreeNodeStr or lvTreeNodeNum type.
 * key_mvtype will have the MV_STR bit set in case of lvTreeNodeStr and otherwise if lvTreeNodeNum.
 */
typedef	lvTreeNode	lvTreeNodeStr;

/* last lookup clue structure */
typedef struct
{
	lvTreeNode	*lastNodeLookedUp;	/* pointer to the node that was last looked up using lvAvlTreeLookup function */
	lvTreeNode	*lastNodeMin;		/* minimum value of key that can be underneath the last looked up node's subtree */
	lvTreeNode	*lastNodeMax;		/* maximum value of key that can be underneath the last looked up node's subtree */
} treeSrchStatus;

typedef struct lvTreeStruct
{	/* Note if first 3 fields are disturbed, make sure to fix LV_CLONE_TREE macro which references them as a group */
	unsigned short		ident;	    /* 2-byte field (same size as mvtype) set to the value MV_LV_TREE */
	unsigned short		sbs_depth;  /* == "n" => all nodes in current avl tree represent lvns with "n" subscripts */
	uint4			avl_height; /* Height of the AVL tree rooted at "avl_root" */
	struct lv_val_struct	*base_lv;   /* back pointer to base var lv_val (subscript depth 0) */
	lvTreeNode		*avl_root;  /* pointer to the root of the AVL tree corresponding to this subscript level;
					     * overloaded as the free list pointer if this "lvTree" structure is in the free list.
					     */
	lvTreeNode		*sbs_parent;/* pointer to parent (points to lv_val if sbs_depth==1 and lvTreeNode if sbs_depth>1) */
	treeSrchStatus		lastLookup; /* clue to last node lookup in the AVL tree rooted at "avl_root" */
} lvTree;

/* This section defines macros for the AVL tree implementation. Note that an AVL tree maintains its log(n) height by ensuring
 * the left and right subtrees at any level never differ in height by more than 1.
 */

#define MAX_BALANCE	1 /* the maximum difference in height of the left and right subtrees of a tree,
			   * balance > MAX_BALANCE will trigger a re-balance */

#define	TREE_DESCEND_LEFT	0
#define	TREE_DESCEND_RIGHT	1

/* AVL tree algorithms talk about balance factor as being -1 (left heavy), 1 (right heavy)  or 0 (balanced)
 * but we use 0, 1 and 2 instead for this implementation.
 */
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

/* Macro to get numeric valued subscript in an lv node */
#define	LV_NUM_NODE_GET_KEY(NODE, KEY)								\
{												\
	lvTreeNodeNum	*lcl_flt_node;								\
	int		lcl_mvtype;								\
												\
	lcl_flt_node = (lvTreeNodeNum *)NODE;							\
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

/* Macro to get string valued subscript in an lv node */
#define	LV_STR_NODE_GET_KEY(NODE, KEY)			\
{							\
	assert(MVTYPE_IS_STRING(NODE->key_mvtype));	\
	(KEY)->mvtype = NODE->key_mvtype;		\
	(KEY)->str.len = NODE->key_len;			\
	(KEY)->str.addr = NODE->key_addr;		\
}

/* If NODE contains a numeric key, before returning the key's mval to non-lv code make sure we
 * reset any bit(s) that are set only in lv code. Non-lv code does not know to handle this bit.
 * The only one at this point in time is MV_CANONICAL.
 */
#define	LV_NODE_GET_KEY(NODE, KEY_MVAL)				\
{								\
	if (TREE_KEY_SUBSCR_IS_CANONICAL(NODE->key_mvtype))	\
	{	/* "NODE" is of type "lvTreeNodeNum *" */	\
		LV_NUM_NODE_GET_KEY(NODE, KEY_MVAL);		\
		(KEY_MVAL)->mvtype &= MV_CANONICAL_OFF;		\
	} else	/* "NODE" is of type "lvTreeNode *" */		\
		LV_STR_NODE_GET_KEY(NODE, KEY_MVAL);		\
}

/* Macro to return if a given "lvTreeNode *" pointer is a null subscript string.
 * Input "node" could actually be any one of "lvTreeNodeNum *" or "lvTreeNode *".
 * A null subscript string is of actual type "lvTreeNode *". To minimize the checks, we check for the MV_STR bit in the mvtype.
 * This is asserted below. For "lvTreeNodeNum *", the MV_STR bit is guaranteed not to be set. That leaves us with "lvTreeNode *".
 */
#define	LV_NODE_KEY_IS_STRING(NODE)	(DBG_ASSERT(NULL != NODE)					\
					MVTYPE_IS_STRING(NODE->key_mvtype))

#define	LV_NODE_KEY_IS_NULL_SUBS(NODE)	(LV_NODE_KEY_IS_STRING(NODE) && (0 == NODE->key_len))

#ifdef DEBUG
int 		lvAvlTreeKeySubscrCmp(treeKeySubscr *aSubscr, lvTreeNode *bNode);
int 		lvAvlTreeNodeSubscrCmp(lvTreeNode *aNode, lvTreeNode *bNode);
#endif
lvTreeNode	*lvAvlTreeFirst(lvTree *lvt);
lvTreeNode	*lvAvlTreeLast(lvTree *lvt);
lvTreeNode 	*lvAvlTreePrev(lvTreeNode *node);
lvTreeNode 	*lvAvlTreeKeyPrev(lvTree *lvt, treeKeySubscr *key);
lvTreeNode 	*lvAvlTreeNext(lvTreeNode *node);
lvTreeNode 	*lvAvlTreeKeyNext(lvTree *lvt, treeKeySubscr *key);
lvTreeNode	*lvAvlTreeFirstPostOrder(lvTree *lvt);
lvTreeNode	*lvAvlTreeNextPostOrder(lvTreeNode *node);
lvTreeNode	*lvAvlTreeKeyCollatedNext(lvTree *lvt, treeKeySubscr *key);
lvTreeNode	*lvAvlTreeNodeCollatedNext(lvTreeNode *node);
lvTreeNode	*lvAvlTreeCloneSubTree(lvTreeNode *node, lvTree *lvt, lvTreeNode *avl_parent);

#ifdef DEBUG
boolean_t	lvTreeIsWellFormed(lvTree *lvt);
void		assert_tree_member_offsets(void);
#endif

lvTreeNode	*lvAvlTreeLookupInt(lvTree *lvt, treeKeySubscr *key, lvTreeNode **lookupNode);
lvTreeNode	*lvAvlTreeLookupNum(lvTree *lvt, treeKeySubscr *key, lvTreeNode **lookupNode);
lvTreeNode	*lvAvlTreeLookupStr(lvTree *lvt, treeKeySubscr *key, lvTreeNode **lookupNode);
lvTreeNode	*lvAvlTreeLookup(lvTree *lvt, treeKeySubscr *key, lvTreeNode **lookupNode);
lvTreeNode	*lvAvlTreeNodeInsert(lvTree *lvt, treeKeySubscr *key, lvTreeNode *parent);
void		lvAvlTreeNodeDelete(lvTree *lvt, lvTreeNode *node);

/* The following LV_TREE_* macros are not defined as functions for performance reasons (to avoid overhead of parameter passing
 * and C stack push and pop)
 */
#define	LV_TREE_CREATE(NEWTREE, SBS_PARENT, SBS_DEPTH, BASE_LV)								\
{															\
	DEBUG_ONLY(assert_tree_member_offsets());									\
	NEWTREE = lvtree_getslot(LV_GET_SYMVAL(base_lv));								\
	/* Note: the fields are initialized below in the order in which they are defined in the "lvTree" structure */	\
	NEWTREE->ident = MV_LV_TREE;											\
	assert(0 < SBS_DEPTH);												\
	NEWTREE->sbs_depth = SBS_DEPTH;											\
	NEWTREE->avl_height = 0;											\
	NEWTREE->base_lv = BASE_LV;											\
	NEWTREE->avl_root = NULL;											\
	NEWTREE->sbs_parent = SBS_PARENT; /* note: LVT_PARENT macro not used as otherwise one would wonder why		\
					   * all members of "lvTree" structure except "sbs_parent" are initialized.	\
					   */										\
	(SBS_PARENT)->sbs_child = NEWTREE;										\
	NEWTREE->lastLookup.lastNodeLookedUp = NULL;									\
}

#define	LV_TREE_NODE_DELETE(LVT, NODE)								\
{												\
	assert(NULL != LVT);									\
	lvAvlTreeNodeDelete(LVT, NODE);								\
	/* Now that "NODE" has been removed from the AVL tree, it is safe to free the slot */	\
	LVTREENODE_FREESLOT(NODE);								\
	TREE_DEBUG_ONLY(assert(lvTreeIsWellFormed(LVT));)					\
}

#define	LV_TREE_CLONE(LVT, SBS_PARENT, BASE_LV)										\
{															\
        lvTree		*cloneTree;											\
	lvTreeNode	*avl_root;											\
															\
	cloneTree = lvtree_getslot(LV_GET_SYMVAL(BASE_LV));								\
	/* The following is optimized to do the initialization of just the needed structure members.			\
	 * For that it assumes a particular "lvTree" structure layout. The assumed layout is asserted			\
	 * so any changes to the layout will automatically alert us (by an assert failure) here and			\
	 * cause the below initialization to be accordingly reworked.							\
	 */														\
	assert(8 == OFFSETOF(lvTree, base_lv));										\
	assert(OFFSETOF(lvTree, base_lv) + SIZEOF(LVT->base_lv) == OFFSETOF(lvTree, avl_root));				\
	assert(OFFSETOF(lvTree, avl_root) + SIZEOF(LVT->avl_root) == OFFSETOF(lvTree, sbs_parent));			\
	assert(OFFSETOF(lvTree, sbs_parent) + SIZEOF(LVT->sbs_parent) == OFFSETOF(lvTree, lastLookup));			\
	assert(OFFSETOF(lvTree, lastLookup) + SIZEOF(LVT->lastLookup) == SIZEOF(lvTree));				\
	/* Directly copy the first 3 fields */										\
	memcpy(cloneTree, (LVT), OFFSETOF(lvTree, avl_height) + SIZEOF(LVT->avl_height));				\
	cloneTree->base_lv = BASE_LV;											\
	cloneTree->sbs_parent = SBS_PARENT;	/* see comment in LV_TREE_CREATE macro (against sbs_parent		\
						 * initialization) for why LVT_PARENT macro is not used */		\
	(SBS_PARENT)->sbs_child = cloneTree;										\
	/* reset clue in cloned tree as source tree pointers are no longer relevant in cloned tree */			\
	cloneTree->lastLookup.lastNodeLookedUp = NULL;									\
	if (NULL != (avl_root = (LVT)->avl_root))									\
        	cloneTree->avl_root = lvAvlTreeCloneSubTree(avl_root, cloneTree, NULL);					\
	else														\
        	cloneTree->avl_root = NULL;										\
}

#ifdef TREE_DEBUG
#        define TREE_DEBUG1(p)			{printf(p);             FFLUSH(stdout);}
#        define TREE_DEBUG2(p, q)		{printf(p, q);          FFLUSH(stdout);}
#        define TREE_DEBUG3(p, q, r)		{printf(p, q, r);       FFLUSH(stdout);}
#        define TREE_DEBUG4(p, q, r, s)		{printf(p, q, r, s);    FFLUSH(stdout);}
#        define TREE_DEBUG5(p, q, r, s, t)	{printf(p, q, r, s, t); FFLUSH(stdout);}
#        define	TREE_DEBUG_ONLY(X)			X
#else
#        define TREE_DEBUG1(p)
#        define TREE_DEBUG2(p, q)
#        define TREE_DEBUG3(p, q, r)
#        define TREE_DEBUG4(p, q, r, s)
#        define TREE_DEBUG5(p, q, r, s, t)
#        define	TREE_DEBUG_ONLY(X)
#endif

#endif
