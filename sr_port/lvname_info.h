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

/* lvname_info.h
 * ------------
 * Following structure has the format of variable arguments passed to op_putindx
 */
#ifndef MERGE_LOCAL_DEFINED
typedef struct lvname_info_struct
{
	intszofptr_t		total_lv_subs; /* Total subscripts + 1 for name itself */
	struct lv_val_struct	*start_lvp;
	mval 			*lv_subs[MAX_LVSUBSCRIPTS];
	struct lv_val_struct	*end_lvp;
} lvname_info;
typedef lvname_info	*lvname_info_ptr;

/* Function Prototypes for local variables functions of merge */
boolean_t 	lcl_arg1_is_desc_of_arg2(lv_val *cur, lv_val *ref);
unsigned char	*format_key_mvals(unsigned char *buff, int size, lvname_info *lvnp);
unsigned char   *format_key_lv_val(lv_val *lvpin, unsigned char *buff, int size);
#define MERGE_LOCAL_DEFINED
#endif
