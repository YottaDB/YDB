/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <ssdef.h>	/* for SS$_NORMAL */
#include <jpidef.h>
#include <psldef.h>	/* for PSL$C_USER */
#include <prtdef.h>	/* for PRT$C_NA   */
#include <efndef.h>

#include "gdsroot.h"
#include "ccp.h"
#include "vmsdtype.h"
#include "mem_list.h"
#include "gtm_logicals.h"

GBLREF mem_list *mem_list_head;
GBLREF uint4	process_id;
GBLREF uint4	gtm_memory_noaccess_defined;	/* count of the number of GTM_MEMORY_NOACCESS_ADDR logicals which are defined */
GBLREF uint4	gtm_memory_noaccess[GTM_MEMORY_NOACCESS_COUNT];	/* see VMS gtm_env_init_sp.c */

OS_PAGE_SIZE_DECLARE

error_def(ERR_SHRMEMEXHAUSTED);

/* forward declarations */

mem_list *coalesce_prev(mem_list *ml_ptr);
void coalesce_next(mem_list *ml_ptr);
mem_list *check_avail(int4 req_pages);

/*-----------------------------------------------------------------------
  coalesce this block with the previous block if it is free and adjacent
  to the current block's address space.
  Return the pointer to the previous link, if it got coalesced
  ----------------------------------------------------------------------*/

mem_list *coalesce_prev(mem_list *ml_ptr)
{
	mem_list *ml_prev = ml_ptr->prev;

	assert(ml_ptr->free);
	if (NULL != ml_prev && ml_prev->free &&
		(ml_prev->addr == ml_ptr->addr - (ml_prev->pages * OS_PAGELET_SIZE)))
	{
		ml_prev->next = ml_ptr->next;
		if (ml_prev->next)
			ml_prev->next->prev = ml_prev;
		ml_prev->pages += ml_ptr->pages;
		free(ml_ptr);
		return ml_prev;
	}
	return ml_ptr;
}

/*-----------------------------------------------------------------------
  coalesce next block with this block if it is free and adjacent to the
  current block's address space.
  ----------------------------------------------------------------------*/

void coalesce_next(mem_list *ml_ptr)
{
	mem_list *ml_next = ml_ptr->next;
	if (NULL != ml_next && ml_next->free &&
		(ml_next->addr == ml_ptr->addr + (ml_ptr->pages * OS_PAGELET_SIZE)))
	{
		ml_ptr->next = ml_next->next;
		if(ml_ptr->next)
			ml_ptr->next->prev = ml_ptr;
		ml_ptr->pages += ml_next->pages;
		free(ml_next);
	}
}

/*-----------------------------------------------------------------------
  add a link at the end of doubly linked list,
  ----------------------------------------------------------------------*/

void add_link(int4 size, int4 inadr)
{
	mem_list *ml_ptr, *ml_prev;

        for (ml_prev = NULL, ml_ptr = mem_list_head;
             ml_ptr != NULL;
             ml_prev = ml_ptr, ml_ptr = ml_ptr->next)
		;

	ml_ptr = (mem_list *)malloc(SIZEOF(mem_list));
	ml_ptr->prev = ml_prev;
	ml_ptr->next = NULL;
	if (mem_list_head)
		ml_prev->next = ml_ptr;
	else
		mem_list_head = ml_ptr;
	ml_ptr->pages = size;
	ml_ptr->free = FALSE;
	ml_ptr->addr = inadr;

}

/*-----------------------------------------------------------------------
  check mem_list, to see if an exact sized chunk is available.
  Return pointer to the appropriate link if space is available, or NULL
  ----------------------------------------------------------------------*/

mem_list *check_avail(int4 req_pages)
{
	mem_list *ml_ptr;

        for (ml_ptr = mem_list_head; ml_ptr != NULL; ml_ptr = ml_ptr->next)
	{
                if (ml_ptr->free  &&  ml_ptr->pages == req_pages)
                        break;
	}
	return ml_ptr;
}

/*-----------------------------------------------------------------------
  check the state of a particular chunk of va. If the required chunk is
  in the list and it is free, it returns TRUE and FALSE otherwise.
  ----------------------------------------------------------------------*/

boolean_t is_va_free(uint4 outaddrs)
{
	mem_list *ml_ptr;

        for (ml_ptr = mem_list_head; ml_ptr; ml_ptr = ml_ptr->next)
                if (outaddrs == ml_ptr->addr)
                        break;
	return(NULL != ml_ptr && TRUE == ml_ptr->free);
}

/*-----------------------------------------------------------------------
  expand the address space. If an exactly enough free chunk is already available
  from previous activities, reuse it,
  ----------------------------------------------------------------------*/

uint4 gtm_expreg(uint4 size, uint4 *inadr, uint4 acmode, uint4 region)
{
	uint4		status;
	mem_list	*ml_ptr;

	ml_ptr = check_avail(size);
	if (ml_ptr == NULL)     /* not enough free space found or the list is empty */
        {
		status = gtm_expreg_noaccess_check(size, inadr, acmode, region);
		if (status == SS$_NORMAL)
			add_link(size, inadr[0]);
		else if ((SS$_ILLPAGCNT == status) && ((signed int)size > 0))
			status = ERR_SHRMEMEXHAUSTED;
		return status;
	}else
	{
		assert (size == ml_ptr->pages);
		ml_ptr->free = FALSE;
                inadr[0] = ml_ptr->addr;
                inadr[1] = ml_ptr->addr + ml_ptr->pages * OS_PAGELET_SIZE - 1;
        }
	return SS$_NORMAL;
}

uint4 gtm_expreg_noaccess_check(uint4 size, uint4 *inadr, uint4 acmode, uint4 region)
{
	uint4	status, count, retadr[2];
	DEBUG_ONLY(static uint4	numiters = 0;)

	do
	{	/* allocate memory using sys$expreg, but after that check if the memory range falls within the
		 * noaccess memory specified by gtm_memory_noaccess. if so, we need to do extra processing.
		 */
		status = sys$expreg(size, inadr, acmode, region);
		if ((SS$_NORMAL != status) || !gtm_memory_noaccess_defined)
			return status;
		/* check if allocated memory range intersects with the noaccess range */
		for (count = 0; count < gtm_memory_noaccess_defined; count++)
		{
			if ((inadr[0] <= gtm_memory_noaccess[count]) && (inadr[1] > gtm_memory_noaccess[count]))
				break;	/* found an intersecting memory address */
		}
		if (count == gtm_memory_noaccess_defined)	/* could not find any intersections */
			return status;				/* return right away */
		/* free the memory allocated just now using sys$deltva and allocate memory specifically at the noaccess
		 * address using sys$cretva, set its protection to be no-access using sys$setprt and then redo the sys$expreg.
		 * this way we will create a small inaccessible hole in the virtual address space. if some function tries
		 * to access this address we will ACCVIO and hopefully find the culprit causing the corruption.
		 */
		status = sys$deltva(inadr, retadr, PSL$C_USER);
		assert(SS$_NORMAL == status);
		/* now create virtual pages at fixed address using sys$cretva
		 *
		 * there are two system calls that allocate memory for a process' virtual address space in VMS.
		 * one is SYS$EXPREG and another is SYS$CRETVA. The former satisfies a request for n-bytes at an
		 * arbitrary address while the latter does it at a fixed address. It is the former that is used
		 * throughout GT.M code (for e.g. attaching to database shared memory). The latter is not advisable
		 * unless there is a real reason. in this case we want to protect a particular memory location
		 * from any access to identify if anything is trying to read/write to that location. hence the cretva.
		 *
		 * the virtual address space of a process in VMS has 4 parts, P0, P1, P2, P3. The last two are
		 * system space and not user accessible. P0 is the user's heap and P1 is the user's stack space.
		 * Given a total of 4Gb of addressible space (2**32), P0 and P1 are each 1Gb. Any memory allocation
		 * occurs in the P0 region. sys$expreg maintains a notion of the current end of the P0 region's
		 * virtual address space. all requests for memory are given from the current end and the current end
		 * is updated accordingly. any frees (using sys$deltva) that occur in the middle of the allocated
		 * space do not decrease the current end. only freeup of memory just before the current end also
		 * update the current end. for example, let us say current end is 0. if two allocations of 4M each
		 * is done, the current end becomes 8M. If the first allocation is freed, the current end stays at 8M.
		 * but when the second allocation is freed, the current end is taken back to 0 (not 4M). this is
		 * because the free logic will coalesce as many of the free address space as possible and decrease
		 * the current end by that amount.
		 *
		 * some problems with doing sys$cretva at a specific address is that sys$expreg will consider
		 * this to be the current end of the process virtual address space. all future sys$expreg()
		 * calls will get virtual addresses higher than the return from the sys$cretva call. it is quite
		 * possible that we will get a virtual-address-space-full (VASFULL) error when we would not have got
		 * it if we had not done the sys$cretva. This is because although the sys$cretva creates only one
		 * inaccessible OS_PAGE_SIZE hole, the virtual address space between the current end (before the sys$cretva)
		 * and the specific address (that we allocated using sys$cretva) is effectively a hole because of the
		 * inherent limitation of sys$expreg call (because of its simple approach of maintaining only the current
		 * end instead of some fancy data structure) to not recognize that space as usable anymore.
		 */
		inadr[0] = gtm_memory_noaccess[count] & ~(OS_PAGE_SIZE - 1);
		inadr[1] = inadr[0] + OS_PAGE_SIZE - 1;
		status = sys$cretva(inadr, NULL, PSL$C_USER);
		assert(SS$_NORMAL == status);
		/* GUARD the above created page for no access using sys$setprt */
		status = sys$setprt(inadr, NULL, (uint4)PSL$C_USER, (uint4)PRT$C_NA, NULL);
		/* at the maximum, we might need to do one iteration for each noaccess memory address. there are at most
		 * gtm_memory_noaccess_defined such addresses. ensure we do not perform more than that number of iterations.
		 */
		DEBUG_ONLY(numiters++;)
		assert(gtm_memory_noaccess_defined >= numiters);
	} while (TRUE);
}

/*-----------------------------------------------------------------------
   Delete the chunk if it is at the end of virtual address space.
   While doing so, coalesce with any other previous adjacent free blocks
   of address space.
   If the given address space is not at the end of the address space,
   just mark it free and do NOT coalesce with any other previous/next
   adjacent free blocks, since we might get into trouble setting protection
   on reuse of coalesced blocks.
  ----------------------------------------------------------------------*/

uint4 gtm_deltva(uint4 *outaddrs, uint4 *retadr, uint4 acmode)
{
	mem_list *ml_ptr, *ml_ptr_new;
	uint4 next_free_va, status;
        unsigned short retlen;
        unsigned short iosb[4];
        struct
        {
                item_list_3     item;
                int4            terminator;
        } item_list;

        for (ml_ptr = mem_list_head; ml_ptr; ml_ptr = ml_ptr->next)
                if (outaddrs[0] == ml_ptr->addr)
                        break;

	/* assert(ml_ptr);
		 some regions may be deleted using gtm_deltva() that are
                 not allocated by gtm_expreg(), so may not be present in the
                 list. For example, in mu_cre_file(), we allocate using
                 sys$crmpsc() which wont make into this list but deleted
                 using gtm_deltva()  */

	if (NULL == ml_ptr)	/* "not in list" case, delete it */
	{
#ifdef DEBUG
	/* To catch the callers supplying incorrect arguments */
        for (ml_ptr = mem_list_head; ml_ptr; ml_ptr = ml_ptr->next)
                if ((outaddrs[0] - OS_PAGE_SIZE) == ml_ptr->addr)
                        break;
	if (ml_ptr)
		assert(FALSE);
#endif
		status = sys$deltva(outaddrs, retadr, acmode);
		return status;
	}

	assert (FALSE == ml_ptr->free);

	if (ml_ptr->next == NULL)
	{
		/* check if the chunk is at the end of va, so we can delete it */
                item_list.item.buffer_length         = 4;
                item_list.item.item_code             = JPI$_FREP0VA;
                item_list.item.buffer_address        = &next_free_va;
                item_list.item.return_length_address = &retlen;
		item_list.terminator 		     = 0;
		status = sys$getjpiw(EFN$C_ENF, &process_id, NULL, &item_list, iosb, NULL, 0);

		if (SS$_NORMAL == status && next_free_va == outaddrs[1] + 1)
		{
			ml_ptr->free = TRUE;
			for (;;)		/* coalesce all the adjacent free blocks */
			{
				ml_ptr_new = coalesce_prev(ml_ptr);
				if (ml_ptr == ml_ptr_new)
					break;
				else
					ml_ptr = ml_ptr_new;
			}
			outaddrs[0] = ml_ptr_new->addr;

			if (ml_ptr_new->prev == NULL)
				mem_list_head = NULL;
			else
				ml_ptr_new->prev->next = NULL;
			free(ml_ptr_new);

			status = sys$deltva(outaddrs, retadr, acmode);
			return status;
		}
	}
	ml_ptr->free = TRUE;
	return SS$_NORMAL;
}
