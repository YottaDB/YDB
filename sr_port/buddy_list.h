/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __BUDDY_LIST_H__
#define __BUDDY_LIST_H__

/* This is a general purpose structure used for storing a list of similar-sized structure-elements with the need for
 *		efficiently accessing the element once we know its index.
 *	It maintains an array of pointers "ptrArray".
 *	The first and second pointers i.e. ptrArray[0] and ptrArray[1] point to an array containing "initAlloc" elements.
 *	Every other pointer ptrArray[i] points to an array containing double-the-number-of-elements
 *		as the array pointed to by ptrArray[i-1].
 *	This also provides for iterating through the list of elements.
 */

typedef struct	buddy_list_struct
{
	char	**ptrArray;     /* the array of pointers */
	int4	elemSize;	/* the size of each structure-element */
	int4	initAlloc;	/* the number of structures in the ptrArray[0] and ptrArray[1] */
	int4	initAllocBits;	/* number of bits corresponding to initAlloc */
	int4	nElems;		/* the current number of elements used up in the total array */
	int4	cumulMaxElems;  /* the maximum number of elements that can be held from the first upto the current array */
	char	**ptrArrayCurr;	/* = &ptrArray[i] where i is the current array index in use for allocation */
	char	*nextFreePtr;	/* pointer to the next free element in the current array. Initially = ptrArray[0] */
	char	*free_que;	/* pointer to the singly linked list of freed-up elements */
#	ifdef DEBUG
	/* The following two debug-only variables are present to avoid mixing of usage of the functions "free_last_n_elements"
	 * and "get_new_free_element"/"free_element()" in the same buddy list. This is because they both use completely
	 * different schemes to free up elements (one reduces the # of elements in the buddy list whereas the other does not
	 * touch the # of elements but maintains an internal singly linked list of freed up elements) and they cannot coexist.
	 */
	boolean_t	used_free_last_n_elements;	/* TRUE if "free_last_n_elements" was called in this buddy_list */
	boolean_t	used_free_element;		/* TRUE if "get_new_free_element" or "free_element" was called */
#	endif
} buddy_list;

void		initialize_list(buddy_list *list, int4 elemSize, int4 initAlloc);
char		*get_new_element(buddy_list *list, int4 nElements);
char		*find_element(buddy_list *list, int4 index);
void		cleanup_list(buddy_list *list);
void		reinitialize_list(buddy_list *list);	/* used for reusing already allocated storage */
boolean_t	free_last_n_elements(buddy_list *list, int4 num);	/* to free up the last contiguous "num" elements */
void		free_element(buddy_list *list, char *elem);	/* to free up an element and reuse it later */
char		*get_new_free_element(buddy_list *list);	/* gets a freed-up element if available otherwise gets a new one */

#define	CAREFUL_FREEUP_BUDDY_LIST(list)		\
{						\
	if (NULL != list)			\
		FREEUP_BUDDY_LIST(list);	\
}

#define	FREEUP_BUDDY_LIST(list)			\
{						\
	assert(list);				\
	if (NULL != list)			\
	{					\
		cleanup_list(list);		\
		free(list);			\
	}					\
}

#define	VERIFY_LIST_IS_REINITIALIZED(list)					\
{	/* The following code verifies the same fields initialized by the	\
	 * function "reinitialize_list". Any changes to one should be reflected	\
	 * in the other.							\
	 */									\
	assert((0 == list->nElems) || process_exiting);				\
	assert((list->cumulMaxElems == list->initAlloc) || process_exiting);	\
	assert((list->ptrArrayCurr == list->ptrArray) || process_exiting);	\
	assert((list->nextFreePtr == list->ptrArray[0])|| process_exiting);	\
	assert((NULL == list->free_que) || process_exiting);			\
}

#define	REINITIALIZE_LIST(LST)						\
{									\
	buddy_list	*lcllist;					\
									\
	lcllist = LST;							\
	assert((NULL == lcllist->free_que) || lcllist->nElems);		\
	if (lcllist->nElems)						\
		reinitialize_list(lcllist);				\
	else								\
	{	/* No need to reinitialize. Verify			\
		 * that list is already initialized */			\
		VERIFY_LIST_IS_REINITIALIZED(lcllist);			\
	}								\
}

#endif
