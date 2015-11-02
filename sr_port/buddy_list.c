/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
        list->ptrArray = (char **)malloc(sizeof(char *) * (MAX_MEM_SIZE_IN_BITS + 2));
							/* 1 for holding the NULL pointer and 1 for ptrArray[0] */
        memset(list->ptrArray, 0, sizeof(char *) * (MAX_MEM_SIZE_IN_BITS + 2));
        list->ptrArrayCurr = list->ptrArray;
        list->nextFreePtr = list->ptrArray[0] = (char *)malloc(initAlloc * elemSize);
        list->itrptrArrayCurr = 0;      /* null-initialise the iteration fields */
        list->itrnElems = 0;
        list->itrcumulMaxElems = 0;
        list->itrnextFreePtr = 0;
	list->free_que = (buddy_que_head *)malloc(sizeof(buddy_que_head));
	list->free_que->fl = list->free_que->bl = 0;
}

void	reinitialize_list(buddy_list *list)
{
	assert(list);
	list->nElems = 0;
	list->cumulMaxElems = list->initAlloc;
	list->ptrArrayCurr = list->ptrArray;
	list->nextFreePtr = list->ptrArray[0];
        list->itrptrArrayCurr = 0;      /* null-initialise the iteration fields */
        list->itrnElems = 0;
        list->itrcumulMaxElems = 0;
        list->itrnextFreePtr = 0;
	list->free_que->fl = list->free_que->bl = 0;	/* reset the list to have no free element queue */
}

boolean_t	free_last_n_elements(buddy_list *list, int4 num)
{
	int4	rowElemsMax, rowElemsLeft, numLeft;
	char	**ptrArrayCurr;

	/* don't mix the usage of this routine with the usage of get_new_free_element() or free_element() */

	assert(list);
	if (list->nElems >= num)
	{
		ptrArrayCurr = list->ptrArrayCurr;
		numLeft = num;
		rowElemsLeft = (int4)(list->nextFreePtr - ptrArrayCurr[0]) / list->elemSize;
		rowElemsMax = list->nElems - rowElemsLeft;
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
		list->nElems -= num;
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
	int4	allocElems;

	if (0 >= nElements)
	{
		assert(FALSE);
		return NULL;
	}
        if (list->nElems + nElements <= list->cumulMaxElems)
        {
                retPtr = list->nextFreePtr;
                list->nextFreePtr += nElements * list->elemSize;
                list->nElems += nElements;
        }
        else
        {
		while (list->nElems + nElements > list->cumulMaxElems)
		{
			allocElems = list->cumulMaxElems;
			ptrArrayCurr = ++list->ptrArrayCurr;
			if (!(retPtr = *ptrArrayCurr))
				retPtr = *ptrArrayCurr = (char *)malloc(allocElems * list->elemSize);
			list->nElems = list->cumulMaxElems;
			list->cumulMaxElems += allocElems;
		}
                list->nElems += nElements;
                list->nextFreePtr = retPtr + list->elemSize * nElements;
        }
        return retPtr;
}

char	*get_new_free_element(buddy_list *list)
{
	char	*elem;

	elem = (char *)remqh((que_ent_ptr_t)list->free_que);
	if (elem)
		return elem;
	return get_new_element(list, 1);
}

void	free_element(buddy_list *list, char *elem)
{
	assert(elem);
	/* assumes that elem has a "que_head" structure as the first member */
	insqt((que_ent_ptr_t)elem, (que_ent_ptr_t)list->free_que);
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

char    *find_first_element(buddy_list *list, int4 index)
{
        /* index is provided as a future enhancement. Currently it is assumed to be 0 */

	assert(0 == index);
        list->itrptrArrayCurr = list->ptrArray;
        list->itrnElems = 0;
        list->itrcumulMaxElems = list->initAlloc;
        list->itrnextFreePtr = *list->ptrArray;
        return list->itrnextFreePtr;
}

char    *find_next_element(buddy_list *list, int4 nElements)
{
        /* nElements is provided as a future enhancement. Currently it is assumed to be 1 */

        assert(1 == nElements);
        if (++list->itrnElems >= list->nElems)
	{
		assert(list->itrnElems == list->nElems);
                return NULL;
	}
        if (list->itrnElems < list->itrcumulMaxElems)
                list->itrnextFreePtr += list->elemSize;
        else
        {
                list->itrcumulMaxElems = list->itrcumulMaxElems << 1;
		if (list->itrcumulMaxElems > list->nElems)
			list->itrcumulMaxElems = list->nElems;
                list->itrnextFreePtr = *++list->itrptrArrayCurr;
        }
        return list->itrnextFreePtr;
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
	free(list->free_que);
        free(list->ptrArray);
}
