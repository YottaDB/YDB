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

typedef	struct
{
	int4	fl;
	int4	bl;		/* can use the que_head definition from relqueop.c, but gdsfhead.h has a separate one.	*/
} buddy_que_head;		/* therefore, need a separate que_head definition 					*/

typedef struct	buddy_list_struct
{
	char	**ptrArray;     /* the array of pointers */
	int4	elemSize;	/* the size of each structure-element */
	int4	maxElems;	/* the maximum number of elements that can be totally stored */
	int4	maxElemsBits;	/* 1 + number of bits associated with 2PowerCeil(maxElems) */
	int4	initAlloc;	/* the number of structures in the ptrArray[0] and ptrArray[1] */
	int4	initAllocBits;	/* number of bits corresponding to initAlloc */
	int4	nElems;		/* the current number of elements used up in the total array */
	int4	cumulMaxElems;  /* the maximum number of elements that can be held from the first upto the current array */
	char	**ptrArrayCurr;	/* = &ptrArray[i] where i is the current array index in use for allocation */
	char	*nextFreePtr;	/* pointer to the next free element in the current array. Initially = ptrArray[0] */
	int4	itrnElems;	/* fields starting with itr are similar to non-itr counterparts except used while iterating */
	int4	itrcumulMaxElems;
	char	**itrptrArrayCurr;
	char	*itrnextFreePtr;
	buddy_que_head	*free_que;	/* pointer to the list of freed-up elements */
} buddy_list;

/*
 * Note that to use the free_element() and get_new_free_element() functions, the structure that is used to
 * store the element should have a "que_head" member as the first field with "fl" and "bl" subfields
 * to maintain a linked list of free elements.
 *
 * Also don't mix usage of free_last_n_elements() and get_new_free_element() unless you know what you are doing.
 *
 */

void		initialize_list(buddy_list *list, int4 elemSize, int4 initAlloc);
char		*get_new_element(buddy_list *list, int4 nElements);
char		*find_element(buddy_list *list, int4 index);
char		*find_first_element(buddy_list *list, int4 index);
char		*find_next_element(buddy_list *list, int4 nElements);
void		cleanup_list(buddy_list *list);
void		reinitialize_list(buddy_list *list);	/* used for reusing already allocated storage */
boolean_t	free_last_n_elements(buddy_list *list, int4 num);	/* to free up the last contiguous "num" elements */
void		free_element(buddy_list *list, char *elem);	/* to free up an element and reuse it later */
char		*get_new_free_element(buddy_list *list);	/* gets a freed-up element if available otherwise gets a new one */

#define	FREEUP_BUDDY_LIST(list)			\
{						\
	void	cleanup_list();			\
						\
	cleanup_list(list);			\
	assert(list);				\
	free(list);				\
}

#endif
