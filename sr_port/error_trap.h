/****************************************************************
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ERROR_TRAP_H
#define ERROR_TRAP_H

#define IS_ETRAP	(err_act == &dollar_etrap.str)

void dollar_ecode_build(int level, mval *v);
boolean_t ecode_check(void);
void error_return(void);
#ifdef VMS
void error_return_vms(void);
#endif
void error_ret_code(void);
void op_zg1(int4 level);
void set_ecode(int errnum);

/* linked list for $ECODE */

typedef struct ecode_list_struct	/* contents of a list entry */
{
	int				level;		/* the Do/Execute-level indicator */
	mstr				str;		/* the value added at the current level */
	struct ecode_list_struct	*previous;	/* linked list pointer */
} ecode_list;

#endif /* ERROR_TRAP_H */
