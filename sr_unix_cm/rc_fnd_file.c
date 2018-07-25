/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "rc.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "rc_nspace.h"
#include "iosp.h"
#include "copy.h"
#include "rc_oflow.h"
#include "error.h"
#include "gdsblk.h"
#include "gtcm.h"
#include "omi.h"
#include "io.h"
#include "gvcst_protos.h"	/* for "gvcst_get", "gvcst_init" prototype */
#include "change_reg.h"
#include "mvalconv.h"
#include "ydb_trans_log_name.h"
#include "hashtab_mname.h"
#include "hashtab.h"
#include "gtmio.h"
#include "have_crit.h"

/* TEMPORARY CONSTANT - minimum value of the database RC_RESERVED field */
#define RC_RESERVED	128

GBLDEF int		rc_server_id = RC_DEF_SERV_ID;
GBLDEF rc_dsid_list	*dsid_list = NULL;

GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_key			*gv_altkey;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gv_namehead		*gv_target;
GBLREF	int4			gv_keysize;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_addr			*gd_header;
GBLREF	rc_oflow		*rc_overflow;

static rc_dsid_list	*fdi_ptr = 0;
static int		rc_overflow_size = 0;

static CONDITION_HANDLER(rc_fnd_file_ch1);
static CONDITION_HANDLER(rc_fnd_file_ch2);

LITREF	char	*ydbenvname[YDBENVINDX_MAX_INDEX];
LITREF	char	*gtmenvname[YDBENVINDX_MAX_INDEX];

short rc_fnd_file(rc_xdsid *xdsid)
{
	gv_namehead	*g;
	short		dsid, node;
	gd_binding	*map;
	gd_addr		*addr;
	char		buff[1024], *cp, *cp1;
	mstr		fpath2;
	mval		v;
	mname_entry	gvname;
	int		i, keysize;
	int             len, node2;
	gvnh_reg_t	*gvnh_reg;
	boolean_t	is_ydb_env_match;

	ASSERT_IS_LIBGTCM;
	GET_SHORT(dsid, &xdsid->dsid.value);
	GET_SHORT(node, &xdsid->node.value);
	if (!dsid_list)
	{
		/*	open special database, set up entry */
		dsid_list = (rc_dsid_list *)malloc(SIZEOF(rc_dsid_list));
		dsid_list->dsid = RC_NSPACE_DSID;
		dsid_list->next = NULL;
		if (SS_NORMAL != ydb_trans_log_name(YDBENVINDX_DTNDBD, &fpath2, buff, SIZEOF(buff),
								IGNORE_ERRORS_TRUE, &is_ydb_env_match))
		{
			char msg[256];
			SPRINTF(msg, "Invalid DB filename, \"%s\"", is_ydb_env_match ? ydbenvname[YDBENVINDX_EVENT_LOG_LIBPATH]
											: gtmenvname[YDBENVINDX_EVENT_LOG_LIBPATH]);
			gtcm_rep_err(msg, errno);
			return RC_BADFILESPEC;
		}
		if (fpath2.len > MAX_FN_LEN)
			return RC_BADFILESPEC;
		dsid_list->fname = (char *)malloc(fpath2.len + 1);
		memcpy(dsid_list->fname, fpath2.addr, fpath2.len);
		*((char*)(dsid_list->fname + fpath2.len)) = 0;
		gv_cur_region = (gd_region *)malloc(SIZEOF(gd_region));
		memset(gv_cur_region, 0, SIZEOF(gd_region));
		gv_cur_region->dyn.addr = (gd_segment *)malloc(SIZEOF(gd_segment));
		memset(gv_cur_region->dyn.addr, 0, SIZEOF(gd_segment));
		memcpy(gv_cur_region->dyn.addr->fname, fpath2.addr, fpath2.len);
		gv_cur_region->dyn.addr->fname_len = fpath2.len;
		REG_ACC_METH(gv_cur_region) = dba_bg;
		ESTABLISH_RET(rc_fnd_file_ch1, RC_SUCCESS);
		gvcst_init(gv_cur_region);
		REVERT;
		change_reg();
		/* check to see if this DB has the reserved bytes field set
		 * correctly.  Global pages must always have some extra unused
		 * space left in them (RC_RESERVED bytes) so that the page
		 * will fit into the client buffer when unpacked by the
		 * client.
		 */
		if (cs_data->reserved_bytes < RC_RESERVED)
		{
			OMI_DBG((omi_debug,
			"Unable to access database file:  \"%s\"\nReserved_bytes field in the file header is too small for GT.CM\n",
			fpath2.addr));
			free(dsid_list->fname);
			dsid_list->fname = NULL;
			free(dsid_list);
			dsid_list = NULL;
			free(gv_cur_region->dyn.addr);
			gv_cur_region->dyn.addr = NULL;
			free(gv_cur_region);
			gv_cur_region = NULL;
			return RC_FILEACCESS;
		}
		GVKEYSIZE_INIT_IF_NEEDED;	/* sets up "gv_keysize", "gv_currkey" and "gv_altkey" in sync if not already done */
		cs_addrs->dir_tree = (gv_namehead *)malloc(SIZEOF(gv_namehead) + 2 * SIZEOF(gv_key) + 3 * (gv_keysize - 1));
		g = cs_addrs->dir_tree;
		g->first_rec = (gv_key*)(g->clue.base + gv_keysize);
		g->last_rec = (gv_key*)(g->first_rec->base + gv_keysize);
		g->clue.top = g->last_rec->top = g->first_rec->top = gv_keysize;
		/* No need to initialize g->clue.prev as it is never used */
		g->clue.end = 0;
		g->root = DIR_ROOT;
		assertpro(FALSE);	/* Need to rework the below codepath if/when this is reached */
#		ifdef DISABLED_CODE
		addr = dsid_list->gda = (gd_addr *)malloc(SIZEOF(gd_addr) + DUMMY_GBLDIR_TOT_MAP_SIZE);
		memset(addr, 0, SIZEOF(gd_addr) + DUMMY_GBLDIR_TOT_MAP_SIZE);
		DUMMY_GLD_MAP_INIT(addr, RELATIVE_OFFSET_FALSE, gv_cur_region);
#		endif
		dsid_list->gda->max_rec_size = gv_cur_region->max_rec_size;
		dsid_list->gda->tab_ptr = (hash_table_mname *)malloc(SIZEOF(hash_table_mname));
		init_hashtab_mname(dsid_list->gda->tab_ptr, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
		change_reg();
		if (rc_overflow->top < cs_addrs->hdr->blk_size)
		{
			if (rc_overflow->buff)
				free(rc_overflow->buff);
			rc_overflow->top = cs_addrs->hdr->blk_size;
			rc_overflow->buff = (char*)malloc(rc_overflow->top);
			if (rc_overflow_size < rc_overflow->top)
				rc_overflow_size = rc_overflow->top;
		}
	}
	for (fdi_ptr = dsid_list; fdi_ptr && (fdi_ptr->dsid != dsid); fdi_ptr = fdi_ptr->next)
		;
	if (!fdi_ptr)
	{	/*	need to open new database, add to list, set fdi_ptr */
		gd_header = dsid_list->gda;
		gv_currkey->end = 0;
		gvname.var_name.len  = RC_NSPACE_GLOB_LEN - 1;
		gvname.var_name.addr = RC_NSPACE_GLOB;
		COMPUTE_HASH_MNAME(&gvname);
		GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, &gvname, gvnh_reg);
		assert(NULL == gvnh_reg->gvspan); /* so GV_BIND_SUBSNAME_IF_GVSPAN is not needed */
		if (!gv_target->root)	/* No namespace global */
			return RC_UNDEFNAMSPC;
		v.mvtype = MV_STR;
		v.str.len = SIZEOF(RC_NSPACE_DSI_SUB)-1;
		v.str.addr = RC_NSPACE_DSI_SUB;
		mval2subsc(&v, gv_currkey, gv_cur_region->std_null_coll);
		node2 = node;
		MV_FORCE_MVAL(&v,node2);
		mval2subsc(&v, gv_currkey, gv_cur_region->std_null_coll);
		i = dsid / 256;
		MV_FORCE_MVAL(&v,i);
		mval2subsc(&v, gv_currkey, gv_cur_region->std_null_coll);
		if (gvcst_get(&v))
			return RC_UNDEFNAMSPC;
		for (cp = v.str.addr, i = 1; i < RC_FILESPEC_PIECE; i++)
			for (; *cp++ != RC_FILESPEC_DELIM; )
				;
		for (cp1 = cp; *cp1++ != RC_FILESPEC_DELIM; )
			;
		cp1--;
		len = (int)(cp1 - cp);
		if (len > MAX_FN_LEN)
			return RC_BADFILESPEC;
		fdi_ptr = (rc_dsid_list *)malloc(SIZEOF(rc_dsid_list));
		fdi_ptr->fname = (char *)malloc(len+1);
		fdi_ptr->dsid = dsid;
		memcpy(fdi_ptr->fname, cp, len);
		*(fdi_ptr->fname + (len)) = 0;
		gv_cur_region = (gd_region *)malloc(SIZEOF(gd_region));
		memset(gv_cur_region, 0, SIZEOF(gd_region));
		gv_cur_region->dyn.addr = (gd_segment *)malloc(SIZEOF(gd_segment));
		memset(gv_cur_region->dyn.addr, 0, SIZEOF(gd_segment));
		memcpy(gv_cur_region->dyn.addr->fname, cp, len);
		gv_cur_region->dyn.addr->fname_len = len;
		REG_ACC_METH(gv_cur_region) = dba_bg;
		ESTABLISH_RET(rc_fnd_file_ch2, RC_SUCCESS);
		gvcst_init(gv_cur_region);
		REVERT;
		change_reg();
		/* check to see if this DB has the reserved bytes field set
		 * correctly.  Global pages must always have some extra unused
		 * space left in them (RC_RESERVED bytes) so that the page
		 * will fit into the client buffer when unpacked by the
		 * client.
		 */
		if (cs_data->reserved_bytes < RC_RESERVED)
		{
			OMI_DBG((omi_debug,
			"Unable to access database file:  \"%s\"\nReserved_bytes field in the file header is too small for GT.CM\n",
			fdi_ptr->fname));
			free(dsid_list->fname);
			dsid_list->fname = NULL;
			free(dsid_list);
			dsid_list = NULL;
			free(gv_cur_region->dyn.addr);
			gv_cur_region->dyn.addr = NULL;
			free(gv_cur_region);
			gv_cur_region = NULL;
			return RC_FILEACCESS;
		}
		assert(!cs_addrs->hold_onto_crit);	/* this ensures we can safely do unconditional grab_crit and rel_crit */
		grab_crit(gv_cur_region);
		cs_data->rc_srv_cnt++;
		if (!cs_data->dsid)
		{
			cs_data->dsid = dsid;
			cs_data->rc_node = node;
		} else if (cs_data->dsid != dsid || cs_data->rc_node != node)
		{
			cs_data->rc_srv_cnt--;
			rel_crit(gv_cur_region);
			OMI_DBG((omi_debug, "Dataset ID/RC node mismatch"));
			OMI_DBG((omi_debug, "DB file:  \"%s\"\n", dsid_list->fname));
			OMI_DBG((omi_debug, "Stored DSID:  %d\tRC Node:  %d\n", cs_data->dsid, cs_data->rc_node));
			OMI_DBG((omi_debug, "RC Rq DSID:  %d\tRC Node:  %d\n", dsid,node));
			free(fdi_ptr->fname);
			fdi_ptr->fname = NULL;
			free(fdi_ptr);
			fdi_ptr = NULL;
			free(gv_cur_region->dyn.addr);
			gv_cur_region->dyn.addr = NULL;
			free(gv_cur_region);
			gv_cur_region = NULL;
			return RC_FILEACCESS;
		}
		rel_crit(gv_cur_region);
		GVKEYSIZE_INIT_IF_NEEDED;
		keysize = DBKEYSIZE(gv_cur_region->max_key_size);
		cs_addrs->dir_tree = (gv_namehead *)malloc(SIZEOF(gv_namehead) + 2 * SIZEOF(gv_key) + 3 * (keysize - 1));
		g = cs_addrs->dir_tree;
		g->first_rec = (gv_key*)(g->clue.base + keysize);
		g->last_rec = (gv_key*)(g->first_rec->base + keysize);
		g->clue.top = g->last_rec->top = g->first_rec->top = keysize;
		/* No need to initialize g->clue.prev as it is not currently used */
		g->clue.end = 0;
		g->root = DIR_ROOT;
		assert(FALSE);	/* Need to rework the below codepath if/when this is reached */
#		ifdef DISABLED_CODE
		addr = fdi_ptr->gda = (gd_addr *)malloc(SIZEOF(gd_addr) + DUMMY_GBLDIR_TOT_MAP_SIZE);
		memset(addr, 0, SIZEOF(gd_addr) + DUMMY_GBLDIR_TOT_MAP_SIZE);
		DUMMY_GLD_MAP_INIT(addr, RELATIVE_OFFSET_FALSE, gv_cur_region);
#		endif
		fdi_ptr->gda->max_rec_size = gv_cur_region->max_rec_size;
		fdi_ptr->gda->tab_ptr = (hash_table_mname *)malloc(SIZEOF(hash_table_mname));
		init_hashtab_mname(fdi_ptr->gda->tab_ptr, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
		fdi_ptr->next = dsid_list->next;
		dsid_list->next = fdi_ptr;
	}
	gv_cur_region = fdi_ptr->gda->maps[1].reg.addr;
	change_reg();
	if (rc_overflow->top < cs_addrs->hdr->blk_size)
	{
		if (rc_overflow->buff)
			free(rc_overflow->buff);
		rc_overflow->top = cs_addrs->hdr->blk_size;
		rc_overflow->buff = (char*)malloc(rc_overflow->top);
		if (rc_overflow_size < rc_overflow->top)
			rc_overflow_size = rc_overflow->top;
	}
	if (!rc_overflow -> top)
	{
		rc_overflow -> top = rc_overflow_size;
		rc_overflow->buff = (char *)malloc(rc_overflow->top);
	}
	gd_header = fdi_ptr->gda;
	return RC_SUCCESS;
}

/* clean up from "gvcst_init" failure, when dsid_list was NULL (open first db) */
static CONDITION_HANDLER(rc_fnd_file_ch1)
{	/* undo setup */
	ASSERT_IS_LIBGTCM;
	START_CH(FALSE);
	free(dsid_list->fname);
	dsid_list = NULL;
	free(gv_cur_region->dyn.addr);
	gv_cur_region->dyn.addr = NULL;
	free(gv_cur_region);
	gv_cur_region = NULL;
	NEXTCH;
}

/* clean up from "gvcst_init" failure, when dsid_list was non-NULL (open new db) */
static CONDITION_HANDLER(rc_fnd_file_ch2)
{	/* undo setup */
	ASSERT_IS_LIBGTCM;
	START_CH(FALSE);
	free(fdi_ptr->fname);
	fdi_ptr->fname = NULL;
	free(fdi_ptr);
	free(gv_cur_region->dyn.addr);
	gv_cur_region->dyn.addr = NULL;
	free(gv_cur_region);
	gv_cur_region = NULL;
	NEXTCH;
}
