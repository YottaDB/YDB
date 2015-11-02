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

#include "mdef.h"

#include "gtm_string.h"

#include "buddy_list.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "relqop.h"

#define MAX_MEM_SIZE_IN_BITS     63     /* the unreachable maximum size of memory (in bits) */

void    initialize_list(buddy_list *list, int4 elemSize, int4 initAlloc)
{
        int4    i, maxPtrs;
        int4    temp;

        if (!initAlloc)
                initAlloc = 1;

	assert(0 < elemSize);
	assert(0 < initAlloc);

	elemSize = ROUND_UP2(elemSize, 8) ;	/* make it 8 byte aligned */
	temp = initAlloc;
        while (temp & (temp - 1))     		/* floor it to a perfect power of 2 */
                temp = temp & (temp - 1);
	if (initAlloc != temp)
		initAlloc = temp << 1;
	for (i = 0; initAlloc >> i; i++);

	list->initAllocBits = i - 1;
        list->elemSize = elemSize;
        list->initAlloc = initAlloc;
        list->cumulMaxElems = initAlloc;
        list->nElems = 0;
	/* Note: size_t typecast done below to enable 64-bit arithmetic so sizes > 4GB can be allocated */
        list->ptrArray = (char **)malloc((size_t)SIZEOF(char *) * (MAX_MEM_SIZE_IN_BITS + 2));
						/* +2 = +1 for holding the NULL pointer and +1 for ptrArray[0] */
        memset(list->ptrArray, 0, SIZEOF(char *) * (MAX_MEM_SIZE_IN_BITS + 2));
        list->ptrArrayCurr = list->ptrArray;
	/* Note: size_t typecast done below to enable 64-bit arithmetic so sizes > 4GB can be allocated */
        list->nextFreePtr = list->ptrArray[0] = (char *)malloc((size_t)initAlloc * elemSize);
	list->free_que = NULL; /* initialize the list to have no free element queue */
	DEBUG_ONLY(list->used_free_last_n_elements = FALSE;)
	DEBUG_ONLY(list->used_free_element = FALSE;)
}

/* Any changes to this routine need corresponding changes to the VERIFY_LIST_IS_REINITIALIZED macro (defined in buddy_list.h) */
void	reinitialize_list(buddy_list *list)
{
	assert(list);
	list->nElems = 0;
	list->cumulMaxElems = list->initAlloc;
	list->ptrArrayCurr = list->ptrArray;
	list->nextFreePtr = list->ptrArray[0];
	list->free_que = NULL; /* reset the list to have no free element queue */
	DEBUG_ONLY(list->used_free_last_n_elements = FALSE;)
	DEBUG_ONLY(list->used_free_element = FALSE;)
}

boolean_t	free_last_n_elements(buddy_list *list, int4 num)
{
	int4	rowElemsMax, rowElemsLeft, numLeft, nElems;
	char	**ptrArrayCurr;

	assert(list);
	assert(!list->used_free_element);
	DEBUG_ONLY(list->used_free_last_n_elements = TRUE;)
	nElems = list->nElems;
	if (nElems >= num)
	{
		ptrArrayCurr = list->ptrArrayCurr;
		numLeft = num;
		rowElemsLeft = (int4)(list->nextFreePtr - ptrArrayCurr[0]) / list->elemSize;
		rowElemsMax = nElems - rowElemsLeft;
		while (numLeft >= rowElemsLeft  &&  ptrArrayCurr != list->ptrArray)
		{
			assert(0 == rowElemsMax % list->initAlloc);
			numLeft -= rowElemsLeft;
			list->cumulMaxElems -= rowElemsMax;
			if (rowElemsMax != list->initAlloc)
				rowElemsMax = rowElemsMax >> 1;
			rowElemsLeft = rowElemsMax;
			ptrArrayCurr--;
			assert(rowElemsMax >= list->initAlloc);
		}
		list->nElems = nElems - num;
		list->ptrArrayCurr = ptrArrayCurr;
		list->nextFreePtr = ptrArrayCurr[0] + list->elemSize * (rowElemsLeft - numLeft);
		assert(list->ptrArrayCurr >= list->ptrArray);
		return TRUE;
	} else
		return FALSE;
}

char    *get_new_element(buddy_list *list, int4 nElements)
{
        char    *retPtr;
        char    **ptrArrayCurr;
	int4	cumulMaxElems, nElems, elemSize;

	if (0 >= nElements)
	{
		assert(FALSE);
		return NULL;
	}
	nElems = list->nElems;
	cumulMaxElems = list->cumulMaxElems;
	elemSize = list->elemSize;
        if (nElems + nElements <= cumulMaxElems)
        {
                retPtr = list->nextFreePtr;
                list->nextFreePtr += nElements * elemSize;
                list->nElems = nElems + nElements;
        } else
        {
		do
		{
			ptrArrayCurr = ++list->ptrArrayCurr;
			/* Note: size_t typecast done below to enable 64-bit arithmetic so sizes > 4GB can be allocated */
			if (!(retPtr = *ptrArrayCurr))
				retPtr = *ptrArrayCurr = (char *)malloc((size_t)cumulMaxElems * elemSize);
			nElems = cumulMaxElems;
			cumulMaxElems *= 2;
		} while (nElems + nElements > cumulMaxElems);
                list->nElems = nElems + nElements;
                list->nextFreePtr = retPtr + elemSize * nElements;
		list->cumulMaxElems = cumulMaxElems;
        }
        return retPtr;
}

char	*get_new_free_element(buddy_list *list)
{
	char	*elem;

	assert(!list->used_free_last_n_elements);
	DEBUG_ONLY(list->used_free_element = TRUE;)
	/* Assert that each element has enough space to store a pointer. This will be used to maintain the singly linked list
	 * of freed up elements in the buddy list. The head of this list will be list->free_que.
	 */
	assert(SIZEOF(char *) <= list->elemSize);
	elem = list->free_que;
	if (NULL != elem)
	{
		list->free_que = *(char **)elem;
		assert(elem != list->free_que);
		return elem;
	}
	return get_new_element(list, 1);
}

void	free_element(buddy_list *list, char *elem)
{
	assert(!list->used_free_last_n_elements);
	DEBUG_ONLY(list->used_free_element = TRUE;)
	assert(elem);
	assert(elem != list->free_que);
	/* Add it to the singly linked list of freed up elements in the buddy_list */
	*(char **)elem = list->free_que;
	list->free_que = elem;
}

char    *find_element(buddy_list *list, int4 index)
{
        char    **ptrArrayCurr;
        int4    i, initAllocBits;

        if (index > list->nElems)
                return NULL;
	initAllocBits = list->initAllocBits;
	for (i = initAllocBits; index >> i; i++);
	i = i - initAllocBits;
	return list->ptrArray[i] + list->elemSize * (index - list->initAlloc * (i ? 1 << (i-1) : 0));
}

void    cleanup_list(buddy_list *list)
{
        char    **curr;

	assert(list);
	if (!list ||  !(curr = list->ptrArray))
		return;
        while(*curr)
	{
                free(*curr);
                curr++;
	}
        free(list->ptrArray);
}
