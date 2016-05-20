/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __DBGBLDIR_H__
#define __DBGBLDIR_H__

typedef struct gvt_container_struct
{
	gv_namehead			**gvt_ptr;	/* pointer to a location (either the "gvnh_reg_t->gvt" (for globals that
							 * dont span regions) OR "gvnh_spanreg_t->gvt_array[]" (for globals that
							 * span regions) that contains a pointer to the "gv_target" and that
							 * needs to be updated if/when the gv_target gets re-allocated.
							 */
	gv_namehead			**gvt_ptr2;	/* only for spanning globals, this points to a SECOND location where the
							 * gvt corresponding to the region (that maps the unsubscripted global
							 * reference) is stored (i.e. gvnh_reg_t->gvt). And that needs to be
							 * updated as well if/when the gv_target gets re-allocated.
							 */
	gd_region			*gd_reg;	/* region corresponding to the gv_target that is waiting for reg-open */
	struct gvt_container_struct	*next_gvtc;
} gvt_container;

boolean_t	get_first_gdr_name(gd_addr *current_gd_header, mstr *log_nam);
gd_addr		*zgbldir(mval *v);
gd_addr		*zgbldir_name_lookup_only(mval *v);
gd_addr		*gd_load(mstr *v);
gd_addr		*get_next_gdr(gd_addr *prev);
mstr		*get_name(mstr *ms);
void		cm_add_gdr_ptr(gd_region *greg);
void		cm_del_gdr_ptr(gd_region *greg);
void		*open_gd_file(mstr *v);
void		gd_rundown(void);
void 		gd_ht_kill(struct hash_table_mname_struct *table, boolean_t contents);

#endif
