/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#ifdef DEBUG
#include "gtm_stdio.h"
#endif

#include "rc.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "rc_nspace.h"
#include "iosp.h"
#include "hashdef.h"
#include "copy.h"
#include "rc_oflow.h"
#include "error.h"
#include "gdsblk.h"
#include "gtcm.h"
#include "omi.h"
#include "io.h"
#include "gvcst_init.h"
#include "gvcst_get.h"
#include "change_reg.h"
#include "mvalconv.h"
#include "trans_log_name.h"

/* TEMPORARY CONSTANT - minimum value of the database RC_RESERVED field */
#define RC_RESERVED	128

GBLREF gd_region	*gv_cur_region;
GBLDEF int		rc_server_id = RC_DEF_SERV_ID;
GBLDEF rc_dsid_list	*dsid_list=0;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_namehead	*gv_target;
GBLREF int4		gv_keysize;
GBLREF sgmnt_data	*cs_data;
GBLREF gd_addr		*gd_header;
GBLREF rc_oflow	*rc_overflow;

static rc_dsid_list	*fdi_ptr=0;
static int		rc_overflow_size=0;

static CONDITION_HANDLER(rc_fnd_file_ch1);
static CONDITION_HANDLER(rc_fnd_file_ch2);

short rc_fnd_file(rc_xdsid *xdsid)
{
    gv_namehead		*g;
    short		dsid, node;
    gd_binding		*map;
    char		 buff[1024], *cp, *cp1;
    mstr		 fpath1, fpath2;
    mval		 v;
    int			 i, keysize;
    bool		 got_it;
    int                  len, node2;

    GET_SHORT(dsid, &xdsid->dsid.value);
    GET_SHORT(node, &xdsid->node.value);

    if (!dsid_list) {
    /*	open special database, set up entry */
	dsid_list = (rc_dsid_list *)malloc(sizeof(rc_dsid_list));
	dsid_list->dsid = RC_NSPACE_DSID;
	dsid_list->next = NULL;
	fpath1.addr = RC_NSPACE_PATH;
	fpath1.len = sizeof(RC_NSPACE_PATH);
	if (trans_log_name(&fpath1, &fpath2, buff) != SS_NORMAL) {
	    char msg[256];
	    sprintf(msg,"Invalid DB filename, \"%s\"",fpath1.addr);
	    gtcm_rep_err(msg, errno);
	    return RC_BADFILESPEC;
	}
	if (fpath2.len > MAX_FN_LEN)
	    return RC_BADFILESPEC;
	dsid_list->fname = (char *)malloc(fpath2.len + 1);
	memcpy(dsid_list->fname, fpath2.addr, fpath2.len);
	*((char*)(dsid_list->fname + fpath2.len)) = 0;
	gv_cur_region = (gd_region *)malloc(sizeof(gd_region));
	memset(gv_cur_region, 0, sizeof(gd_region));
	gv_cur_region->dyn.addr = (gd_segment *)malloc(sizeof(gd_segment));
	memset(gv_cur_region->dyn.addr, 0, sizeof(gd_segment));
	memcpy(gv_cur_region->dyn.addr->fname, fpath2.addr, fpath2.len);
	gv_cur_region->dyn.addr->fname_len = fpath2.len;
	gv_cur_region->dyn.addr->acc_meth = dba_bg;
	ESTABLISH_RET(rc_fnd_file_ch1, RC_SUCCESS);
	gvcst_init(gv_cur_region);
	REVERT;
	change_reg();
	/* check to see if this DB has the reserved bytes field set
	 * correctly.  Global pages must always have some extra unused
	 * space left in them (RC_RESERVED bytes) so that the page
	 * will fit into the DataTree buffer when unpacked by the
	 * DataTree client.
	 */
	if (cs_data->reserved_bytes < RC_RESERVED)
	{
	    OMI_DBG((omi_debug,
		"Unable to access database file:  \"%s\"\nReserved_bytes field in the file header is too small for GT.CM\n",
		fpath2.addr));
	    free(dsid_list->fname);
	    dsid_list->fname=0;
	    free(dsid_list);
	    dsid_list=0;
	    free(gv_cur_region->dyn.addr);
	    gv_cur_region->dyn.addr=0;
	    free(gv_cur_region);
	    gv_cur_region=0;
	    return RC_FILEACCESS;
	}
	gv_keysize = (gv_cur_region->max_key_size + MAX_NUM_SUBSC_LEN + 4) & (-4);
	cs_addrs->dir_tree = (gv_namehead*)malloc(sizeof(gv_namehead) + 2*sizeof(gv_key) + 3*(gv_keysize - 1));
	gv_altkey = (gv_key*)malloc(sizeof(gv_key) - 1 + gv_keysize);
	gv_currkey = (gv_key*)malloc(sizeof(gv_key) - 1 + gv_keysize);
	gv_currkey->top = gv_altkey->top = gv_keysize;
	gv_currkey->end = gv_currkey->prev = gv_altkey->end = gv_altkey->prev = 0;
	g = cs_addrs->dir_tree;
	g->first_rec = (gv_key*)(g->clue.base + gv_keysize);
	g->last_rec = (gv_key*)(g->first_rec->base + gv_keysize);
	g->clue.top = g->last_rec->top = g->first_rec->top = gv_keysize;
	g->clue.prev = g->clue.end = 0;
	g->root = DIR_ROOT;
	dsid_list->gda = (gd_addr*)malloc(sizeof(gd_addr) + 3 * sizeof(gd_binding));
	dsid_list->gda->n_maps = 3;
	dsid_list->gda->n_regions = 1;
	dsid_list->gda->n_segments = 1;
	dsid_list->gda->maps = (gd_binding*)((char*)dsid_list->gda + sizeof(gd_addr));
	dsid_list->gda->max_rec_size = gv_cur_region->max_rec_size;
	map = dsid_list->gda->maps;
	map ++;
	memset(map->name,0,MAX_NM_LEN);
	map->name[0] = '%';
	map->reg.addr = gv_cur_region;
	map++;
	map->reg.addr = gv_cur_region;
	memset(map->name,-1,MAX_NM_LEN);
	dsid_list->gda->tab_ptr = (htab_desc *)malloc(sizeof(htab_desc));
	ht_init(dsid_list->gda->tab_ptr,0);
	change_reg();
	if (rc_overflow->top < cs_addrs->hdr->blk_size)
	{	if (rc_overflow->buff)
		    free(rc_overflow->buff);
		    rc_overflow->top = cs_addrs->hdr->blk_size;
		    rc_overflow->buff = (char*)malloc(rc_overflow->top);
		    if (rc_overflow_size < rc_overflow->top)
			rc_overflow_size = rc_overflow->top;
	}
    }
    for (fdi_ptr = dsid_list; fdi_ptr; fdi_ptr = fdi_ptr->next)
    	if (fdi_ptr->dsid == dsid)
	    break;
    if (!fdi_ptr) {
	/*	need to open new database, add to list, set fdi_ptr */
	gd_header = dsid_list->gda;
	gv_currkey->end = 0;
	v.mvtype = MV_STR;
	v.str.len = RC_NSPACE_GLOB_LEN-1;
	v.str.addr = RC_NSPACE_GLOB;
	gv_bind_name(gd_header, &v.str);
	if (!gv_target->root)	/* No namespace global */
	{	return RC_UNDEFNAMSPC;
	    }
	v.mvtype = MV_STR;
	v.str.len = sizeof(RC_NSPACE_DSI_SUB)-1;
	v.str.addr = RC_NSPACE_DSI_SUB;
	mval2subsc(&v,gv_currkey);

	node2 = node;
	MV_FORCE_MVAL(&v,node2);
	mval2subsc(&v,gv_currkey);
	i = dsid / 256;
	MV_FORCE_MVAL(&v,i);
	mval2subsc(&v,gv_currkey);
	if (!(got_it = gvcst_get(&v)))
	{	return RC_UNDEFNAMSPC;
	    }
	for (cp = v.str.addr, i = 1; i < RC_FILESPEC_PIECE; i++)
	    for (; *cp++ != RC_FILESPEC_DELIM; )
		;
	for (cp1 = cp; *cp1++ != RC_FILESPEC_DELIM; )
	    ;
	cp1--;

	len = (int) cp1 - (int) cp;
	if (len > MAX_FN_LEN)
	    return RC_BADFILESPEC;

	fdi_ptr = (rc_dsid_list *)malloc(sizeof(rc_dsid_list));
	fdi_ptr->fname = (char *)malloc(len+1);
	fdi_ptr->dsid = dsid;
	memcpy(fdi_ptr->fname, cp, len);
	*(fdi_ptr->fname + (len)) = 0;
	gv_cur_region = (gd_region *)malloc(sizeof(gd_region));
	memset(gv_cur_region, 0, sizeof(gd_region));
	gv_cur_region->dyn.addr = (gd_segment *)malloc(sizeof(gd_segment));
	memset(gv_cur_region->dyn.addr, 0, sizeof(gd_segment));
	memcpy(gv_cur_region->dyn.addr->fname, cp, len);
	gv_cur_region->dyn.addr->fname_len = len;
	gv_cur_region->dyn.addr->acc_meth = dba_bg;
	ESTABLISH_RET(rc_fnd_file_ch2, RC_SUCCESS);
	gvcst_init(gv_cur_region);
	REVERT;
	change_reg();
	/* check to see if this DB has the reserved bytes field set
	 * correctly.  Global pages must always have some extra unused
	 * space left in them (RC_RESERVED bytes) so that the page
	 * will fit into the DataTree buffer when unpacked by the
	 * DataTree client.
	 */
	if (cs_data->reserved_bytes < RC_RESERVED)
	{
	    OMI_DBG((omi_debug,
		"Unable to access database file:  \"%s\"\nReserved_bytes field in the file header is too small for GT.CM\n",
		fdi_ptr->fname));
	    free(dsid_list->fname);
	    dsid_list->fname=0;
	    free(dsid_list);
	    dsid_list=0;
	    free(gv_cur_region->dyn.addr);
	    gv_cur_region->dyn.addr=0;
	    free(gv_cur_region);
	    gv_cur_region=0;
	    return RC_FILEACCESS;
	}
	grab_crit(gv_cur_region);
	cs_data->rc_srv_cnt++;
	if (!cs_data->dsid)
	{
	    cs_data->dsid = dsid;
	    cs_data->rc_node = node;
	}
	else if (cs_data->dsid != dsid || cs_data->rc_node != node)
	{
	    cs_data->rc_srv_cnt--;
	    rel_crit(gv_cur_region);
	    OMI_DBG((omi_debug, "Dataset ID/RC node mismatch"));
	    OMI_DBG((omi_debug, "DB file:  \"%s\"\n", dsid_list->fname));
	    OMI_DBG((omi_debug, "Stored DSID:  %d\tRC Node:  %d\n",
		     cs_data->dsid, cs_data->rc_node));
	    OMI_DBG((omi_debug, "RC Rq DSID:  %d\tRC Node:  %d\n",
		     dsid,node));
	    free(fdi_ptr->fname);
	    fdi_ptr->fname=0;
	    free(fdi_ptr);
	    fdi_ptr=0;
	    free(gv_cur_region->dyn.addr);
	    gv_cur_region->dyn.addr=0;
	    free(gv_cur_region);
	    gv_cur_region=0;
	    return RC_FILEACCESS;
	}
	rel_crit(gv_cur_region);
	keysize = (gv_cur_region->max_key_size + MAX_NUM_SUBSC_LEN + 4) & (-4);
	cs_addrs->dir_tree = (gv_namehead*)malloc(sizeof(gv_namehead) + 2*sizeof(gv_key) + 3*(keysize - 1));
	if (keysize > gv_keysize) {
	    gv_keysize = keysize;;
	    free(gv_altkey);
	    free(gv_currkey);
	    gv_altkey = (gv_key*)malloc(sizeof(gv_key) - 1 + gv_keysize);
	    gv_currkey = (gv_key*)malloc(sizeof(gv_key) - 1 + gv_keysize);
	    gv_currkey->top = gv_altkey->top = gv_keysize;
	    gv_currkey->end = gv_currkey->prev = gv_altkey->end = gv_altkey->prev = 0;
	}
	g = cs_addrs->dir_tree;
	g->first_rec = (gv_key*)(g->clue.base + keysize);
	g->last_rec = (gv_key*)(g->first_rec->base + keysize);
	g->clue.top = g->last_rec->top = g->first_rec->top = keysize;
	g->clue.prev = g->clue.end = 0;
	g->root = DIR_ROOT;
	fdi_ptr->gda = (gd_addr*)malloc(sizeof(gd_addr) + 3 * sizeof(gd_binding));
	fdi_ptr->gda->n_maps = 3;
	fdi_ptr->gda->n_regions = 1;
	fdi_ptr->gda->n_segments = 1;
	fdi_ptr->gda->maps = (gd_binding*)((char*)fdi_ptr->gda + sizeof(gd_addr));
	fdi_ptr->gda->max_rec_size = gv_cur_region->max_rec_size;
	map = fdi_ptr->gda->maps;
	map ++;
	memset(map->name,0,MAX_NM_LEN);
	map->name[0] = '%';
	map->reg.addr = gv_cur_region;
	map++;
	map->reg.addr = gv_cur_region;
	memset(map->name,-1,MAX_NM_LEN);
	fdi_ptr->gda->tab_ptr = (htab_desc *)malloc(sizeof(htab_desc));
	ht_init(fdi_ptr->gda->tab_ptr,0);
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


/* clean up from gvcst_init() failure, when dsid_list was NULL (open first db) */
static CONDITION_HANDLER(rc_fnd_file_ch1)
{
    /* undo setup */
    START_CH
	free(dsid_list->fname);
    dsid_list=0;
    free(gv_cur_region->dyn.addr);
    gv_cur_region->dyn.addr=0;
    free(gv_cur_region);
    gv_cur_region=0;
    NEXTCH;
}

/* clean up from gvcst_init() failure, when dsid_list was non-NULL (open new db) */
static CONDITION_HANDLER(rc_fnd_file_ch2)
{
    /* undo setup */
    START_CH
	free(fdi_ptr->fname);
    fdi_ptr->fname=0;
    free(fdi_ptr);
    free(gv_cur_region->dyn.addr);
    gv_cur_region->dyn.addr=0;
    free(gv_cur_region);
    gv_cur_region=0;
    NEXTCH;
}
