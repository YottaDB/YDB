/****************************************************************
 *                                                              *
 *      Copyright 2003 Sanchez Computer Associates, Inc.        *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#ifndef GTM_MAXSTR_INCLUDED
#define GTM_MAXSTR_INCLUDED

/* The following macros should be used whenever an automatic string buffer
 * of size MAX_STRLEN needs to be declared in a function. Given that MAX_STRLEN
 * is enhanced to 1MB, allocating 1MB of local buffer on stack is not viable and
 * may lead to C stack overflows. The following macros employ an adaptive
 * scheme where an inital buffer of size 32K is allocated on stack and whenever
 * the buffer is found to be insufficient, the algorithm reallocates the buffer in
 * malloc'd space by doubling the previous size.
 *
 * If the string fits within 32K, there is [almost] no additional penalty since
 * the buffer is on stack as before.
 *
 * For any future requirement of MAX_STRLEN automatic buffers, the following macros
 * should be used. An example below:
 *
 * func()
 * {
 * 	char	buffer[MAX_STRLEN];
 * 	mstr	src, dst;
 *
 * 	...
 * 	dst.addr = &buffer[0];
 * 	memcpy(dst.addr, src.addr, len)
 * 	...
 *
 * 	return
 * }
 *
 * should be replaced something like
 *
 * #include "gtm_maxstr.h"
 * func()
 * {
 * 	MAXSTR_BUFF_DECL(buffer);
 * 	mstr	src, dst;
 *
 * 	MAXSTR_BUFF_INIT;
 * 	...
 * 	...
 * 	dst.addr = &buffer[0];
 * 	MAXSTR_BUFF_ALLOC(src.len, &dst.addr, 0);
 * 	...
 * 	...
 * 	MAXSTR_BUFF_FINI;
 * 	return;
 * }
 *
 */

/* The maximum nested depth of MAXSTR_BUFF_INIT/MAXSTR_BUFF_FINI allowed. We do not expect
 * many nesting levels. When the buffer stack overflows, GT.M asserts. */
#define	MAXSTR_STACK_SIZE	10

GBLREF mstr	maxstr_buff[];		/* Buffer stack for nested MAXSTR_BUFF_INIT/MAXSTR_BUFF_FINI */
/* Each entry in the buffer stack where each entry points to the buffer and it's size. Note that
 * although mstr is chosen as the entry type, it does not represent a GT.M string in the
 * traditional sense. The addr field point to the malloc'd buffer and is NULL if no
 * reallocation occured, i.e. buffer lies on the stack. The len field stores the current
 * buffer size which can grow geometrically.
 */
GBLREF int	maxstr_stack_level;	/* Current (0-index based) depth of nested MAXSTR_BUFF_INIT/MAXSTR_BUFF_FINI */

#define MAXSTR_BUFF_DECL(var)	char var[MAX_STRBUFF_INIT];

#define MAXSTR_BUFF_INIT 					\
{								\
	ESTABLISH(gtm_maxstr_ch);				\
	maxstr_stack_level++;					\
	assert(maxstr_stack_level < MAXSTR_STACK_SIZE);		\
	maxstr_buff[maxstr_stack_level].len = MAX_STRBUFF_INIT;	\
	maxstr_buff[maxstr_stack_level].addr = NULL;		\
}

#define MAXSTR_BUFF_INIT_RET 					\
{								\
	ESTABLISH_RET(gtm_maxstr_ch, -1);			\
	maxstr_stack_level++;					\
	assert(maxstr_stack_level < MAXSTR_STACK_SIZE);		\
	maxstr_buff[maxstr_stack_level].len = MAX_STRBUFF_INIT;	\
	maxstr_buff[maxstr_stack_level].addr = NULL;		\
}

/* The following macro checks whether the existing available buffer is sufficient
 * and if not, it reallocates the buffer to the sufficient size.
 * space_needed - buffer space needed to accommodate the string that is about to be written.
 * buff - pointer to the beginning of the buffer. If reallocation occurs, buff will be
 * 	modified to point to the reallocated buffer.
 * space_occupied - how full is the buffer?
 * returns - size of the allocated buffer (whether reallocated or not).
 */
#define MAXSTR_BUFF_ALLOC(space_needed, buff, space_occupied)	\
	gtm_maxstr_alloc((space_needed), &(buff), (space_occupied))

#define MAXSTR_BUFF_FINI 					\
{								\
	if (maxstr_buff[maxstr_stack_level].addr)		\
	{							\
		free(maxstr_buff[maxstr_stack_level].addr);	\
		maxstr_buff[maxstr_stack_level].addr = NULL;	\
	}							\
	maxstr_buff[maxstr_stack_level].len = 0;		\
	maxstr_stack_level--;					\
	REVERT; /* gtm_maxstr_ch() */				\
}

int gtm_maxstr_alloc(int space_needed, char** buff, int space_occupied);

#endif /* GTM_MAXSTR_INCLUDED */
