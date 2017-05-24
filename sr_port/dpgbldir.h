/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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

GBLREF	mstr			extnam_str;
GBLREF	mval			dollar_zgbldir;

#define GET_CURR_GLD_NAME(GLDNAME)					\
MBSTART {								\
	GLDNAME = extnam_str.len ? &extnam_str : &dollar_zgbldir.str;	\
} MBEND

/* Define constants for a dummy gld file */
#define	IMPOS_GBLNAME_7			"\xFF\xFF\xFF\xFF\xFF\xFF\xFF"	/* 7-bytes of 0xFF */
#define	IMPOS_GBLNAME_8			IMPOS_GBLNAME_7 "\xFF"		/* 8-bytes of 0xFF */
#define	IMPOSSIBLE_GBLNAME_31		IMPOS_GBLNAME_8 IMPOS_GBLNAME_8 IMPOS_GBLNAME_8 IMPOS_GBLNAME_7
#define	DUMMY_GBLDIR_N_REGS		2		/* one for "DEFAULT" region and one for "default" (statsdb region) */
#define	DUMMY_GBLDIR_N_MAPS		5		/* one each for local locks "#)", "%", "%Y", "%Z" and 0xFFFFFF... */
#define DUMMY_GBLDIR_FIRST_MAP		"#)"		/* local locks */
#define DUMMY_GBLDIR_SECOND_MAP		"%"		/* start of valid global name */
#define DUMMY_GBLDIR_THIRD_MAP		"%Y"		/* map for non-%Y global name */
#define DUMMY_GBLDIR_FOURTH_MAP		"%Z"		/* map for %Y global name */
#define DUMMY_GBLDIR_FIFTH_MAP		IMPOSSIBLE_GBLNAME_31 /* last map always corresponds to impossible global name */
#define	DUMMY_GBLDIR_MAP_GVN_SIZE(KEY)	(SIZEOF(KEY)-1)	/* SIZEOF already counts the null byte in the literal so remove it */
#define	DUMMY_GBLDIR_MAP_KEY_SIZE(KEY)	(SIZEOF(KEY)+1)	/* +1 for second null byte (SIZEOF already counts the 1st null byte) */
#define	DUMMY_GBLDIR_VAR_MAP_SIZE	ROUND_UP2(DUMMY_GBLDIR_MAP_KEY_SIZE(DUMMY_GBLDIR_FIRST_MAP)		\
							+ DUMMY_GBLDIR_MAP_KEY_SIZE(DUMMY_GBLDIR_SECOND_MAP)	\
							+ DUMMY_GBLDIR_MAP_KEY_SIZE(DUMMY_GBLDIR_THIRD_MAP)	\
							+ DUMMY_GBLDIR_MAP_KEY_SIZE(DUMMY_GBLDIR_FOURTH_MAP)	\
							+ DUMMY_GBLDIR_MAP_KEY_SIZE(DUMMY_GBLDIR_FIFTH_MAP), 8)
#define	DUMMY_GBLDIR_FIX_MAP_SIZE	(DUMMY_GBLDIR_N_MAPS * SIZEOF(gd_binding))
#define	DUMMY_GBLDIR_TOT_MAP_SIZE	(DUMMY_GBLDIR_FIX_MAP_SIZE + DUMMY_GBLDIR_VAR_MAP_SIZE)
/* Note: The below definition of DUMMY_GBLDIR_SIZE does not include SIZEOF(header_struct) that is there in the actual gld file */
#define	DUMMY_GBLDIR_SIZE		(SIZEOF(gd_addr) + DUMMY_GBLDIR_TOT_MAP_SIZE			\
							 + DUMMY_GBLDIR_N_REGS * SIZEOF(gd_region)	\
							 + DUMMY_GBLDIR_N_REGS * SIZEOF(gd_segment))

#endif
